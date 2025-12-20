#include "../common/protocol.h"
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
 * Ket noi den server
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

    printf("Da ket noi den server %s:%d\n", ip, port);
    return sockfd;
}

/**
 * Gui message den server
 */
int send_message(int sockfd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[1024];
    
    // Tao message format: [TYPE][LENGTH][PAYLOAD]
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
    
    printf("Da gui %zd bytes (type=0x%02X, payload_len=%d)\n", sent, type, payload_len);
    return 0;
}

/**
 * Nhan va parse response
 */
 int receive_response(int fd, uint8_t* expected_type) {
    uint8_t buffer[1024];
    
    while (1) {  // Vong lap de xu ly tat ca messages
        ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("✗ Server da dong ket noi\n");
            } else {
                perror("recv() failed");
            }
            return -1;
        }
        
        if (bytes_read < 3) {
            printf("✗ Loi: Message qua ngan (%zd bytes)\n", bytes_read);
            return -1;
        }
        
        uint8_t type = buffer[0];
        uint16_t length_network;
        memcpy(&length_network, buffer + 1, 2);
        uint16_t length = ntohs(length_network);
        
        // Xu ly ROOM_UPDATE ngay lap tuc neu nhan duoc
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
                
                // Tiep tuc doi message mong doi
                continue;
            }
        }
        
        // Kiem tra message type mong doi
        if (expected_type && type != *expected_type) {
            printf("✗ Loi: Nhan message type 0x%02X, mong doi 0x%02X\n", type, *expected_type);
            return -1;
        }
        
        printf("✓ Nhan response: type=0x%02X, length=%d\n", type, length);
        
        // Parse response dua tren type (giu nguyen code cu)
        switch (type) {
            // ... cac case khac giu nguyen ...
        }
        
        return 0;  // Da nhan duoc message mong doi
    }
}

/**
 * Test LOGIN
 */
int test_login(int sockfd, const char* username, const char* password) {
    printf("\n=== TEST LOGIN ===\n");
    printf("Username: %s\n", username);
    printf("Password: %s\n", password);
    
    // Tao login request
    login_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
    
    // Gui LOGIN_REQUEST
    if (send_message(sockfd, MSG_LOGIN_REQUEST, (uint8_t*)&req, sizeof(req)) < 0) {
        return -1;
    }
    
    // Nhan response
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
    
    // Tao register request
    register_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
    strncpy(req.email, email, MAX_EMAIL_LEN - 1);
    
    // Gui REGISTER_REQUEST
    if (send_message(sockfd, MSG_REGISTER_REQUEST, (uint8_t*)&req, sizeof(req)) < 0) {
        return -1;
    }
    
    // Nhan response
    uint8_t expected = MSG_REGISTER_RESPONSE;
    return receive_response(sockfd, &expected);
}

void login_session_loop(int sockfd) {
    // Doi lenh tu nguoi dung, vi du: gui lenh hoac nhan data tu server
    // O day chi don gian la cho server gui tin nhan hoac nhap lenh tu stdin
    printf("Ban dang o trang thai da dang nhap. Nhap 'quit' de thoat.\n");
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
        // Nhap tu ban phim
        if (FD_ISSET(0, &readfds)) {
            if (fgets(input, sizeof(input), stdin)) {
                // Loai bo ky tu newline
                size_t len = strlen(input);
                if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
                if (strcmp(input, "quit") == 0) {
                    printf("Dang dang xuat va dong ket noi...\n");
                    break;
                }
                // Neu co lenh khac, gui message hoac xu ly tuy y. O day chi in ra.
                printf("Ban da nhap: %s (co the hien thuc gui lenh nay len server)\n", input);
            }
        }
        // Co du lieu tu server
        if (FD_ISSET(sockfd, &readfds)) {
            printf("Co message tu server:\n");
            if (receive_response(sockfd, NULL) < 0) {
                printf("Mat ket noi server hoac loi khi nhan message.\n");
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
    
    // Ket noi den server
    int sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
        return 1;
    }
    
    int result = 0;
    bool is_logged_in = false;
    
    if (strcmp(argv[1], "login") == 0) {
        if (argc < 4) {
            printf("Loi: Thieu username hoac password\n");
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
            printf("Loi: Thieu thong tin dang ky\n");
            printf("Usage: %s register <username> <password> <email>\n", argv[0]);
            result = 1;
        } else {
            result = test_register(sockfd, argv[2], argv[3], argv[4]);
        }
    } else {
        printf("Loi: Command khong hop le: %s\n", argv[1]);
        result = 1;
    }

    // Neu da dang nhap thanh cong thi giu ket noi, cho phep gui lenh
    if (is_logged_in) {
        login_session_loop(sockfd);
    }

    close(sockfd);
    return result;
}
