#include "../include/protocol.h"
#include "../include/auth.h"
#include "../include/database.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

// External database connection (từ main.c)
extern db_connection_t* db;

/**
 * Parse message từ buffer nhận được
 */
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out) {
    if (!buffer || !msg_out || buffer_len < 3) {
        return -1;
    }

    // Đọc type (1 byte)
    msg_out->type = buffer[0];

    // Đọc length (2 bytes, network byte order)
    uint16_t length_network;
    memcpy(&length_network, buffer + 1, 2);
    msg_out->length = ntohs(length_network);

    // Kiểm tra độ dài hợp lệ
    if (msg_out->length > buffer_len - 3) {
        fprintf(stderr, "Lỗi: Payload length (%d) vượt quá buffer còn lại (%zu)\n", 
                msg_out->length, buffer_len - 3);
        return -1;
    }

    // Cấp phát bộ nhớ cho payload
    if (msg_out->length > 0) {
        msg_out->payload = (uint8_t*)malloc(msg_out->length);
        if (!msg_out->payload) {
            fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho payload\n");
            return -1;
        }
        memcpy(msg_out->payload, buffer + 3, msg_out->length);
    } else {
        msg_out->payload = NULL;
    }

    return 0;
}

/**
 * Tạo message theo format protocol
 */
int protocol_create_message(uint8_t type, const uint8_t* payload, uint16_t payload_len, uint8_t* buffer_out) {
    if (!buffer_out) {
        return -1;
    }

    // Type (1 byte)
    buffer_out[0] = type;

    // Length (2 bytes, network byte order)
    uint16_t length_network = htons(payload_len);
    buffer_out[1] = (length_network >> 8) & 0xFF;
    buffer_out[2] = length_network & 0xFF;

    // Payload
    if (payload && payload_len > 0) {
        memcpy(buffer_out + 3, payload, payload_len);
    }

    return 3 + payload_len;
}

/**
 * Gửi message đến client
 */
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[BUFFER_SIZE];
    
    if (3 + payload_len > BUFFER_SIZE) {
        fprintf(stderr, "Lỗi: Message quá lớn (%d bytes)\n", 3 + payload_len);
        return -1;
    }

    int msg_len = protocol_create_message(type, payload, payload_len, buffer);
    if (msg_len < 0) {
        return -1;
    }

    ssize_t sent = send(client_fd, buffer, msg_len, 0);
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }

    if (sent != msg_len) {
        fprintf(stderr, "Cảnh báo: Chỉ gửi được %zd/%d bytes\n", sent, msg_len);
        return -1;
    }

    return 0;
}

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
 * Xử lý LOGIN_REQUEST
 */
static int protocol_handle_login(server_t* server, int client_index, const message_t* msg) {
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
static int protocol_handle_register(server_t* server, int client_index, const message_t* msg) {
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

/**
 * Xử lý message nhận được từ client
 */
int protocol_handle_message(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg) {
        return -1;
    }

    switch (msg->type) {
        case MSG_LOGIN_REQUEST:
            return protocol_handle_login(server, client_index, msg);
            
        case MSG_REGISTER_REQUEST:
            return protocol_handle_register(server, client_index, msg);
            
        case MSG_LOGOUT:
            // TODO: Implement logout handler
            printf("Nhận LOGOUT từ client %d\n", client_index);
            return 0;
            
        default:
            fprintf(stderr, "Unknown message type: 0x%02X từ client %d\n", 
                    msg->type, client_index);
            return -1;
    }
}

