#include "../include/server.h"
#include "../include/protocol.h"
#include "../include/room.h"
#include "../include/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

// Khoi tao server
int server_init(server_t *server, int port) {
    // Khoi tao cau truc server
    memset(server, 0, sizeof(server_t));
    server->port = port;
    server->client_count = 0;
    server->max_fd = 0;

    // Tao socket
    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0) {
        perror("socket() failed");
        return -1;
    }

    // Thiet lap tuy chon socket de tai su dung dia chi
    // Khi chay test thuong dung Ctrl + C de ngat server. 
    // Neu muon chay lai ngay ma khong bi loi server already in use thi can su dung option nay
    
    int opt = 1;
    if (setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt() failed");
        close(server->socket_fd);
        return -1;
    }

    // Cau hinh dia chi
    memset(&server->address, 0, sizeof(server->address));
    server->address.sin_family = AF_INET;
    server->address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server->address.sin_port = htons(port);

    // Bind socket
    if (bind(server->socket_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0) {
        perror("bind() failed");
        close(server->socket_fd);
        return -1;
    }

    return 0;
}

// Bat dau lang nghe ket noi
int server_listen(server_t *server) {
    // Set hang doi ket noi toi da la 5 de tranh qua tai
    if (listen(server->socket_fd, 5) < 0) {
        perror("listen() failed");
        return -1;
    }

    printf("Server dang lang nghe tren port %d\n", server->port);
    return 0;
}

// Them client vao danh sach
int server_add_client(server_t *server, int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        // Neu client thu i chua duoc su dung thi them vao danh sach
        if (!server->clients[i].active) {
            server->clients[i].fd = client_fd;
            server->clients[i].active = 1;
            server->clients[i].user_id = -1;
            server->clients[i].username[0] = '\0';
            server->clients[i].state = CLIENT_STATE_LOGGED_OUT;
            server->client_count++;
            
            // Cap nhat max_fd moi neu can de select() hoat dong dung
            if (client_fd > server->max_fd) {
                server->max_fd = client_fd;
            }
            
            printf("Client moi ket noi (fd: %d, index: %d)\n", client_fd, i);
            return i;
        }
    }
    
    printf("Da dat so luong client toi da\n");
    close(client_fd);
    return -1;
}

// Xoa client khoi danh sach
void server_remove_client(server_t *server, int client_index) {
    if (client_index >= 0 && client_index < MAX_CLIENTS && server->clients[client_index].active) {
        close(server->clients[client_index].fd);
        server->clients[client_index].active = 0;
        server->clients[client_index].fd = -1;
        server->clients[client_index].user_id = -1;
        server->clients[client_index].username[0] = '\0';
        server->clients[client_index].state = CLIENT_STATE_LOGGED_OUT;
        server->client_count--;
        printf("Client da ngat ket noi (index: %d)\n", client_index);
    }
}

// Chap nhan ket noi moi
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
    printf("Ket noi moi tu %s:%d\n", client_ip, ntohs(client_addr.sin_port));

    return server_add_client(server, client_fd);
}

// Xu ly du lieu tu client
void server_handle_client_data(server_t *server, int client_index) {
    uint8_t buffer[BUFFER_SIZE];
    int client_fd = server->clients[client_index].fd;
    
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        // Loi hoac ket noi dong
        server_handle_disconnect(server, client_index);
        return;
    }
    
    // Parse message
    message_t msg;
    if (protocol_parse_message(buffer, bytes_read, &msg) == 0) {
        // Xu ly message
        protocol_handle_message(server, client_index, &msg);
        
        // Giai phong payload
        if (msg.payload) {
            free(msg.payload);
        }
    } else {
        fprintf(stderr, "Loi parse message tu client %d\n", client_index);
    }
}

/**
 * Tim room ma client dang o trong
 */
room_t* server_find_room_by_user(server_t* server, int user_id) {
    if (!server || user_id <= 0) {
        return NULL;
    }

    // Duyet qua tat ca rooms
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
 * Broadcast message den tat ca clients trong phong
 */
int server_broadcast_to_room(server_t* server, int room_id, uint8_t msg_type, 
                             const uint8_t* payload, uint16_t payload_len, 
                             int exclude_user_id) {
    if (!server || room_id <= 0) {
        return -1;
    }

    // Tim room
    room_t* room = NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (server->rooms[i] != NULL && server->rooms[i]->room_id == room_id) {
            room = server->rooms[i];
            break;
        }
    }

    if (!room) {
        fprintf(stderr, "Khong tim thay phong voi room_id=%d\n", room_id);
        return -1;
    }

    // Gui message den tat ca clients trong phong
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];
        
        // Bo qua client khong active
        if (!client->active) {
            continue;
        }

        // Bo qua client bi loai tru
        if (exclude_user_id > 0 && client->user_id == exclude_user_id) {
            continue;
        }

        // Kiem tra client co trong phong khong
        if (room_has_player(room, client->user_id)) {
            if (protocol_send_message(client->fd, msg_type, payload, payload_len) == 0) {
                sent_count++;
            }
        }
    }

    printf("Da broadcast message type 0x%02X den phong '%s' (ID: %d) - %d clients nhan duoc\n",
           msg_type, room->room_name, room_id, sent_count);
    
    return sent_count;
}

