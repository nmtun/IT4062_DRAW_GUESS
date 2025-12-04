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

/**
 * Kết nối đến server
 */
int connect_to_server(const char* ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() failed");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sockfd);
        return -1;
    }

    printf("Đã kết nối đến server %s:%d\n", ip, port);
    return sockfd;
}

/**
 * Gửi message đến server
 */
int send_message(int sockfd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[1024];
    
    // Tạo message format: [TYPE][LENGTH][PAYLOAD]
    buffer[0] = type;
    
    // Length (network byte order)
    uint16_t length_network = htons(payload_len);
    memcpy(buffer + 1, &length_network, 2);
    
    // Copy payload
    if (payload && payload_len > 0) {
        memcpy(buffer + 3, payload, payload_len);
    }
    
    int total_len = 3 + payload_len;
    ssize_t sent = send(sockfd, buffer, total_len, 0);
    
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }
    
    printf("Đã gửi %zd bytes (type=0x%02X, payload_len=%d)\n", sent, type, payload_len);
    return 0;
}

/**
 * Nhận và parse response
 */
 int receive_response(int fd, uint8_t* expected_type) {
    uint8_t buffer[1024];
    
    while (1) {  // Vòng lặp để xử lý tất cả messages
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
        memcpy(&length_network, buffer + 1, 2);
        uint16_t length = ntohs(length_network);
        
        // Xử lý ROOM_UPDATE ngay lập tức nếu nhận được
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
                
                // Tiếp tục đợi message mong đợi
                continue;
            }
        }
        
        // Kiểm tra message type mong đợi
        if (expected_type && type != *expected_type) {
            printf("✗ Lỗi: Nhận message type 0x%02X, mong đợi 0x%02X\n", type, *expected_type);
            return -1;
        }
        
        printf("✓ Nhận response: type=0x%02X, length=%d\n", type, length);
        
        // Parse response dựa trên type (giữ nguyên code cũ)
        switch (type) {
            // ... các case khác giữ nguyên ...
        }
        
        return 0;  // Đã nhận được message mong đợi
    }
}

/**
 * Test LOGIN
 */
int test_login(int sockfd, const char* username, const char* password) {
    printf("\n=== TEST LOGIN ===\n");
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    
    // Tạo login request
    login_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
    
    // Gửi LOGIN_REQUEST
    if (send_message(sockfd, MSG_LOGIN_REQUEST, (uint8_t*)&req, sizeof(req)) < 0) {
        return -1;
    }
    
    // Nhận response
    uint8_t expected = MSG_LOGIN_RESPONSE;
    return receive_response(sockfd, &expected);
}

/**
 * Test REGISTER
 */
int test_register(int sockfd, const char* username, const char* password, const char* email) {
    printf("\n=== TEST REGISTER ===\n");
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    printf("Email: %s\n", email);
    
    // Tạo register request
    register_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
    strncpy(req.email, email, MAX_EMAIL_LEN - 1);
    
    // Gửi REGISTER_REQUEST
    if (send_message(sockfd, MSG_REGISTER_REQUEST, (uint8_t*)&req, sizeof(req)) < 0) {
        return -1;
    }
    
    // Nhận response
    uint8_t expected = MSG_REGISTER_RESPONSE;
    return receive_response(sockfd, &expected);
}

void login_session_loop(int sockfd) {
    // Đợi lệnh từ người dùng, ví dụ: gửi lệnh hoặc nhận data từ server
    // Ở đây chỉ đơn giản là chờ server gửi tin nhắn hoặc nhập lệnh từ stdin
    printf("Bạn đang ở trạng thái đã đăng nhập. Nhập 'quit' để thoát.\n");
    fd_set readfds;
    char input[256];
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin
        FD_SET(sockfd, &readfds);
        int maxfd = (sockfd > 0 ? sockfd : 0);

        int ready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            perror("select() failed");
            break;
        }
        // Nhập từ bàn phím
        if (FD_ISSET(0, &readfds)) {
            if (fgets(input, sizeof(input), stdin)) {
                // Loại bỏ ký tự newline
                size_t len = strlen(input);
                if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
                if (strcmp(input, "quit") == 0) {
                    printf("Đang đăng xuất và đóng kết nối...\n");
                    break;
                }
                // Nếu có lệnh khác, gửi message hoặc xử lý tùy ý. Ở đây chỉ in ra.
                printf("Bạn đã nhập: %s (có thể hiện thực gửi lệnh này lên server)\n", input);
            }
        }
        // Có dữ liệu từ server
        if (FD_ISSET(sockfd, &readfds)) {
            printf("Có message từ server:\n");
            if (receive_response(sockfd, NULL) < 0) {
                printf("Mất kết nối server hoặc lỗi khi nhận message.\n");
                break;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        printf("\nCommands:\n");
        printf("  login <username> <password>     - Test login\n");
        printf("  register <username> <password> <email> - Test register\n");
        printf("\nExample:\n");
        printf("  %s login demo_user mypass123\n", argv[0]);
        printf("  %s register testuser pass123 test@email.com\n", argv[0]);
        return 1;
    }
    
    // Kết nối đến server
    int sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
        return 1;
    }
    
    int result = 0;
    bool is_logged_in = false;
    
    if (strcmp(argv[1], "login") == 0) {
        if (argc < 4) {
            printf("Lỗi: Thiếu username hoặc password\n");
            printf("Usage: %s login <username> <password>\n", argv[0]);
            result = 1;
        } else {
            result = test_login(sockfd, argv[2], argv[3]);
            if (result == 0) {
                is_logged_in = true;
            }
        }
    } else if (strcmp(argv[1], "register") == 0) {
        if (argc < 5) {
            printf("Lỗi: Thiếu thông tin đăng ký\n");
            printf("Usage: %s register <username> <password> <email>\n", argv[0]);
            result = 1;
        } else {
            result = test_register(sockfd, argv[2], argv[3], argv[4]);
        }
    } else {
        printf("Lỗi: Command không hợp lệ: %s\n", argv[1]);
        result = 1;
    }

    // Nếu đã đăng nhập thành công thì giữ kết nối, cho phép gửi lệnh
    if (is_logged_in) {
        login_session_loop(sockfd);
    }

    close(sockfd);
    return result;
}
