#include "../include/server.h"
#include "../include/protocol.h"
#include "../include/room.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

// Khởi tạo server
int server_init(server_t *server, int port) {
    // Khởi tạo cấu trúc server
    memset(server, 0, sizeof(server_t));
    server->port = port;
    server->client_count = 0;
    server->max_fd = 0;

    // Tạo socket
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        perror("socket() failed");
        return -1;
    }

    // Thiết lập tùy chọn socket để tái sử dụng địa chỉ
    // Khi chạy test thường dùng Ctrl + C để ngắt server. 
    // Nếu muốn chạy lại ngay mà không bị lỗi server already in use thì cần sử dụng option này
    
    int opt = 1;
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        close(server->socket_fd);
        return -1;
    }

    // Cấu hình địa chỉ
    memset(&server->address, 0, sizeof(server->address));
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = INADDR_ANY;
    server->address.sin_port = htons(port);

    // Bind socket
    if (bind(server->socket_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0) {
        perror("bind() failed");
        close(server->socket_fd);
        return -1;
    }

    return 0;
}

// Bắt đầu lắng nghe kết nối
int server_listen(server_t *server) {
    //Set hàng đợi kết nối tối đa là 5 để tránh quá tải
    if (listen(server->socket_fd, 5) < 0) {
        perror("listen() failed");
        return -1;
    }

    printf("Server đang lắng nghe trên port %d\n", server->port);
    return 0;
}

// Thêm client vào danh sách
int server_add_client(server_t *server, int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        //Nếu client thứ i chưa được sử dụng thì thêm vào danh sách
        if (!server->clients[i].active) {
            server->clients[i].fd = client_fd;
            server->clients[i].active = 1;
            server->clients[i].user_id = -1;
            server->clients[i].username[0] = '\0';
            server->clients[i].state = CLIENT_STATE_LOGGED_OUT;
            server->client_count++;
            
            //Cập nhật max_fd mới nếu cần để select() hoạt động đúng
            if (client_fd > server->max_fd) {
                server->max_fd = client_fd;
            }
            
            printf("Client mới kết nối (fd: %d, index: %d)\n", client_fd, i);
            return i;
        }
    }
    
    printf("Đã đạt số lượng client tối đa\n");
    close(client_fd);
    return -1;
}

// Xóa client khỏi danh sách
void server_remove_client(server_t *server, int client_index) {
    if (client_index >= 0 && client_index < MAX_CLIENTS && server->clients[client_index].active) {
        close(server->clients[client_index].fd);
        server->clients[client_index].active = 0;
        server->clients[client_index].fd = -1;
        server->clients[client_index].user_id = -1;
        server->clients[client_index].username[0] = '\0';
        server->clients[client_index].state = CLIENT_STATE_LOGGED_OUT;
        server->client_count--;
        printf("Client đã ngắt kết nối (index: %d)\n", client_index);
    }
}

// Chấp nhận kết nối mới
int server_accept_client(server_t *server) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept() failed");
        return -1;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Kết nối mới từ %s:%d\n", client_ip, ntohs(client_addr.sin_port));

    return server_add_client(server, client_fd);
}

// Xử lý dữ liệu từ client
void server_handle_client_data(server_t *server, int client_index) {
    uint8_t buffer[BUFFER_SIZE];
    int client_fd = server->clients[client_index].fd;
    
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        // Lỗi hoặc kết nối đóng
        server_handle_disconnect(server, client_index);
        return;
    }
    
    // Parse message
    message_t msg;
    if (protocol_parse_message(buffer, bytes_read, &msg) == 0) {
        // Xử lý message
        protocol_handle_message(server, client_index, &msg);
        
        // Giải phóng payload
        if (msg.payload) {
            free(msg.payload);
        }
    } else {
        fprintf(stderr, "Lỗi parse message từ client %d\n", client_index);
    }
}

/**
 * Tìm room mà client đang ở trong
 */
room_t* server_find_room_by_user(server_t* server, int user_id) {
    if (!server || user_id <= 0) {
        return NULL;
    }

    // Duyệt qua tất cả rooms
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (server->rooms[i] != NULL) {
            if (room_has_player(server->rooms[i], user_id)) {
                return server->rooms[i];
            }
        }
    }

    return NULL;
}

/**
 * Broadcast message đến tất cả clients trong phòng
 */
