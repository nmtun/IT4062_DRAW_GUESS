#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include "room.h"

#define MAX_CLIENTS 100
#define MAX_ROOMS 50
#define BUFFER_SIZE 1024
#define DEFAULT_PORT 8080

// Trạng thái client
typedef enum {
    CLIENT_STATE_LOGGED_OUT = 0,
    CLIENT_STATE_LOGGED_IN = 1,
    CLIENT_STATE_IN_ROOM = 2,
    CLIENT_STATE_IN_GAME = 3
} client_state_t;

// Cấu trúc client
typedef struct {
    int fd;
    int active;
    int user_id;                    // -1 nếu chưa đăng nhập
    char username[32];              // Username (null-terminated)
    client_state_t state;           // Trạng thái hiện tại
} client_t;

// Cấu trúc server
typedef struct server {
    int socket_fd;
    int port;
    struct sockaddr_in address;
    client_t clients[MAX_CLIENTS];
    int client_count;
    room_t* rooms[MAX_ROOMS];       // Mảng con trỏ đến các phòng
    int room_count;                  // Số phòng hiện tại
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

/**
 * Tìm room mà client đang ở trong
 * @param server Con trỏ đến server_t
 * @param user_id User ID của client
 * @return Con trỏ đến room_t nếu tìm thấy, NULL nếu không
 */
room_t* server_find_room_by_user(server_t* server, int user_id);

/**
 * Broadcast message đến tất cả clients trong phòng
 * @param server Con trỏ đến server_t
 * @param room_id Room ID cần broadcast
 * @param msg_type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @param exclude_user_id User ID cần loại trừ (hoặc -1 nếu không loại trừ)
 * @return Số lượng clients đã nhận được message, -1 nếu lỗi
 */
int server_broadcast_to_room(server_t* server, int room_id, uint8_t msg_type, 
                             const uint8_t* payload, uint16_t payload_len, 
                             int exclude_user_id);

#endif // SERVER_H

