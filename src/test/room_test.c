#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

static int sockfd = -1;
static bool is_logged_in = false;
static int current_user_id = -1;
static int current_room_id = -1;

// Thread control
static pthread_t receiver_thread;
static bool receiver_running = false;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool waiting_for_response = false;
static uint8_t expected_response_type = 0;

/**
 * Ket noi den server
 */
int connect_to_server(const char* ip, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() failed");
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(fd);
        return -1;
    }

    printf("✓ Da ket noi den server %s:%d\n", ip, port);
    return fd;
}

/**
 * Gui message den server
 */
int send_message(int fd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[1024];
    
    buffer[0] = type;
    uint16_t length_network = htons(payload_len);
    memcpy(buffer + 1, &length_network, 2);
    
    if (payload && payload_len > 0) {
        memcpy(buffer + 3, payload, payload_len);
    }
    
    int total_len = 3 + payload_len;
    ssize_t sent = send(fd, buffer, total_len, 0);
    
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }
    
    return 0;
}

/**
 * Xu ly mot message da nhan duoc
 * @return 1 neu la broadcast message (tiep tuc doi), 0 neu la response mong doi, -1 neu loi
 */
static int process_message(uint8_t* buffer, uint16_t length, uint8_t type, uint8_t* expected_type) {
    // Xu ly ROOM_LIST_RESPONSE nhu broadcast message (khi server tu dong gui)
    if (type == MSG_ROOM_LIST_RESPONSE) {
        // Neu dang doi response nay, xu ly nhu response binh thuong
        if (expected_type && *expected_type == MSG_ROOM_LIST_RESPONSE) {
            // Xu ly nhu response binh thuong (se xu ly o switch case ben duoi)
        } else {
            // Day la broadcast message, xu ly ngay
            if (length >= sizeof(room_list_response_t)) {
                room_list_response_t* header = (room_list_response_t*)(buffer + 3);
                uint16_t room_count = ntohs(header->room_count);
                
                printf("\n[ROOM_LIST_BROADCAST] Danh sach phong da duoc cap nhat (%d phong)\n", room_count);
                
                if (room_count > 0) {
                    room_info_protocol_t* rooms = (room_info_protocol_t*)(buffer + 3 + sizeof(room_list_response_t));
                    
                    printf("%-5s %-20s %-8s %-8s %-10s %-8s\n", 
                           "ID", "Ten phong", "So nguoi", "Toi da", "Trang thai", "Owner");
                    printf("------------------------------------------------------------\n");
                    
                    for (int i = 0; i < room_count; i++) {
                        int32_t room_id = (int32_t)ntohl((uint32_t)rooms[i].room_id);
                        uint8_t state = rooms[i].state;
                        const char* state_str = (state == 0) ? "WAITING" : 
                                               (state == 1) ? "PLAYING" : "FINISHED";
                        int32_t owner_id = (int32_t)ntohl((uint32_t)rooms[i].owner_id);
                        
                        printf("%-5d %-20s %-8d %-8d %-10s %-8d\n",
                               room_id, rooms[i].room_name, 
                               rooms[i].player_count, rooms[i].max_players,
                               state_str, owner_id);
                    }
                }
                printf("==========================================\n\n");
                return 1; // Da xu ly, tiep tuc doi
            }
        }
    }
    
    // Xu ly ROOM_UPDATE ngay lap tuc (broadcast message)
    if (type == MSG_ROOM_UPDATE) {
        if (length >= sizeof(room_info_protocol_t)) {
            room_info_protocol_t* room_info = (room_info_protocol_t*)(buffer + 3);
            int32_t room_id = (int32_t)ntohl((uint32_t)room_info->room_id);
            uint8_t state = room_info->state;
            const char* state_str = (state == 0) ? "WAITING" : 
                                   (state == 1) ? "PLAYING" : "FINISHED";
            
            printf("\n[ROOM_UPDATE] Phong %d: %s - %d/%d nguoi choi (%s)\n",
                   room_id, room_info->room_name, 
                   room_info->player_count, room_info->max_players, state_str);
            return 1; // Da xu ly, tiep tuc doi
        }
    }
    
    // Xu ly ROOM_PLAYERS_UPDATE (broadcast message voi danh sach day du)
    if (type == MSG_ROOM_PLAYERS_UPDATE) {
        if (length >= sizeof(room_players_update_t)) {
            room_players_update_t* update = (room_players_update_t*)(buffer + 3);
            int32_t room_id = (int32_t)ntohl((uint32_t)update->room_id);
            uint8_t action = update->action;
            int32_t changed_user_id = (int32_t)ntohl((uint32_t)update->changed_user_id);
            uint16_t player_count = ntohs(update->player_count);
            uint8_t state = update->state;
            int32_t owner_id = (int32_t)ntohl((uint32_t)update->owner_id);
            const char* state_str = (state == 0) ? "WAITING" : 
                                   (state == 1) ? "PLAYING" : "FINISHED";
            
            const char* action_str = (action == 0) ? "da tham gia" : "da roi";
            printf("\n[ROOM_PLAYERS_UPDATE] Phong %d (%s): %s (ID: %d) %s phong\n",
                   room_id, update->room_name, update->changed_username, changed_user_id, action_str);
            printf("  Trang thai: %s | So nguoi: %d/%d | Owner: %d\n",
                   state_str, player_count, update->max_players, owner_id);
            
            // Hien thi danh sach nguoi choi hien tai
            player_info_protocol_t* players = (player_info_protocol_t*)(buffer + 3 + sizeof(room_players_update_t));
            printf("Danh sach nguoi choi hien tai (%d nguoi):\n", player_count);
            for (int i = 0; i < player_count; i++) {
                int32_t pid = (int32_t)ntohl((uint32_t)players[i].user_id);
                const char* owner_mark = players[i].is_owner ? " (Owner)" : "";
                printf("  - %s (ID: %d)%s\n", players[i].username, pid, owner_mark);
            }
            printf("\n");
            return 1; // Da xu ly, tiep tuc doi
        }
    }
    
    // Kiem tra message type mong doi (chi khi dang doi response)
    if (expected_type && type != *expected_type) {
        printf("✗ Loi: Nhan message type 0x%02X, mong doi 0x%02X\n", type, *expected_type);
        return -1;
    }
    
    printf("✓ Nhan response: type=0x%02X, length=%d\n", type, length);
    
    // Parse response dua tren type
    switch (type) {
        case MSG_LOGIN_RESPONSE: {
            if (length >= sizeof(login_response_t)) {
                login_response_t* resp = (login_response_t*)(buffer + 3);
                uint8_t status = resp->status;
                int32_t user_id = (int32_t)ntohl((uint32_t)resp->user_id);
                
                if (status == STATUS_SUCCESS) {
                    printf("✓ Dang nhap thanh cong! user_id=%d, username=%s\n", 
                           user_id, resp->username);
                    current_user_id = user_id;
                    is_logged_in = true;
                    return 0;
                } else {
                    printf("✗ Dang nhap that bai (status=0x%02X)\n", status);
                    return -1;
                }
            }
            break;
        }
        
        case MSG_ROOM_LIST_RESPONSE: {
            if (length >= sizeof(room_list_response_t)) {
                room_list_response_t* header = (room_list_response_t*)(buffer + 3);
                uint16_t room_count = ntohs(header->room_count);
                
                printf("\n=== DANH SACH PHONG (%d phong) ===\n", room_count);
                
                if (room_count > 0) {
                    room_info_protocol_t* rooms = (room_info_protocol_t*)(buffer + 3 + sizeof(room_list_response_t));
                    
                    printf("%-5s %-20s %-8s %-8s %-10s %-8s\n", 
                           "ID", "Ten phong", "So nguoi", "Toi da", "Trang thai", "Owner");
                    printf("------------------------------------------------------------\n");
                    
                    for (int i = 0; i < room_count; i++) {
                        int32_t room_id = (int32_t)ntohl((uint32_t)rooms[i].room_id);
                        uint8_t state = rooms[i].state;
                        const char* state_str = (state == 0) ? "WAITING" : 
                                               (state == 1) ? "PLAYING" : "FINISHED";
                        int32_t owner_id = (int32_t)ntohl((uint32_t)rooms[i].owner_id);
                        
                        printf("%-5d %-20s %-8d %-8d %-10s %-8d\n",
                               room_id, rooms[i].room_name, 
                               rooms[i].player_count, rooms[i].max_players,
                               state_str, owner_id);
                    }
                }
                printf("==========================================\n\n");
                return 0;
            }
            break;
        }
        
        case MSG_CREATE_ROOM: {
            if (length >= sizeof(create_room_response_t)) {
                create_room_response_t* resp = (create_room_response_t*)(buffer + 3);
                uint8_t status = resp->status;
                int32_t room_id = (int32_t)ntohl((uint32_t)resp->room_id);
                
                if (status == STATUS_SUCCESS) {
                    printf("✓ Tao phong thanh cong! room_id=%d\n", room_id);
                    printf("  Message: %s\n", resp->message);
                    current_room_id = room_id;
                    return 0;
                } else {
                    printf("✗ Tao phong that bai: %s\n", resp->message);
                    return -1;
                }
            }
            break;
        }
        
        case MSG_JOIN_ROOM: {
            if (length >= sizeof(join_room_response_t)) {
                join_room_response_t* resp = (join_room_response_t*)(buffer + 3);
                uint8_t status = resp->status;
                int32_t room_id = (int32_t)ntohl((uint32_t)resp->room_id);
                
                if (status == STATUS_SUCCESS) {
                    printf("✓ Tham gia phong thanh cong! room_id=%d\n", room_id);
                    printf("  Message: %s\n", resp->message);
                    current_room_id = room_id;
                    return 0;
                } else {
                    printf("✗ Tham gia phong that bai: %s\n", resp->message);
                    return -1;
                }
            }
            break;
        }
        
        case MSG_LEAVE_ROOM: {
            if (length >= sizeof(leave_room_response_t)) {
                leave_room_response_t* resp = (leave_room_response_t*)(buffer + 3);
                uint8_t status = resp->status;
                
                if (status == STATUS_SUCCESS) {
                    printf("✓ Roi phong thanh cong!\n");
                    printf("  Message: %s\n", resp->message);
                    current_room_id = -1;
                    return 0;
                } else {
                    printf("✗ Roi phong that bai: %s\n", resp->message);
                    return -1;
                }
            }
            break;
        }
        
        default:
            printf("⚠ Nhan message type khong xu ly: 0x%02X\n", type);
            break;
    }
    
    return -1;
}

