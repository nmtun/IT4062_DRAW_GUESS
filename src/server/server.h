#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080

// Cấu trúc client
typedef struct {
    int fd;
    int active;
} client_t;

// Cấu trúc server
typedef struct {
    int socket_fd;
    int port;
    struct sockaddr_in address;
    client_t clients[MAX_CLIENTS];
    int client_count;
    fd_set read_fds;
    int max_fd;
} server_t;

// Khởi tạo server
int server_init(server_t *server, int port);

// Bắt đầu lắng nghe kết nối
int server_listen(server_t *server);

// Chấp nhận kết nối mới
int server_accept_client(server_t *server);

// Xử lý vòng lặp sự kiện với select()
void server_event_loop(server_t *server);

// Xử lý dữ liệu từ client
void server_handle_client_data(server_t *server, int client_index);

// Xử lý ngắt kết nối
void server_handle_disconnect(server_t *server, int client_index);

// Thêm client vào danh sách
int server_add_client(server_t *server, int client_fd);

// Xóa client khỏi danh sách
void server_remove_client(server_t *server, int client_index);

// Dọn dẹp và đóng server
void server_cleanup(server_t *server);

#endif // SERVER_H

