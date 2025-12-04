#include "common/protocol.h"
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
 * Kết nối đến server
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

    printf("✓ Đã kết nối đến server %s:%d\n", ip, port);
    return fd;
}

/**
 * Gửi message đến server
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
 * Xử lý một message đã nhận được
 * @return 1 nếu là broadcast message (tiếp tục đợi), 0 nếu là response mong đợi, -1 nếu lỗi
 */
static int process_message(uint8_t* buffer, uint16_t length, uint8_t type, uint8_t* expected_type) {
    // Xử lý ROOM_UPDATE ngay lập tức (broadcast message)
    if (type == MSG_ROOM_UPDATE) {
        if (length >= sizeof(room_info_protocol_t)) {
            room_info_protocol_t* room_info = (room_info_protocol_t*)(buffer + 3);
            int32_t room_id = (int32_t)ntohl((uint32_t)room_info->room_id);
            uint8_t state = room_info->state;
            const char* state_str = (state == 0) ? "WAITING" : 
                                   (state == 1) ? "PLAYING" : "FINISHED";
            
            printf("\n[ROOM_UPDATE] Phòng %d: %s - %d/%d người chơi (%s)\n",
                   room_id, room_info->room_name, 
                   room_info->player_count, room_info->max_players, state_str);
            return 1; // Đã xử lý, tiếp tục đợi
        }
    }
    
    // Xử lý ROOM_PLAYERS_UPDATE (broadcast message với danh sách đầy đủ)
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
            
            const char* action_str = (action == 0) ? "đã tham gia" : "đã rời";
            printf("\n[ROOM_PLAYERS_UPDATE] Phòng %d (%s): %s (ID: %d) %s phòng\n",
                   room_id, update->room_name, update->changed_username, changed_user_id, action_str);
            printf("  Trạng thái: %s | Số người: %d/%d | Owner: %d\n",
                   state_str, player_count, update->max_players, owner_id);
            
            // Hiển thị danh sách người chơi hiện tại
            player_info_protocol_t* players = (player_info_protocol_t*)(buffer + 3 + sizeof(room_players_update_t));
            printf("Danh sách người chơi hiện tại (%d người):\n", player_count);
            for (int i = 0; i < player_count; i++) {
                int32_t pid = (int32_t)ntohl((uint32_t)players[i].user_id);
                const char* owner_mark = players[i].is_owner ? " (Owner)" : "";
                printf("  - %s (ID: %d)%s\n", players[i].username, pid, owner_mark);
            }
            printf("\n");
            return 1; // Đã xử lý, tiếp tục đợi
        }
    }
    
    // Kiểm tra message type mong đợi (chỉ khi đang đợi response)
    if (expected_type && type != *expected_type) {
        printf("✗ Lỗi: Nhận message type 0x%02X, mong đợi 0x%02X\n", type, *expected_type);
        return -1;
    }
    
    printf("✓ Nhận response: type=0x%02X, length=%d\n", type, length);
    
    // Parse response dựa trên type
    switch (type) {
        case MSG_LOGIN_RESPONSE: {
            if (length >= sizeof(login_response_t)) {
                login_response_t* resp = (login_response_t*)(buffer + 3);
                uint8_t status = resp->status;
                int32_t user_id = (int32_t)ntohl((uint32_t)resp->user_id);
                
                if (status == STATUS_SUCCESS) {
                    printf("✓ Đăng nhập thành công! user_id=%d, username=%s\n", 
                           user_id, resp->username);
                    current_user_id = user_id;
                    is_logged_in = true;
                    return 0;
                } else {
                    printf("✗ Đăng nhập thất bại (status=0x%02X)\n", status);
                    return -1;
                }
            }
            break;
        }
        
        case MSG_ROOM_LIST_RESPONSE: {
            if (length >= sizeof(room_list_response_t)) {
                room_list_response_t* header = (room_list_response_t*)(buffer + 3);
                uint16_t room_count = ntohs(header->room_count);
                
                printf("\n=== DANH SÁCH PHÒNG (%d phòng) ===\n", room_count);
                
                if (room_count > 0) {
                    room_info_protocol_t* rooms = (room_info_protocol_t*)(buffer + 3 + sizeof(room_list_response_t));
                    
                    printf("%-5s %-20s %-8s %-8s %-10s %-8s\n", 
                           "ID", "Tên phòng", "Số người", "Tối đa", "Trạng thái", "Owner");
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
                    printf("✓ Tạo phòng thành công! room_id=%d\n", room_id);
                    printf("  Message: %s\n", resp->message);
                    current_room_id = room_id;
                    return 0;
                } else {
                    printf("✗ Tạo phòng thất bại: %s\n", resp->message);
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
                    printf("✓ Tham gia phòng thành công! room_id=%d\n", room_id);
                    printf("  Message: %s\n", resp->message);
                    current_room_id = room_id;
                    return 0;
                } else {
                    printf("✗ Tham gia phòng thất bại: %s\n", resp->message);
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
                    printf("✓ Rời phòng thành công!\n");
                    printf("  Message: %s\n", resp->message);
                    current_room_id = -1;
                    return 0;
                } else {
                    printf("✗ Rời phòng thất bại: %s\n", resp->message);
                    return -1;
                }
            }
            break;
        }
        
        default:
            printf("⚠ Nhận message type không xử lý: 0x%02X\n", type);
            break;
    }
    
    return -1;
}

