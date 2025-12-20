#include "../common/protocol.h"
#include "../include/drawing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <arpa/inet.h>

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

    printf("✓ Da ket noi den server %s:%d\n", ip, port);
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
    
    if (sent != total_len) {
        printf("⚠ Canh bao: Chi gui duoc %zd/%d bytes\n", sent, total_len);
        return -1;
    }
    
    printf("✓ Da gui %zd bytes (type=0x%02X, payload_len=%d)\n", sent, type, payload_len);
    return 0;
}

/**
 * Nhan message tu server
 */
int receive_message(int sockfd, uint8_t* msg_type_out, uint8_t* buffer_out, size_t buffer_size) {
    uint8_t header[3];
    ssize_t bytes_read = recv(sockfd, header, 3, 0);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("✗ Server da dong ket noi\n");
        } else {
            perror("recv() failed");
        }
        return -1;
    }
    
    if (bytes_read < 3) {
        printf("✗ Loi: Message header qua ngan (%zd bytes)\n", bytes_read);
        return -1;
    }
    
    uint8_t type = header[0];
    uint16_t length_network;
    memcpy(&length_network, header + 1, 2);
    uint16_t length = ntohs(length_network);
    
    if (msg_type_out) {
        *msg_type_out = type;
    }
    
    if (length > 0) {
        if (length > buffer_size - 1) {
            printf("✗ Loi: Payload qua lon (%d bytes, buffer chi co %zu bytes)\n", length, buffer_size);
            return -1;
        }
        
        bytes_read = recv(sockfd, buffer_out, length, 0);
        if (bytes_read != length) {
            printf("✗ Loi: Chi nhan duoc %zd/%d bytes payload\n", bytes_read, length);
            return -1;
        }
        buffer_out[length] = '\0';  // Null-terminate de an toan
    }
    
    printf("✓ Nhan message: type=0x%02X, length=%d\n", type, length);
    return length;
}

/**
 * Test LOGIN
 */
int test_login(int sockfd, const char* username, const char* password) {
    printf("\n=== TEST LOGIN ===\n");
    printf("Username: %s\n", username);
    
    login_request_t req;
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    strncpy(req.password, password, MAX_PASSWORD_LEN - 1);
    
    if (send_message(sockfd, MSG_LOGIN_REQUEST, (uint8_t*)&req, sizeof(req)) < 0) {
        return -1;
    }
    
    uint8_t msg_type;
    uint8_t buffer[256];
    if (receive_message(sockfd, &msg_type, buffer, sizeof(buffer)) < 0) {
        return -1;
    }
    
    if (msg_type != MSG_LOGIN_RESPONSE) {
        printf("✗ Loi: Nhan message type 0x%02X, mong doi LOGIN_RESPONSE (0x%02X)\n", 
               msg_type, MSG_LOGIN_RESPONSE);
        return -1;
    }
    
    login_response_t* resp = (login_response_t*)buffer;
    if (resp->status == STATUS_SUCCESS) {
        int32_t user_id = (int32_t)ntohl((uint32_t)resp->user_id);
        printf("✓ Dang nhap thanh cong! user_id=%d, username=%s\n", user_id, resp->username);
        return user_id;
    } else {
        printf("✗ Dang nhap that bai (status=0x%02X)\n", resp->status);
        return -1;
    }
}

/**
 * Test gui DRAW_DATA
 */
int test_send_draw_data(int sockfd, uint8_t action, uint16_t x1, uint16_t y1, 
                        uint16_t x2, uint16_t y2, uint32_t color, uint8_t width) {
    printf("\n=== TEST SEND DRAW_DATA ===\n");
    printf("Action: %d, Line: (%d,%d) -> (%d,%d), Color: 0x%08X, Width: %d\n",
           action, x1, y1, x2, y2, color, width);
    
    // Tao draw action
    draw_action_t draw_action;
    draw_action.action = (draw_action_type_t)action;
    draw_action.x1 = x1;
    draw_action.y1 = y1;
    draw_action.x2 = x2;
    draw_action.y2 = y2;
    draw_action.color = color;
    draw_action.width = width;
    
    // Validate action
    if (!drawing_validate_action(&draw_action)) {
        printf("✗ Loi: Draw action khong hop le\n");
        return -1;
    }
    
    // Serialize action
    uint8_t payload[14];
    int payload_len = drawing_serialize_action(&draw_action, payload);
    if (payload_len != 14) {
        printf("✗ Loi: Serialize that bai (payload_len=%d, mong doi 14)\n", payload_len);
        return -1;
    }
    
    // Gui DRAW_DATA
    if (send_message(sockfd, MSG_DRAW_DATA, payload, 14) < 0) {
        return -1;
    }
    
    printf("✓ Da gui DRAW_DATA thanh cong\n");
    return 0;
}

/**
 * Test nhan DRAW_BROADCAST
 */
