#include "../include/server.h"
#include "../include/database.h"
#include "../include/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

server_t server;
db_connection_t* db = NULL;

// Xử lý tín hiệu để dừng server một cách an toàn
void signal_handler(int sig) {
    printf("\nNhận tín hiệu dừng, đang đóng server...\n");
    if (db) {
        db_disconnect(db);
        db = NULL;
    }
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
    
    // Kết nối đến database
    // NOTE: pass the hostname (here localhost) as the first argument.
    // The code in database.c currently uses port 3308 when calling mysql_real_connect
    // so we connect to 127.0.0.1 (host) and port 3308 will be used inside db_connect.
    db = db_connect("127.0.0.1", "root", "123456", "draw_guess");
    if (!db) {
        fprintf(stderr, "Không thể kết nối đến database. Server vẫn sẽ chạy nhưng không có database.\n");
        // Tiếp tục chạy server dù không có database
    }
    
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
    
    //Test authentication module
    if (db) {
        
        // Test đăng ký
        char hash[65];
        auth_hash_password("mypass123", hash);
        int user_id = db_register_user(db, "demo_user", hash);
        if(user_id > 0) {
            printf("Đăng ký thành công: ID=%d\n", user_id);
        } else {
            printf("Đăng ký thất bại\n");
        }
        
        // Test đăng nhập đúng
        auth_hash_password("mypass123", hash);
        db_authenticate_user(db, "demo_user", hash); 
        
        // Test đăng nhập sai
        auth_hash_password("wrongpass", hash);
        int wrong_id = db_authenticate_user(db, "demo_user", hash);
        printf("Đăng nhập thất bại, ID trả về: %d\n", wrong_id);
        
    }

    // Bắt đầu vòng lặp sự kiện
    server_event_loop(&server);
    
    // Dọn dẹp (thường không đến đây vì vòng lặp vô hạn)
    if (db) {
        db_disconnect(db);
        db = NULL;
    }
    server_cleanup(&server);
    
    return 0;
}