/**
 * Nhận và parse response (blocking - dùng cho đợi response cụ thể)
 */
int receive_response(int fd, uint8_t* expected_type) {
    uint8_t buffer[1024];
    
    // Đánh dấu đang đợi response để receiver thread không can thiệp
    if (expected_type) {
        waiting_for_response = true;
        expected_response_type = *expected_type;
    }
    
    // Vòng lặp để xử lý tất cả messages (bao gồm broadcast messages)
    while (1) {
        pthread_mutex_lock(&socket_mutex);
        ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        pthread_mutex_unlock(&socket_mutex);
        
        if (bytes_read <= 0) {
            waiting_for_response = false;
            if (bytes_read == 0) {
                printf("✗ Server đã đóng kết nối\n");
            } else {
                perror("recv() failed");
            }
            return -1;
        }
        
        if (bytes_read < 3) {
            printf("✗ Lỗi: Message quá ngắn (%zd bytes)\n", bytes_read);
            waiting_for_response = false;
            return -1;
        }
        
        uint8_t type = buffer[0];
        uint16_t length_network;
        memcpy(&length_network, buffer + 1, 2);
        uint16_t length = ntohs(length_network);
        
        int result = process_message(buffer, length, type, expected_type);
        if (result == 0) {
            // Đã nhận được response mong đợi
            waiting_for_response = false;
            return 0;
        } else if (result == 1) {
            // Broadcast message, tiếp tục đợi
            continue;
        } else {
            // Lỗi
            waiting_for_response = false;
            return -1;
        }
    }
}

/**
 * Thread function: Liên tục nhận messages từ server (background)
 * Chỉ xử lý broadcast messages (ROOM_UPDATE, ROOM_PLAYERS_UPDATE)
 */
void* message_receiver_thread(void* arg) {
    int fd = *(int*)arg;
    uint8_t buffer[1024];
    
    printf("[Receiver Thread] Bắt đầu lắng nghe broadcast messages...\n");
    
    while (receiver_running) {
        // Kiểm tra nếu main thread đang đợi response, tạm thời không đọc
        if (waiting_for_response) {
            usleep(50000); // Sleep 50ms
            continue;
        }
        
        // Sử dụng select với timeout để không block quá lâu
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
            // Timeout, tiếp tục
            continue;
        }
        
        if (!FD_ISSET(fd, &read_fds)) {
            continue;
        }
        
        // Kiểm tra lại nếu main thread đang đợi response
        if (waiting_for_response) {
            continue;
        }
        
        // Peek message type trước (không remove khỏi buffer)
        uint8_t peek_buffer[3];
        pthread_mutex_lock(&socket_mutex);
        ssize_t peek_bytes = recv(fd, peek_buffer, 3, MSG_PEEK);
        pthread_mutex_unlock(&socket_mutex);
        
        if (peek_bytes < 3) {
            if (peek_bytes == 0) {
                printf("\n[Receiver Thread] ✗ Server đã đóng kết nối\n");
                break;
            }
            continue;
        }
        
        uint8_t type = peek_buffer[0];
        
        // Chỉ xử lý broadcast messages
        if (type == MSG_ROOM_UPDATE || type == MSG_ROOM_PLAYERS_UPDATE) {
            // Đọc message thực sự (chỉ khi là broadcast message)
            pthread_mutex_lock(&socket_mutex);
            ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
            pthread_mutex_unlock(&socket_mutex);
            
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    printf("\n[Receiver Thread] ✗ Server đã đóng kết nối\n");
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
            // Không phải broadcast message, bỏ qua (để main thread xử lý)
            // Không đọc message để main thread có thể đọc sau
            usleep(50000); // Sleep để tránh busy loop và để main thread có cơ hội đọc
        }
    }
    
    printf("[Receiver Thread] Đã dừng.\n");
    return NULL;
}