int server_broadcast_to_room(server_t* server, int room_id, uint8_t msg_type, 
                             const uint8_t* payload, uint16_t payload_len, 
                             int exclude_user_id) {
    if (!server || room_id <= 0) {
        return -1;
    }

    // Tìm room
    room_t* room = NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (server->rooms[i] != NULL && server->rooms[i]->room_id == room_id) {
            room = server->rooms[i];
            break;
        }
    }

    if (!room) {
        fprintf(stderr, "Không tìm thấy phòng với room_id=%d\n", room_id);
        return -1;
    }

    // Gửi message đến tất cả clients trong phòng
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];
        
        // Bỏ qua client không active
        if (!client->active) {
            continue;
        }

        // Bỏ qua client bị loại trừ
        if (exclude_user_id > 0 && client->user_id == exclude_user_id) {
            continue;
        }

        // Kiểm tra client có trong phòng không
        if (room_has_player(room, client->user_id)) {
            if (protocol_send_message(client->fd, msg_type, payload, payload_len) == 0) {
                sent_count++;
            }
        }
    }

    printf("Đã broadcast message type 0x%02X đến phòng '%s' (ID: %d) - %d clients nhận được\n",
           msg_type, room->room_name, room_id, sent_count);
    
    return sent_count;
}

// Xử lý ngắt kết nối
void server_handle_disconnect(server_t *server, int client_index) {
    if (client_index < 0 || client_index >= MAX_CLIENTS) {
        return;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return;
    }

    // Nếu client đang trong phòng, xóa khỏi phòng
    if (client->user_id > 0 && 
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {
        
        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            printf("Client %d (user_id=%d) đang disconnect, xóa khỏi phòng '%s' (ID: %d)\n",
                   client_index, client->user_id, room->room_name, room->room_id);
            
            // Xóa player khỏi phòng
            room_remove_player(room, client->user_id);
            
            // Lưu thông tin phòng trước khi có thể destroy
            char room_name[ROOM_NAME_MAX_LENGTH];
            int room_id = room->room_id;
            strncpy(room_name, room->room_name, ROOM_NAME_MAX_LENGTH - 1);
            room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';
            
            // Lưu thông tin user đang disconnect
            int leaving_user_id = client->user_id;
            char leaving_username[32];
            strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
            leaving_username[sizeof(leaving_username) - 1] = '\0';
            
            // Nếu phòng trống, xóa phòng
            if (room->player_count == 0) {
                // Tìm và xóa phòng khỏi server
                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (server->rooms[i] == room) {
                        server->rooms[i] = NULL;
                        server->room_count--;
                        break;
                    }
                }
                room_destroy(room);
                printf("Phòng '%s' (ID: %d) đã bị xóa vì không còn người chơi\n",
                       room_name, room_id);
                
                // Broadcast danh sách phòng cho tất cả clients đã đăng nhập
                protocol_broadcast_room_list(server);
            } else {
                // Broadcast ROOM_PLAYERS_UPDATE với danh sách đầy đủ (đã bao gồm tất cả thông tin phòng)
                protocol_broadcast_room_players_update(server, room, 1, // 1 = LEAVE
                                                      leaving_user_id, leaving_username, 
                                                      -1);
            }
        }
    }

    // Xóa client khỏi danh sách
    server_remove_client(server, client_index);
}

// Xử lý vòng lặp sự kiện với select()
void server_event_loop(server_t *server) {
    while (1) {
        // Khởi tạo tập hợp file descriptor
        FD_ZERO(&server->read_fds);
        
        // Thêm server socket vào tập hợp
        FD_SET(server->socket_fd, &server->read_fds);
        server->max_fd = server->socket_fd;
        
        // Thêm tất cả client sockets vào tập hợp
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].fd, &server->read_fds);
                if (server->clients[i].fd > server->max_fd) {
                    server->max_fd = server->clients[i].fd;
                }
            }
        }
        
        // Sử dụng select() để chờ sự kiện
        int activity = select(server->max_fd + 1, &server->read_fds, NULL, NULL, NULL);
        
        if (activity < 0) {
            perror("select() failed");
            break;
        }
        
        // Kiểm tra kết nối mới từ server socket
        if (FD_ISSET(server->socket_fd, &server->read_fds)) {
            server_accept_client(server);
        }
        
        // Kiểm tra dữ liệu từ các client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active && FD_ISSET(server->clients[i].fd, &server->read_fds)) {
                server_handle_client_data(server, i);
            }
        }
    }
}

// Dọn dẹp và đóng server
void server_cleanup(server_t *server) {
    // Đóng tất cả client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            close(server->clients[i].fd);
        }
    }
    
    // Đóng server socket
    if (server->socket_fd >= 0) {
        close(server->socket_fd);
    }
    
    printf("Server đã đóng\n");
}