/**
 * Nhan va parse response (blocking - dung cho doi response cu the)
 */
int receive_response(int fd, uint8_t* expected_type) {
    uint8_t buffer[1024];
    
    // Danh dau dang doi response de receiver thread khong can thiep
    if (expected_type) {
        waiting_for_response = true;
        expected_response_type = *expected_type;
    }
    
    // Vong lap de xu ly tat ca messages (bao gom broadcast messages)
    while (1) {
        pthread_mutex_lock(&socket_mutex);
        ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        pthread_mutex_unlock(&socket_mutex);
        
        if (bytes_read <= 0) {
            waiting_for_response = false;
            if (bytes_read == 0) {
                printf("✗ Server da dong ket noi\n");
            } else {
                perror("recv() failed");
            }
            return -1;
        }
        
        if (bytes_read < 3) {
            printf("✗ Loi: Message qua ngan (%zd bytes)\n", bytes_read);
            waiting_for_response = false;
            return -1;
        }
        
        uint8_t type = buffer[0];
        uint16_t length_network;
        memcpy(&length_network, buffer + 1, 2);
        uint16_t length = ntohs(length_network);
        
        int result = process_message(buffer, length, type, expected_type);
        if (result == 0) {
            // Da nhan duoc response mong doi
            waiting_for_response = false;
            return 0;
        } else if (result == 1) {
            // Broadcast message, tiep tuc doi
            continue;
        } else {
            // Loi
            waiting_for_response = false;
            return -1;
        }
    }
}

