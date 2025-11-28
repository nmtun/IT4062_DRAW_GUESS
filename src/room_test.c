#include "common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

static int sockfd = -1;
static bool is_logged_in = false;
static int current_user_id = -1;
static int current_room_id = -1;

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
 * Nhận và parse response
 */
int receive_response(int fd, uint8_t* expected_type) {
    uint8_t buffer[1024];
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("✗ Server đã đóng kết nối\n");
        } else {
            perror("recv() failed");
        }
        return -1;
    }
    
    if (bytes_read < 3) {
        printf("✗ Lỗi: Message quá ngắn (%zd bytes)\n", bytes_read);
        return -1;
    }
    
    uint8_t type = buffer[0];
    uint16_t length_network;
    memcpy(&length_network, buffer + 2, 2);
    uint16_t length = ntohs(length_network);
    
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
        
        case MSG_ROOM_UPDATE: {
            if (length >= sizeof(room_info_protocol_t)) {
                room_info_protocol_t* room_info = (room_info_protocol_t*)(buffer + 3);
                int32_t room_id = (int32_t)ntohl((uint32_t)room_info->room_id);
                uint8_t state = room_info->state;
                const char* state_str = (state == 0) ? "WAITING" : 
                                       (state == 1) ? "PLAYING" : "FINISHED";
                
                printf("\n[ROOM_UPDATE] Phòng %d: %s - %d/%d người chơi (%s)\n",
                       room_id, room_info->room_name, 
                       room_info->player_count, room_info->max_players, state_str);
                return 0;
            }
            break;
        }
        
        default:
            printf("⚠ Nhận message type không xử lý: 0x%02X\n", type);
            break;
    }
    
    return 0;
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

int main() {
    printf("=== ROOM PROTOCOL TEST CLIENT ===\n");
    printf("Kết nối đến server %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
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
    if (sockfd >= 0) {
        close(sockfd);
    }
    
    return 0;
}

