#include "../include/protocol.h"
#include "../include/auth.h"
#include "../include/database.h"
#include "../include/game.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

// External database connection (tu main.c)
extern db_connection_t* db;

// Forward declaration from protocol_game.c
extern int protocol_handle_round_timeout(server_t* server, room_t* room, const char* word_before_clear);

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
 * Gui thong bao tai khoan dang duoc dang nhap o noi khac
 */
int protocol_send_account_logged_in_elsewhere(int client_fd) {
    // Message khong co payload
    return protocol_send_message(client_fd, MSG_ACCOUNT_LOGGED_IN_ELSEWHERE, NULL, 0);
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
    char avatar[32];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';
    strncpy(avatar, req->avatar, sizeof(avatar) - 1);
    avatar[sizeof(avatar) - 1] = '\0';
    // Default avatar nếu không có
    if (avatar[0] == '\0') {
        strncpy(avatar, "avt1.jpg", sizeof(avatar) - 1);
        avatar[sizeof(avatar) - 1] = '\0';
    }

    printf("Nhan LOGIN_REQUEST tu client %d: username=%s, avatar=%s\n", client_index, username, avatar);

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
        int old_client_index = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (i == client_index) {
                continue;
            }
            client_t* old_client = &server->clients[i];
            if (old_client->active && 
                old_client->user_id == user_id && 
                old_client->state == CLIENT_STATE_LOGGED_IN) {
                old_client_index = i;
                break;
            }
        }
        
        if (old_client_index >= 0) {
            // User da dang nhap o client khac
            // Gui thong bao cho client cu truoc khi disconnect
            client_t* old_client = &server->clients[old_client_index];
            int send_result = protocol_send_account_logged_in_elsewhere(old_client->fd);
            if (send_result == 0) {
                printf("Gui thong bao account_logged_in_elsewhere den client %d (user_id=%d, username=%s)\n",
                       old_client_index, user_id, old_client->username);
                
                // Flush socket để đảm bảo message được gửi
                // shutdown(SHUT_WR) sẽ đảm bảo dữ liệu được flush trước khi đóng
                shutdown(old_client->fd, SHUT_WR);
                
                // Đợi một chút để đảm bảo message được gửi qua gateway
                // Sử dụng usleep (microseconds) - 100ms = 100000 microseconds
                usleep(100000);
            } else {
                printf("Loi khi gui thong bao account_logged_in_elsewhere den client %d\n", old_client_index);
            }
            
            // Ngat ket noi client cu (se tu dong cleanup room, etc.)
            // server_remove_client() sẽ đóng socket (đã shutdown ở trên)
            server_handle_disconnect(server, old_client_index);
            printf("Da ngat ket noi client %d (user_id=%d, username=%s) vi dang nhap o noi khac\n",
                   old_client_index, user_id, old_client->username);
        }

        // Dang nhap thanh cong cho client moi
        client->user_id = user_id;
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        strncpy(client->avatar, avatar, sizeof(client->avatar) - 1);
        client->avatar[sizeof(client->avatar) - 1] = '\0';
        client->state = CLIENT_STATE_LOGGED_IN;
        protocol_send_login_response(client->fd, STATUS_SUCCESS, user_id, username);
        printf("Client %d dang nhap thanh cong: user_id=%d, username=%s, avatar=%s\n", 
               client_index, user_id, username, avatar);
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
            // Neu dang choi va nguoi roi la drawer -> end round ngay de game khong bi ket
            int was_playing = (room->state == ROOM_PLAYING && room->game != NULL);
            int was_drawer = (was_playing && room->game->drawer_id == client->user_id);
            char word_before[64];
            word_before[0] = '\0';
            if (was_playing) {
                strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
                word_before[sizeof(word_before) - 1] = '\0';
            }
            
            int leaving_user_id = client->user_id;
            char leaving_username[32];
            strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
            leaving_username[sizeof(leaving_username) - 1] = '\0';

            // Xoa player khoi phong
            room_remove_player(room, leaving_user_id);

            // Kiểm tra số người chơi active sau khi rời phòng
            int active_count = 0;
            for (int i = 0; i < room->player_count; i++) {
                if (room->active_players[i] == 1) {
                    active_count++;
                }
            }
            
            if (room->player_count == 0 || active_count == 0) {
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
                // Đảm bảo owner là người chơi active
                if (!room_ensure_active_owner(room)) {
                    // Không có người chơi active nào, xóa phòng
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
            
            // Xu ly drawer roi phong trong game (sau khi da broadcast danh sach players)
            if (was_drawer && room->game && word_before[0] != '\0') {
                // End round hiện tại trước (để đảm bảo round được đếm đúng)
                game_end_round(room->game, false, -1);
                // Sau đó mới bắt đầu round mới
                protocol_handle_round_timeout(server, room, word_before);
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

