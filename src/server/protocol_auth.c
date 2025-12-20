#include "../include/protocol.h"
#include "../include/auth.h"
#include "../include/database.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

// External database connection (tu main.c)
extern db_connection_t* db;

/**
 * Gui LOGIN_RESPONSE
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
 * Gui REGISTER_RESPONSE
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
 * Kiem tra user da dang nhap va dang active khong
 * @param server Con tro den server_t
 * @param user_id User ID can kiem tra
 * @param exclude_client_index Client index can loai tru (thuong la client hien tai)
 * @return 1 neu user da dang nhap va active, 0 neu chua
 */
static int protocol_is_user_already_logged_in(server_t* server, int user_id, int exclude_client_index) {
    if (!server || user_id <= 0) {
        return 0;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Bo qua client hien tai
        if (i == exclude_client_index) {
            continue;
        }

        client_t* client = &server->clients[i];
        
        // Kiem tra client dang active, da dang nhap voi cung user_id va state = LOGGED_IN
        if (client->active && 
            client->user_id == user_id && 
            client->state == CLIENT_STATE_LOGGED_IN) {
            return 1;  // User da dang nhap
        }
    }

    return 0;  // User chua dang nhap
}

/**
 * Xu ly LOGIN_REQUEST
 */
int protocol_handle_login(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return -1;
    }

    // Kiem tra payload size
    if (msg->length < sizeof(login_request_t)) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Parse payload
    login_request_t* req = (login_request_t*)msg->payload;
    
    // Dam bao null-terminated
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';

    printf("Nhan LOGIN_REQUEST tu client %d: username=%s\n", client_index, username);

    // Kiem tra database connection
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

    // Xac thuc user
    int user_id = db_authenticate_user(db, username, password_hash);
    
    if (user_id > 0) {
        // Kiem tra user da dang nhap va dang active chua
        if (protocol_is_user_already_logged_in(server, user_id, client_index)) {
            // User da dang nhap o client khac
            protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
            printf("Client %d dang nhap that bai: user_id=%d (username=%s) da dang nhap o client khac\n", 
                   client_index, user_id, username);
            return -1;
        }

        // Dang nhap thanh cong
        client->user_id = user_id;
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        client->state = CLIENT_STATE_LOGGED_IN;
        protocol_send_login_response(client->fd, STATUS_SUCCESS, user_id, username);
        printf("Client %d dang nhap thanh cong: user_id=%d, username=%s\n", 
               client_index, user_id, username);
        return 0;
    } else {
        // Dang nhap that bai
        protocol_send_login_response(client->fd, STATUS_AUTH_FAILED, -1, "");
        printf("Client %d dang nhap that bai: username=%s\n", client_index, username);
        return -1;
    }
}

/**
 * Xu ly REGISTER_REQUEST
 */
int protocol_handle_register(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return -1;
    }

    // Kiem tra payload size
    if (msg->length < sizeof(register_request_t)) {
        protocol_send_register_response(client->fd, STATUS_ERROR, "Du lieu khong hop le");
        return -1;
    }

    // Parse payload
    register_request_t* req = (register_request_t*)msg->payload;
    
    // Dam bao null-terminated
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    char email[MAX_EMAIL_LEN];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';
    strncpy(email, req->email, MAX_EMAIL_LEN - 1);
    email[MAX_EMAIL_LEN - 1] = '\0';

    printf("Nhan REGISTER_REQUEST tu client %d: username=%s, email=%s\n", 
           client_index, username, email);

    // Validate username
    if (!auth_validate_username(username)) {
        protocol_send_register_response(client->fd, STATUS_INVALID_USERNAME, 
                                       "Username khong hop le");
        return -1;
    }

    // Validate password
    if (!auth_validate_password(password)) {
        protocol_send_register_response(client->fd, STATUS_INVALID_PASSWORD, 
                                       "Mat khau khong hop le");
        return -1;
    }

    // Kiem tra database connection
    if (!db) {
        protocol_send_register_response(client->fd, STATUS_ERROR, 
                                       "Loi ket noi database");
        return -1;
    }

    // Hash password
    char password_hash[65];
    if (auth_hash_password(password, password_hash) != 0) {
        protocol_send_register_response(client->fd, STATUS_ERROR, 
                                       "Loi xu ly mat khau");
        return -1;
    }

    // Dang ky user
    int user_id = db_register_user(db, username, password_hash);
    
    if (user_id > 0) {
        // Dang ky thanh cong
        protocol_send_register_response(client->fd, STATUS_SUCCESS, 
                                       "Dang ky thanh cong");
        printf("Client %d dang ky thanh cong: user_id=%d, username=%s\n", 
               client_index, user_id, username);
        return 0;
    } else {
        // Dang ky that bai (username da ton tai hoac loi khac)
        protocol_send_register_response(client->fd, STATUS_USER_EXISTS, 
                                       "Username da ton tai");
        printf("Client %d dang ky that bai: username=%s\n", client_index, username);
        return -1;
    }
}

/**
 * Xu ly LOGOUT
 * - Neu dang trong room -> leave room (giong disconnect nhung khong close socket)
 * - Reset client state ve LOGGED_OUT
 */
int protocol_handle_logout(server_t* server, int client_index, const message_t* msg) {
    (void)msg;
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) return -1;

    // Neu dang o trong phong thi remove khoi phong va broadcast update
    if (client->user_id > 0 &&
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {

        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            int leaving_user_id = client->user_id;
            char leaving_username[32];
            strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
            leaving_username[sizeof(leaving_username) - 1] = '\0';

            // Xoa player khoi phong
            room_remove_player(room, leaving_user_id);

            if (room->player_count == 0) {
                // Xoa room khoi server
                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (server->rooms[i] == room) {
                        server->rooms[i] = NULL;
                        server->room_count--;
                        break;
                    }
                }
                room_destroy(room);
                protocol_broadcast_room_list(server);
            } else {
                protocol_broadcast_room_players_update(server, room, 1, leaving_user_id, leaving_username, -1);
            }
        }
    }

    // Reset client state
    client->user_id = -1;
    client->username[0] = '\0';
    client->state = CLIENT_STATE_LOGGED_OUT;

    printf("Client %d logout thanh cong\n", client_index);
    return 0;
}