// Xu ly ngat ket noi
void server_handle_disconnect(server_t *server, int client_index) {
    if (client_index < 0 || client_index >= MAX_CLIENTS) {
        return;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return;
    }

    // Neu client dang trong phong, xoa khoi phong
    if (client->user_id > 0 && 
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {
        
        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            printf("Client %d (user_id=%d) dang disconnect, xoa khoi phong '%s' (ID: %d)\n",
                   client_index, client->user_id, room->room_name, room->room_id);

            // Neu dang choi va nguoi roi la drawer -> end round ngay de game khong bi ket
            int was_playing = (room->state == ROOM_PLAYING && room->game != NULL);
            int was_drawer = (was_playing && room->game->drawer_id == client->user_id);
            char word_before[64];
            word_before[0] = '\0';
            if (was_playing) {
                strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
                word_before[sizeof(word_before) - 1] = '\0';
            }
            
            // Xoa player khoi phong
            room_remove_player(room, client->user_id);
            
            // Luu thong tin phong truoc khi co the destroy
            char room_name[ROOM_NAME_MAX_LENGTH];
            int room_id = room->room_id;
            strncpy(room_name, room->room_name, ROOM_NAME_MAX_LENGTH - 1);
            room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';
            
            // Luu thong tin user dang disconnect
            int leaving_user_id = client->user_id;
            char leaving_username[32];
            strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
            leaving_username[sizeof(leaving_username) - 1] = '\0';
            
            // Neu phong trong, xoa phong
            if (room->player_count == 0) {
                // Tim va xoa phong khoi server
                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (server->rooms[i] == room) {
                        server->rooms[i] = NULL;
                        server->room_count--;
                        break;
                    }
                }
                room_destroy(room);
                printf("Phong '%s' (ID: %d) da bi xoa vi khong con nguoi choi\n",
                       room_name, room_id);
                
                // Broadcast danh sach phong cho tat ca clients da dang nhap
                protocol_broadcast_room_list(server);
            } else {
                // Broadcast ROOM_PLAYERS_UPDATE voi danh sach day du (da bao gom tat ca thong tin phong)
                protocol_broadcast_room_players_update(server, room, 1, // 1 = LEAVE
                                                      leaving_user_id, leaving_username, 
                                                      -1);
            }

            // Xu ly drawer roi phong trong game (sau khi da broadcast danh sach players)
            if (was_drawer && room->game && word_before[0] != '\0') {
                protocol_handle_round_timeout(server, room, word_before);
            }
        }
    }

    // Xoa client khoi danh sach
    server_remove_client(server, client_index);
}

// Xu ly vong lap su kien voi select()
void server_event_loop(server_t *server) {
    while (1) {
        // Khoi tao tap hop file descriptor
        FD_ZERO(&server->read_fds);
        
        // Them server socket vao tap hop
        FD_SET(server->socket_fd, &server->read_fds);
        server->max_fd = server->socket_fd;
        
        // Them tat ca client sockets vao tap hop
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].fd, &server->read_fds);
                if (server->clients[i].fd > server->max_fd) {
                    server->max_fd = server->clients[i].fd;
                }
            }
        }
        
        // Su dung select() de cho su kien
        // Co timeout de tick game timeout (Phase 5 - #19)
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int activity = select(server->max_fd + 1, &server->read_fds, NULL, NULL, &tv);
        
        if (activity < 0) {
            perror("select() failed");
            break;
        }
        
        // Kiem tra ket noi moi tu server socket
        if (FD_ISSET(server->socket_fd, &server->read_fds)) {
            server_accept_client(server);
        }
        
        // Kiem tra du lieu tu cac client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active && FD_ISSET(server->clients[i].fd, &server->read_fds)) {
                server_handle_client_data(server, i);
            }
        }

        // Tick: kiem tra timeout cho tat ca phong dang choi
        for (int r = 0; r < MAX_ROOMS; r++) {
            room_t* room = server->rooms[r];
            if (!room || room->state != ROOM_PLAYING || !room->game) continue;

            // giu lai word truoc khi game_end_round reset state
            char word_before[64];
            memset(word_before, 0, sizeof(word_before));
            strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);

            if (word_before[0] == '\0') continue;

            if (game_check_timeout(room->game)) {
                // broadcast round_end + next round/game end
                protocol_handle_round_timeout(server, room, word_before);
            }
        }
    }
}

// Don dep va dong server
void server_cleanup(server_t *server) {
    // Dong tat ca client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            close(server->clients[i].fd);
        }
    }
    
    // Dong server socket
    if (server->socket_fd >= 0) {
        close(server->socket_fd);
    }
    
    printf("Server da dong\n");
}