int test_receive_draw_broadcast(int sockfd, int timeout_seconds) {
    printf("\n=== TEST RECEIVE DRAW_BROADCAST ===\n");
    printf("Dang doi DRAW_BROADCAST tu server (timeout: %d giay)...\n", timeout_seconds);
    
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    
    int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (ready < 0) {
        perror("select() failed");
        return -1;
    }
    
    if (ready == 0) {
        printf("✗ Timeout: Khong nhan duoc DRAW_BROADCAST trong %d giay\n", timeout_seconds);
        return -1;
    }
    
    if (!FD_ISSET(sockfd, &readfds)) {
        printf("✗ Loi: Socket khong san sang\n");
        return -1;
    }
    
    uint8_t msg_type;
    uint8_t buffer[256];
    int payload_len = receive_message(sockfd, &msg_type, buffer, sizeof(buffer));
    
    if (payload_len < 0) {
        return -1;
    }
    
    if (msg_type != MSG_DRAW_BROADCAST) {
        printf("⚠ Nhan message type 0x%02X (khong phai DRAW_BROADCAST 0x%02X)\n",
               msg_type, MSG_DRAW_BROADCAST);
        return 0;  // Khong phai loi, chi la message khac
    }
    
    // Parse draw action
    draw_action_t action;
    if (drawing_parse_action(buffer, payload_len, &action) != 0) {
        printf("✗ Loi: Khong the parse draw action\n");
        return -1;
    }
    
    printf("✓ Nhan DRAW_BROADCAST thanh cong:\n");
    printf("  Action: %d\n", action.action);
    printf("  Line: (%d,%d) -> (%d,%d)\n", action.x1, action.y1, action.x2, action.y2);
    printf("  Color: 0x%08X\n", action.color);
    printf("  Width: %d\n", action.width);
    
    return 0;
}

/**
 * Test CLEAR action
 */
int test_send_clear(int sockfd) {
    printf("\n=== TEST SEND CLEAR ===\n");
    
    draw_action_t clear_action;
    drawing_create_clear_action(&clear_action);
    
    uint8_t payload[14];
    drawing_serialize_action(&clear_action, payload);
    
    if (send_message(sockfd, MSG_DRAW_DATA, payload, 14) < 0) {
        return -1;
    }
    
    printf("✓ Da gui CLEAR action thanh cong\n");
    return 0;
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    printf("========================================\n");
    printf("  DRAWING PROTOCOL TEST CLIENT\n");
    printf("========================================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <mode> [args...]\n\n", argv[0]);
        printf("Modes:\n");
        printf("  send <username> <password> <action> <x1> <y1> <x2> <y2> <color> <width>\n");
        printf("       - Gui DRAW_DATA (action: 0=MOVE, 1=LINE, 2=CLEAR)\n");
        printf("       - Vi du: %s send user1 pass123 1 100 100 200 200 0xFF0000FF 5\n\n", argv[0]);
        printf("  receive <username> <password> [timeout]\n");
        printf("       - Dang nhap va doi nhan DRAW_BROADCAST\n");
        printf("       - Vi du: %s receive user1 pass123 10\n\n", argv[0]);
        printf("  clear <username> <password>\n");
        printf("       - Gui CLEAR action\n");
        printf("       - Vi du: %s clear user1 pass123\n\n", argv[0]);
        return 1;
    }
    
    // Ket noi den server
    int sockfd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sockfd < 0) {
        return 1;
    }
    
    int result = 0;
    
    if (strcmp(argv[1], "send") == 0) {
        if (argc < 12) {
            printf("✗ Loi: Thieu tham so\n");
            printf("Usage: %s send <username> <password> <action> <x1> <y1> <x2> <y2> <color> <width>\n", argv[0]);
            result = 1;
        } else {
            // Dang nhap
            int user_id = test_login(sockfd, argv[2], argv[3]);
            if (user_id <= 0) {
                result = 1;
            } else {
                // Parse arguments
                uint8_t action = (uint8_t)atoi(argv[4]);
                uint16_t x1 = (uint16_t)atoi(argv[5]);
                uint16_t y1 = (uint16_t)atoi(argv[6]);
                uint16_t x2 = (uint16_t)atoi(argv[7]);
                uint16_t y2 = (uint16_t)atoi(argv[8]);
                uint32_t color = (uint32_t)strtoul(argv[9], NULL, 16);  // Hex format
                uint8_t width = (uint8_t)atoi(argv[10]);
                
                // Gui DRAW_DATA
                result = test_send_draw_data(sockfd, action, x1, y1, x2, y2, color, width);
            }
        }
    } else if (strcmp(argv[1], "receive") == 0) {
        if (argc < 4) {
            printf("✗ Loi: Thieu tham so\n");
            printf("Usage: %s receive <username> <password> [timeout]\n", argv[0]);
            result = 1;
        } else {
            // Dang nhap
            int user_id = test_login(sockfd, argv[2], argv[3]);
            if (user_id <= 0) {
                result = 1;
            } else {
                int timeout = (argc >= 5) ? atoi(argv[4]) : 10;
                printf("\n✓ Da dang nhap. Dang doi DRAW_BROADCAST...\n");
                printf("(Luu y: Can co client khac gui DRAW_DATA de nhan duoc broadcast)\n\n");
                
                // Doi nhan DRAW_BROADCAST
                result = test_receive_draw_broadcast(sockfd, timeout);
            }
        }
    } else if (strcmp(argv[1], "clear") == 0) {
        if (argc < 4) {
            printf("✗ Loi: Thieu tham so\n");
            printf("Usage: %s clear <username> <password>\n", argv[0]);
            result = 1;
        } else {
            // Dang nhap
            int user_id = test_login(sockfd, argv[2], argv[3]);
            if (user_id <= 0) {
                result = 1;
            } else {
                // Gui CLEAR
                result = test_send_clear(sockfd);
            }
        }
    } else {
        printf("✗ Loi: Mode khong hop le: %s\n", argv[1]);
        result = 1;
    }
    
    close(sockfd);
    printf("\n========================================\n");
    if (result == 0) {
        printf("✓ Test hoan thanh thanh cong!\n");
    } else {
        printf("✗ Test that bai!\n");
    }
    printf("========================================\n");
    
    return result;
}

