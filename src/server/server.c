#include "server.h"
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
    char buffer[BUFFER_SIZE];
    int client_fd = server->clients[client_index].fd;
    
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_read <= 0) {
        // Lỗi hoặc kết nối đóng
        server_handle_disconnect(server, client_index);
        return;
    }
    
    buffer[bytes_read] = '\0';
    printf("Nhận dữ liệu từ client %d: %s\n", client_index, buffer);
    
    // TODO: Xử lý dữ liệu theo protocol, cần hoàn thiện thêm các message type khác
    // Ở đây chỉ echo lại cho client
    send(client_fd, buffer, bytes_read, 0);
}

// Xử lý ngắt kết nối
void server_handle_disconnect(server_t *server, int client_index) {
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