/**
 * Khởi động receiver thread
 */
int start_receiver_thread(int fd) {
    receiver_running = true;
    if (pthread_create(&receiver_thread, NULL, message_receiver_thread, &fd) != 0) {
        perror("pthread_create() failed");
        return -1;
    }
    printf("✓ Đã khởi động receiver thread\n");
    return 0;
}

/**
 * Dừng receiver thread
 */
void stop_receiver_thread() {
    receiver_running = false;
    // Đóng socket để recv() return và thread thoát
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RD);
    }
    pthread_join(receiver_thread, NULL);
    printf("✓ Đã dừng receiver thread\n");
}

/**
 * Test đăng nhập
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
        printf("⚠ Đã đăng nhập rồi (user_id=%d)\n", current_user_id);
        return 0;
    }
}

/**
 * Test lấy danh sách phòng
 */
int test_room_list() {
    if (!is_logged_in) {
        printf("✗ Cần đăng nhập trước!\n");
        return -1;
    }
    
    uint8_t expected = MSG_ROOM_LIST_RESPONSE;
    send_message(sockfd, MSG_ROOM_LIST_REQUEST, NULL, 0);
    return receive_response(sockfd, &expected);
}

/**
 * Test tạo phòng
 */
int test_create_room(const char* room_name, int max_players, int rounds) {
    if (!is_logged_in) {
        printf("✗ Cần đăng nhập trước!\n");
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
 * Test tham gia phòng
 */
int test_join_room(int room_id) {
    if (!is_logged_in) {
        printf("✗ Cần đăng nhập trước!\n");
        return -1;
    }
    
    join_room_request_t req;
    req.room_id = (int32_t)htonl((uint32_t)room_id);
    
    uint8_t expected = MSG_JOIN_ROOM;
    send_message(sockfd, MSG_JOIN_ROOM, (uint8_t*)&req, sizeof(req));
    return receive_response(sockfd, &expected);
}

/**
 * Test rời phòng
 */
int test_leave_room(int room_id) {
    if (!is_logged_in) {
        printf("✗ Cần đăng nhập trước!\n");
        return -1;
    }
    
    leave_room_request_t req;
    req.room_id = (int32_t)htonl((uint32_t)room_id);
    
    uint8_t expected = MSG_LEAVE_ROOM;
    send_message(sockfd, MSG_LEAVE_ROOM, (uint8_t*)&req, sizeof(req));
    return receive_response(sockfd, &expected);
}

/**
 * Hiển thị menu
 */
void print_menu() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════╗\n");
    printf("║          ROOM PROTOCOL TEST MENU                   ║\n");
    printf("╠════════════════════════════════════════════════════╣\n");
    printf("║ 1.  Đăng nhập                                      ║\n");
    printf("║ 2.  Lấy danh sách phòng                            ║\n");
    printf("║ 3.  Tạo phòng                                      ║\n");
    printf("║ 4.  Tham gia phòng                                 ║\n");
    printf("║ 5.  Rời phòng                                      ║\n");
    printf("║ 6.  Xem trạng thái hiện tại                       ║\n");
    printf("║ 7.  Lắng nghe ROOM_UPDATE (non-blocking)           ║\n");
    printf("║ 0.  Thoát                                          ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("\n");
    if (is_logged_in) {
        printf("Trạng thái: Đã đăng nhập (user_id=%d", current_user_id);
        if (current_room_id > 0) {
            printf(", room_id=%d", current_room_id);
        }
        printf(")\n");
    } else {
        printf("Trạng thái: Chưa đăng nhập\n");
    }
    printf("Lựa chọn: ");
}

/**
 * Lắng nghe ROOM_UPDATE (non-blocking)
 */
void listen_room_updates() {
    printf("Đang lắng nghe ROOM_UPDATE (nhấn Enter để dừng)...\n");
    
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
            // Timeout, tiếp tục
            continue;
        }
        
        // Kiểm tra stdin (Enter để dừng)
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0 && c == '\n') {
                printf("Dừng lắng nghe.\n");
                break;
            }
        }
        
        // Kiểm tra socket
        if (FD_ISSET(sockfd, &read_fds)) {
            receive_response(sockfd, NULL);
        }
    }
}

