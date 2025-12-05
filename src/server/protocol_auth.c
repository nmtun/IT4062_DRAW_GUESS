#include "../include/protocol.h"
#include "../include/auth.h"
#include "../include/database.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

// External database connection (từ main.c)
extern db_connection_t* db;

/**
 * Gửi LOGIN_RESPONSE
 */
int protocol_send_login_response(int client_fd, uint8_t status, int32_t user_id, const char* username) {
    login_response_t response;
    memset(&response, 0, sizeof(response));
    
    response.status = status;
    response.user_id = (int32_t)htonl((uint32_t)user_id);  // Convert to network byte order
    
    if (username) {
        strncpy(response.username, username, MAX_USERNAME_LEN - 1);
        response.username[MAX_USERNAME_LEN - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_LOGIN_RESPONSE, 
                                (uint8_t*)&response, sizeof(response));
}

/**
 * Gửi REGISTER_RESPONSE
 */
int protocol_send_register_response(int client_fd, uint8_t status, const char* message) {
    register_response_t response;
    memset(&response, 0, sizeof(response));
    
    response.status = status;
    
    if (message) {
        strncpy(response.message, message, sizeof(response.message) - 1);
        response.message[sizeof(response.message) - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_REGISTER_RESPONSE, 
                                (uint8_t*)&response, sizeof(response));
}

/**
 * Kiểm tra user đã đăng nhập và đang active không
 * @param server Con trỏ đến server_t
 * @param user_id User ID cần kiểm tra
 * @param exclude_client_index Client index cần loại trừ (thường là client hiện tại)
 * @return 1 nếu user đã đăng nhập và active, 0 nếu chưa
 */
static int protocol_is_user_already_logged_in(server_t* server, int user_id, int exclude_client_index) {
    if (!server || user_id <= 0) {
        return 0;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Bỏ qua client hiện tại
        if (i == exclude_client_index) {
            continue;
        }

        client_t* client = &server->clients[i];
        
        // Kiểm tra client đang active, đã đăng nhập với cùng user_id và state = LOGGED_IN
        if (client->active && 
            client->user_id == user_id && 
            client->state == CLIENT_STATE_LOGGED_IN) {
            return 1;  // User đã đăng nhập
        }
    }

    return 0;  // User chưa đăng nhập
}

/**
 * Xử lý LOGIN_REQUEST
 */
int protocol_handle_login(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(login_request_t)) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Parse payload
    login_request_t* req = (login_request_t*)msg->payload;
    
    // Đảm bảo null-terminated
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';

    printf("Nhận LOGIN_REQUEST từ client %d: username=%s\n", client_index, username);

    // Kiểm tra database connection
    if (!db) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Hash password
    char password_hash[65];
    if (auth_hash_password(password, password_hash) != 0) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Xác thực user
    int user_id = db_authenticate_user(db, username, password_hash);
    
    if (user_id > 0) {
        // Kiểm tra user đã đăng nhập và đang active chưa
        if (protocol_is_user_already_logged_in(server, user_id, client_index)) {
            // User đã đăng nhập ở client khác
            protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
            printf("Client %d đăng nhập thất bại: user_id=%d (username=%s) đã đăng nhập ở client khác\n", 
                   client_index, user_id, username);
            return -1;
        }

        // Đăng nhập thành công
        client->user_id = user_id;
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        client->state = CLIENT_STATE_LOGGED_IN;
        protocol_send_login_response(client->fd, STATUS_SUCCESS, user_id, username);
        printf("Client %d đăng nhập thành công: user_id=%d, username=%s\n", 
               client_index, user_id, username);
        return 0;
    } else {
        // Đăng nhập thất bại
        protocol_send_login_response(client->fd, STATUS_AUTH_FAILED, -1, "");
        printf("Client %d đăng nhập thất bại: username=%s\n", client_index, username);
        return -1;
    }
}

/**
 * Xử lý REGISTER_REQUEST
 */
int protocol_handle_register(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(register_request_t)) {
        protocol_send_register_response(client->fd, STATUS_ERROR, "Dữ liệu không hợp lệ");
        return -1;
    }

    // Parse payload
    register_request_t* req = (register_request_t*)msg->payload;
    
    // Đảm bảo null-terminated
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    char email[MAX_EMAIL_LEN];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';
    strncpy(email, req->email, MAX_EMAIL_LEN - 1);
    email[MAX_EMAIL_LEN - 1] = '\0';

    printf("Nhận REGISTER_REQUEST từ client %d: username=%s, email=%s\n", 
           client_index, username, email);

    // Validate username
    if (!auth_validate_username(username)) {
        protocol_send_register_response(client->fd, STATUS_INVALID_USERNAME, 
                                       "Username không hợp lệ");
        return -1;
    }

    // Validate password
    if (!auth_validate_password(password)) {
        protocol_send_register_response(client->fd, STATUS_INVALID_PASSWORD, 
                                       "Mật khẩu không hợp lệ");
        return -1;
    }

    // Kiểm tra database connection
    if (!db) {
        protocol_send_register_response(client->fd, STATUS_ERROR, 
                                       "Lỗi kết nối database");
        return -1;
    }

    // Hash password
    char password_hash[65];
    if (auth_hash_password(password, password_hash) != 0) {
        protocol_send_register_response(client->fd, STATUS_ERROR, 
                                       "Lỗi xử lý mật khẩu");
        return -1;
    }

    // Đăng ký user
    int user_id = db_register_user(db, username, password_hash);
    
    if (user_id > 0) {
        // Đăng ký thành công
        protocol_send_register_response(client->fd, STATUS_SUCCESS, 
                                       "Đăng ký thành công");
        printf("Client %d đăng ký thành công: user_id=%d, username=%s\n", 
               client_index, user_id, username);
        return 0;
    } else {
        // Đăng ký thất bại (username đã tồn tại hoặc lỗi khác)
        protocol_send_register_response(client->fd, STATUS_USER_EXISTS, 
                                       "Username đã tồn tại");
        printf("Client %d đăng ký thất bại: username=%s\n", client_index, username);
        return -1;
    }
}

