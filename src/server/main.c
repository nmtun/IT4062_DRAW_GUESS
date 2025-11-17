#include "../include/server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

server_t server;

// Xử lý tín hiệu để dừng server một cách an toàn
void signal_handler(int sig) {
    printf("\nNhận tín hiệu dừng, đang đóng server...\n");
    server_cleanup(&server);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    
    // Đọc port từ tham số dòng lệnh nếu có
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Port không hợp lệ. Sử dụng port mặc định: %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    // Đăng ký xử lý tín hiệu
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Khởi tạo server
    if (server_init(&server, port) < 0) {
        fprintf(stderr, "Không thể khởi tạo server\n");
        return 1;
    }
    
    // Bắt đầu lắng nghe
    if (server_listen(&server) < 0) {
        fprintf(stderr, "Không thể bắt đầu lắng nghe\n");
        server_cleanup(&server);
        return 1;
    }
    
    // Bắt đầu vòng lặp sự kiện
    server_event_loop(&server);
    
    // Dọn dẹp (thường không đến đây vì vòng lặp vô hạn)
    server_cleanup(&server);
    
    return 0;
}