// Signal handler để dừng thread khi thoát
void signal_handler(int sig) {
    printf("\nNhận tín hiệu dừng, đang đóng kết nối...\n");
    stop_receiver_thread();
    if (sockfd >= 0) {
        close(sockfd);
    }
    exit(0);
}

int main() {
    // Đăng ký signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== ROOM PROTOCOL TEST CLIENT (ĐA LUỒNG) ===\n");
    printf("Kết nối đến server %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
        return 1;
    }
    
    // Khởi động receiver thread để nhận broadcast messages
    if (start_receiver_thread(sockfd) < 0) {
        fprintf(stderr, "Không thể khởi động receiver thread\n");
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
                printf("Tên phòng: ");
                if (fgets(room_name, sizeof(room_name), stdin)) {
                    room_name[strcspn(room_name, "\n")] = 0;
                }
                printf("Số người chơi tối đa (2-10, mặc định 4): ");
                if (fgets(input, sizeof(input), stdin) && strlen(input) > 1) {
                    max_players = atoi(input);
                }
                printf("Số rounds (1-10, mặc định 3): ");
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
                    printf("✗ Room ID không hợp lệ\n");
                }
                break;
            }
            
            case 5: {
                int room_id = 0;
                if (current_room_id > 0) {
                    room_id = current_room_id;
                    printf("Rời phòng hiện tại (room_id=%d)? (y/n): ", room_id);
                    if (fgets(input, sizeof(input), stdin)) {
                        if (input[0] != 'y' && input[0] != 'Y') {
                            printf("Hủy bỏ.\n");
                            break;
                        }
                    }
                } else {
                    printf("Room ID: ");
                    if (fgets(input, sizeof(input), stdin)) {
                        room_id = atoi(input);
                    } else {
                        printf("✗ Không thể đọc input\n");
                        break;
                    }
                }
                if (room_id > 0) {
                    test_leave_room(room_id);
                } else {
                    printf("✗ Room ID không hợp lệ\n");
                }
                break;
            }
            
            case 6:
                printf("\n=== TRẠNG THÁI HIỆN TẠI ===\n");
                printf("Kết nối: %s\n", (sockfd >= 0) ? "Đã kết nối" : "Chưa kết nối");
                printf("Đăng nhập: %s\n", is_logged_in ? "Đã đăng nhập" : "Chưa đăng nhập");
                if (is_logged_in) {
                    printf("User ID: %d\n", current_user_id);
                }
                if (current_room_id > 0) {
                    printf("Room ID hiện tại: %d\n", current_room_id);
                } else {
                    printf("Room ID hiện tại: Không có\n");
                }
                printf("==========================\n");
                break;
            
            case 7:
                listen_room_updates();
                break;
            
            case 0:
                printf("Thoát chương trình.\n");
                goto cleanup;
            
            default:
                printf("✗ Lựa chọn không hợp lệ!\n");
                break;
        }
    }
    
cleanup:
    // Dừng receiver thread
    stop_receiver_thread();
    
    if (sockfd >= 0) {
        close(sockfd);
    }
    
    return 0;
}