/**
 * Thread function: Lien tuc nhan messages tu server (background)
 * Chi xu ly broadcast messages (ROOM_UPDATE, ROOM_PLAYERS_UPDATE, ROOM_LIST_RESPONSE)
 */
void* message_receiver_thread(void* arg) {
    int fd = *(int*)arg;
    uint8_t buffer[1024];
    
    printf("[Receiver Thread] Bat dau lang nghe broadcast messages...\n");
    
    while (receiver_running) {
        // Kiem tra neu main thread dang doi response, tam thoi khong doc
        if (waiting_for_response) {
            usleep(50000); // Sleep 50ms
            continue;
        }
        
        // Su dung select voi timeout de khong block qua lau
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int activity = select(fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (receiver_running) {
                perror("[Receiver Thread] select() failed");
            }
            break;
        }
        
        if (activity == 0) {
            // Timeout, tiep tuc
            continue;
        }
        
        if (!FD_ISSET(fd, &read_fds)) {
            continue;
        }
        
        // Kiem tra lai neu main thread dang doi response
        if (waiting_for_response) {
            continue;
        }
        
        // Peek message type truoc (khong remove khoi buffer)
        uint8_t peek_buffer[3];
        pthread_mutex_lock(&socket_mutex);
        ssize_t peek_bytes = recv(fd, peek_buffer, 3, MSG_PEEK);
        pthread_mutex_unlock(&socket_mutex);
        
        if (peek_bytes < 3) {
            if (peek_bytes == 0) {
                printf("\n[Receiver Thread] ✗ Server da dong ket noi\n");
                break;
            }
            continue;
        }
        
        uint8_t type = peek_buffer[0];
        
        // Kiem tra neu dang doi response nay, khong xu ly nhu broadcast
        if (waiting_for_response && expected_response_type == type) {
            // Main thread dang doi response nay, khong doc (de main thread doc)
            usleep(50000);
            continue;
        }
        
        // Chi xu ly broadcast messages
        if (type == MSG_ROOM_UPDATE || type == MSG_ROOM_PLAYERS_UPDATE || type == MSG_ROOM_LIST_RESPONSE) {
            // Doc message thuc su (chi khi la broadcast message)
            pthread_mutex_lock(&socket_mutex);
            ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
            pthread_mutex_unlock(&socket_mutex);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("\n[Receiver Thread] ✗ Server da dong ket noi\n");
                    break;
                } else {
                    if (receiver_running) {
                        perror("[Receiver Thread] recv() failed");
                    }
                    break;
                }
            }
            
            if (bytes_read < 3) {
                continue;
            }
            
            uint16_t length_network;
            memcpy(&length_network, buffer + 1, 2);
            uint16_t length = ntohs(length_network);
            
            process_message(buffer, length, type, NULL);
        } else {
            // Khong phai broadcast message, bo qua (de main thread xu ly)
            // Khong doc message de main thread co the doc sau
            usleep(50000); // Sleep de tranh busy loop va de main thread co co hoi doc
        }
    }
    
    printf("[Receiver Thread] Da dung.\n");
    return NULL;
}

