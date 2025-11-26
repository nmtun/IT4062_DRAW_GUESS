#include "../include/server.h"
#include "../include/database.h"
#include "../include/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>

// ============================================
// CONSTANTS
// ============================================

#define MIN_PORT 1
#define MAX_PORT 65535

// Database configuration (should be moved to config file in production)
#define DB_HOST "127.0.0.1"
#define DB_USER "root"
#define DB_PASSWORD "123456"
#define DB_NAME "draw_guess"

// ============================================
// GLOBAL VARIABLES
// ============================================

static server_t server;
static db_connection_t* db = NULL;

// ============================================
// FORWARD DECLARATIONS
// ============================================

static int parse_port_argument(int argc, char *argv[]);
static int setup_signal_handlers(void);
static int connect_to_database(void);
static int initialize_server(int port);
static void cleanup_resources(void);
static void run_test_authentication(void);

// ============================================
// SIGNAL HANDLER
// ============================================

/**
 * Xử lý tín hiệu để dừng server một cách an toàn
 * @param sig Signal number
 */
void signal_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    printf("\nNhận tín hiệu dừng, đang đóng server...\n");
    cleanup_resources();
    exit(0);
}

// ============================================
// HELPER FUNCTIONS
// ============================================

/**
 * Parse port từ command line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @return Port number, hoặc DEFAULT_PORT nếu không hợp lệ
 */
static int parse_port_argument(int argc, char *argv[]) {
    if (argc <= 1) {
        return DEFAULT_PORT;
    }

    int port = atoi(argv[1]);
    if (port < MIN_PORT || port > MAX_PORT) {
        fprintf(stderr, "Port không hợp lệ (phải từ %d đến %d). "
                "Sử dụng port mặc định: %d\n", 
                MIN_PORT, MAX_PORT, DEFAULT_PORT);
        return DEFAULT_PORT;
    }

    return port;
}

/**
 * Đăng ký signal handlers cho graceful shutdown
 * @return 0 nếu thành công, -1 nếu lỗi
 */
static int setup_signal_handlers(void) {
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Lỗi: Không thể đăng ký handler cho SIGINT\n");
        return -1;
    }

    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Lỗi: Không thể đăng ký handler cho SIGTERM\n");
        return -1;
    }

    return 0;
}

/**
 * Kết nối đến database
 * @return 0 nếu thành công, -1 nếu lỗi (nhưng server vẫn tiếp tục chạy)
 */
static int connect_to_database(void) {
    printf("Đang kết nối đến database...\n");
    db = db_connect(DB_HOST, DB_USER, DB_PASSWORD, DB_NAME);
    
    if (!db) {
        fprintf(stderr, "Cảnh báo: Không thể kết nối đến database. "
                "Server vẫn sẽ chạy nhưng không có database.\n");
        return -1;
    }

    printf("✅ Đã kết nối đến database thành công\n");
    return 0;
}

/**
 * Khởi tạo server
 * @param port Port number để lắng nghe
 * @return 0 nếu thành công, -1 nếu lỗi
 */
static int initialize_server(int port) {
    printf("Đang khởi tạo server trên port %d...\n", port);
    
    if (server_init(&server, port) < 0) {
        fprintf(stderr, "Lỗi: Không thể khởi tạo server\n");
        return -1;
    }

    if (server_listen(&server) < 0) {
        fprintf(stderr, "Lỗi: Không thể bắt đầu lắng nghe trên port %d\n", port);
        server_cleanup(&server);
        return -1;
    }

    printf("✅ Server đã sẵn sàng, đang lắng nghe trên port %d\n", port);
    return 0;
}

/**
 * Dọn dẹp tài nguyên (database connection, server)
 */
static void cleanup_resources(void) {
    if (db) {
        printf("Đang đóng kết nối database...\n");
        db_disconnect(db);
        db = NULL;
    }

    printf("Đang dọn dẹp server...\n");
    server_cleanup(&server);
}

/**
 * Chạy test authentication module (chỉ dùng trong development)
 * TODO: Tách ra file test riêng hoặc xóa trong production
 */
static void run_test_authentication(void) {
#ifdef ENABLE_AUTH_TEST
    if (!db) {
        return;
    }

    printf("\n=== Chạy test authentication module ===\n");

    // Test đăng ký
    char hash[65];
    if (auth_hash_password("mypass123", hash) != 0) {
        fprintf(stderr, "Lỗi: Không thể hash password\n");
        return;
    }

    int user_id = db_register_user(db, "demo_user", hash);
    if (user_id > 0) {
        printf("✅ Test đăng ký thành công: ID=%d\n", user_id);
    } else {
        printf("❌ Test đăng ký thất bại\n");
    }

    // Test đăng nhập đúng
    if (auth_hash_password("mypass123", hash) != 0) {
        fprintf(stderr, "Lỗi: Không thể hash password\n");
        return;
    }
    int correct_id = db_authenticate_user(db, "demo_user", hash);
    if (correct_id > 0) {
        printf("✅ Test đăng nhập đúng thành công: ID=%d\n", correct_id);
    } else {
        printf("❌ Test đăng nhập đúng thất bại\n");
    }

    // Test đăng nhập sai
    if (auth_hash_password("wrongpass", hash) != 0) {
        fprintf(stderr, "Lỗi: Không thể hash password\n");
        return;
    }
    int wrong_id = db_authenticate_user(db, "demo_user", hash);
    if (wrong_id <= 0) {
        printf("✅ Test đăng nhập sai thành công (ID=%d, như mong đợi)\n", wrong_id);
    } else {
        printf("❌ Test đăng nhập sai thất bại (ID=%d, không đúng)\n", wrong_id);
    }

    printf("=== Kết thúc test ===\n\n");
#endif
}

// ============================================
// MAIN FUNCTION
// ============================================

/**
 * Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 nếu thành công, 1 nếu lỗi
 */
int main(int argc, char *argv[]) {
    // Parse port từ command line
    int port = parse_port_argument(argc, argv);

    // Setup signal handlers
    if (setup_signal_handlers() < 0) {
        fprintf(stderr, "Lỗi: Không thể setup signal handlers\n");
        return 1;
    }

    // Kết nối đến database (không bắt buộc, server vẫn chạy được)
    connect_to_database();

    // Khởi tạo server
    if (initialize_server(port) < 0) {
        cleanup_resources();
        return 1;
    }

    // Chạy test authentication (chỉ trong development)
    run_test_authentication();

    // Bắt đầu vòng lặp sự kiện (blocking call)
    printf("Server đang chạy... (Nhấn Ctrl+C để dừng)\n");
    server_event_loop(&server);

    // Dọn dẹp (thường không đến đây vì vòng lặp vô hạn)
    cleanup_resources();

    return 0;
}