/**
 * Khoi dong receiver thread
 */
int start_receiver_thread(int fd) {
    receiver_running = true;
    if (pthread_create(&receiver_thread, NULL, message_receiver_thread, &fd) != 0) {
        perror("pthread_create() failed");
        return -1;
    }
    printf("✓ Da khoi dong receiver thread\n");
    return 0;
}

/**
 * Dung receiver thread
 */
void stop_receiver_thread() {
    receiver_running = false;
    // Dong socket de recv() return va thread thoat
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RD);
    }
    pthread_join(receiver_thread, NULL);
    printf("✓ Da dung receiver thread\n");
}

/**
 * Test dang nhap
 */
int test_login(const char* username, const char* password) {
    if (!is_logged_in) {
        login_request_t req;
        memset(&req, 0, sizeof(req));
        strncpy(req.username, username, MAX_USERNAME_LEN - 1);
        strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
        
        uint8_t expected = MSG_LOGIN_RESPONSE;
        send_message(sockfd, MSG_LOGIN_REQUEST, (uint8_t*)&req, sizeof(req));
        return receive_response(sockfd, &expected);
    } else {
        printf("⚠ Da dang nhap roi (user_id=%d)\n", current_user_id);
        return 0;
    }
}

/**
 * Test lay danh sach phong
 */
int test_room_list() {
    if (!is_logged_in) {
        printf("✗ Can dang nhap truoc!\n");
        return -1;
    }
    
    uint8_t expected = MSG_ROOM_LIST_RESPONSE;
    send_message(sockfd, MSG_ROOM_LIST_REQUEST, NULL, 0);
    return receive_response(sockfd, &expected);
}

/**
 * Test tao phong
 */
int test_create_room(const char* room_name, int max_players, int rounds) {
    if (!is_logged_in) {
        printf("✗ Can dang nhap truoc!\n");
        return -1;
    }
    
    create_room_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.room_name, room_name, MAX_ROOM_NAME_LEN - 1);
    req.max_players = (uint8_t)max_players;
    req.rounds = (uint8_t)rounds;
    
    uint8_t expected = MSG_CREATE_ROOM;
    send_message(sockfd, MSG_CREATE_ROOM, (uint8_t*)&req, sizeof(req));
    return receive_response(sockfd, &expected);
}

/**
 * Test tham gia phong
 */
int test_join_room(int room_id) {
    if (!is_logged_in) {
        printf("✗ Can dang nhap truoc!\n");
        return -1;
    }
    
    join_room_request_t req;
    req.room_id = (int32_t)htonl((uint32_t)room_id);
    
    uint8_t expected = MSG_JOIN_ROOM;
    send_message(sockfd, MSG_JOIN_ROOM, (uint8_t*)&req, sizeof(req));
    return receive_response(sockfd, &expected);
}

/**
 * Test roi phong
 */
int test_leave_room(int room_id) {
    if (!is_logged_in) {
        printf("✗ Can dang nhap truoc!\n");
        return -1;
    }
    
    leave_room_request_t req;
    req.room_id = (int32_t)htonl((uint32_t)room_id);
    
    uint8_t expected = MSG_LEAVE_ROOM;
    send_message(sockfd, MSG_LEAVE_ROOM, (uint8_t*)&req, sizeof(req));
    return receive_response(sockfd, &expected);
}

/**
 * Hien thi menu
 */
void print_menu() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║          ROOM PROTOCOL TEST MENU                   ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ 1.  Dang nhap                                      ║\n");
    printf("║ 2.  Lay danh sach phong                            ║\n");
    printf("║ 3.  Tao phong                                      ║\n");
    printf("║ 4.  Tham gia phong                                 ║\n");
    printf("║ 5.  Roi phong                                      ║\n");
    printf("║ 6.  Xem trang thai hien tai                       ║\n");
    printf("║ 7.  Lang nghe ROOM_UPDATE (non-blocking)           ║\n");
    printf("║ 0.  Thoat                                          ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\n");
    if (is_logged_in) {
        printf("Trang thai: Da dang nhap (user_id=%d", current_user_id);
        if (current_room_id > 0) {
            printf(", room_id=%d", current_room_id);
        }
        printf(")\n");
    } else {
        printf("Trang thai: Chua dang nhap\n");
    }
    printf("Lua chon: ");
}

/**
 * Lang nghe ROOM_UPDATE (non-blocking)
 */
void listen_room_updates() {
    printf("Dang lang nghe ROOM_UPDATE (nhan Enter de dung)...\n");
    
    fd_set read_fds;
    struct timeval timeout;
    int max_fd = sockfd;
    
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            perror("select() failed");
            break;
        }
        
        if (activity == 0) {
            // Timeout, tiep tuc
            continue;
        }
        
        // Kiem tra stdin (Enter de dung)
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0 && c == '\n') {
                printf("Dung lang nghe.\n");
                break;
            }
        }
        
        // Kiem tra socket
        if (FD_ISSET(sockfd, &read_fds)) {
            receive_response(sockfd, NULL);
        }
    }
}

// Signal handler de dung thread khi thoat
void signal_handler(int sig) {
    printf("\nNhan tin hieu dung, dang dong ket noi...\n");
    stop_receiver_thread();
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

int main() {
    // Dang ky signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== ROOM PROTOCOL TEST CLIENT (DA LUONG) ===\n");
    printf("Ket noi den server %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
        return 1;
    }
    
    // Khoi dong receiver thread de nhan broadcast messages
    if (start_receiver_thread(sockfd) < 0) {
        fprintf(stderr, "Khong the khoi dong receiver thread\n");
        close(sockfd);
        return 1;
    }
    
    int choice;
    char input[256];
    
    while (1) {
        print_menu();
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        choice = atoi(input);
        
        switch (choice) {
            case 1: {
                char username[64], password[64];
                printf("Username: ");
                if (fgets(username, sizeof(username), stdin)) {
                    username[strcspn(username, "\n")] = 0;
                }
                printf("Password: ");
                if (fgets(password, sizeof(password), stdin)) {
                    password[strcspn(password, "\n")] = 0;
                }
                test_login(username, password);
                break;
            }
            
            case 2:
                test_room_list();
                break;
            
            case 3: {
                char room_name[64];
                int max_players = 4, rounds = 3;
                printf("Ten phong: ");
                if (fgets(room_name, sizeof(room_name), stdin)) {
                    room_name[strcspn(room_name, "\n")] = 0;
                }
                printf("So nguoi choi toi da (2-10, mac dinh 4): ");
                if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                    max_players = atoi(input);
                }
                printf("So rounds (1-10, mac dinh 3): ");
                if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                    rounds = atoi(input);
                }
                test_create_room(room_name, max_players, rounds);
                break;
            }
            
            case 4: {
                int room_id = 0;
                printf("Room ID: ");
                if (fgets(input, sizeof(input), stdin)) {
                    room_id = atoi(input);
                }
                if (room_id > 0) {
                    test_join_room(room_id);
                } else {
                    printf("✗ Room ID khong hop le\n");
                }
                break;
            }
            
            case 5: {
                int room_id = 0;
                if (current_room_id > 0) {
                    room_id = current_room_id;
                    printf("Roi phong hien tai (room_id=%d)? (y/n): ", room_id);
                    if (fgets(input, sizeof(input), stdin)) {
                        if (input[0] != 'y' && input[0] != 'Y') {
                            printf("Huy bo.\n");
                            break;
                        }
                    }
                } else {
                    printf("Room ID: ");
                    if (fgets(input, sizeof(input), stdin)) {
                        room_id = atoi(input);
                    } else {
                        printf("✗ Khong the doc input\n");
                        break;
                    }
                }
                if (room_id > 0) {
                    test_leave_room(room_id);
                } else {
                    printf("✗ Room ID khong hop le\n");
                }
                break;
            }
            
            case 6:
                printf("\n=== TRANG THAI HIEN TAI ===\n");
                printf("Ket noi: %s\n", (sockfd >= 0) ? "Da ket noi" : "Chua ket noi");
                printf("Dang nhap: %s\n", is_logged_in ? "Da dang nhap" : "Chua dang nhap");
                if (is_logged_in) {
                    printf("User ID: %d\n", current_user_id);
                }
                if (current_room_id > 0) {
                    printf("Room ID hien tai: %d\n", current_room_id);
                } else {
                    printf("Room ID hien tai: Khong co\n");
                }
                printf("==========================\n");
                break;
            
            case 7:
                listen_room_updates();
                break;
            
            case 0:
                printf("Thoat chuong trinh.\n");
                goto cleanup;
            
            default:
                printf("✗ Lua chon khong hop le!\n");
                break;
        }
    }
    
cleanup:
    // Dung receiver thread
    stop_receiver_thread();
    
    if (sockfd >= 0) {
        close(sockfd);
    }
    
    return 0;
}

