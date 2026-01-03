#include "../include/protocol.h"
#include "../include/room.h"
#include "../include/game.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * Gui CREATE_ROOM_RESPONSE
 */
int protocol_send_create_room_response(int client_fd, uint8_t status, int32_t room_id, const char *message)
{
    create_room_response_t response;
    memset(&response, 0, sizeof(response));

    response.status = status;
    response.room_id = (int32_t)htonl((uint32_t)room_id); // Convert to network byte order

    if (message)
    {
        strncpy(response.message, message, sizeof(response.message) - 1);
        response.message[sizeof(response.message) - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_CREATE_ROOM,
                                 (uint8_t *)&response, sizeof(response));
}

/**
 * Gui JOIN_ROOM_RESPONSE
 */
int protocol_send_join_room_response(int client_fd, uint8_t status, int32_t room_id, const char *message)
{
    join_room_response_t response;
    memset(&response, 0, sizeof(response));

    response.status = status;
    response.room_id = (int32_t)htonl((uint32_t)room_id); // Convert to network byte order

    if (message)
    {
        strncpy(response.message, message, sizeof(response.message) - 1);
        response.message[sizeof(response.message) - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_JOIN_ROOM,
                                 (uint8_t *)&response, sizeof(response));
}

/**
 * Gui LEAVE_ROOM_RESPONSE
 */
int protocol_send_leave_room_response(int client_fd, uint8_t status, const char *message)
{
    leave_room_response_t response;
    memset(&response, 0, sizeof(response));

    response.status = status;

    if (message)
    {
        strncpy(response.message, message, sizeof(response.message) - 1);
        response.message[sizeof(response.message) - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_LEAVE_ROOM,
                                 (uint8_t *)&response, sizeof(response));
}

/**
 * Gui ROOM_LIST_RESPONSE den client
 */
int protocol_send_room_list(int client_fd, server_t *server)
{
    if (!server)
    {
        return -1;
    }

    // Lay danh sach phong
    room_info_t room_list[50]; // MAX_ROOMS
    int room_count = room_get_list(server, room_list, 50);

    // Tinh kich thuoc payload
    uint16_t payload_size = sizeof(room_list_response_t) +
                            room_count * sizeof(room_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Loi: Room list qua lon (%d bytes)\n", payload_size);
        return -1;
    }

    // Tao payload
    uint8_t payload[BUFFER_SIZE];
    room_list_response_t *header = (room_list_response_t *)payload;
    header->room_count = htons((uint16_t)room_count);

    // Chuyen doi room_info_t sang room_info_protocol_t
    room_info_protocol_t *room_info_proto = (room_info_protocol_t *)(payload + sizeof(room_list_response_t));
    for (int i = 0; i < room_count; i++)
    {
        room_info_proto[i].room_id = htonl((uint32_t)room_list[i].room_id);
        strncpy(room_info_proto[i].room_name, room_list[i].room_name, MAX_ROOM_NAME_LEN - 1);
        room_info_proto[i].room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
        room_info_proto[i].player_count = room_list[i].player_count;
        room_info_proto[i].max_players = room_list[i].max_players;
        room_info_proto[i].state = (uint8_t)room_list[i].state;
        room_info_proto[i].owner_id = htonl((uint32_t)room_list[i].owner_id);
        
        // Tim username cua chu phong tu server->clients
        strncpy(room_info_proto[i].owner_username, "Unknown", MAX_USERNAME_LEN - 1);
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (server->clients[j].active &&
                server->clients[j].user_id == room_list[i].owner_id)
            {
                strncpy(room_info_proto[i].owner_username, server->clients[j].username, MAX_USERNAME_LEN - 1);
                room_info_proto[i].owner_username[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
        }
    }

    return protocol_send_message(client_fd, MSG_ROOM_LIST_RESPONSE,
                                 payload, payload_size);
}

/**
 * Broadcast ROOM_LIST_RESPONSE den tat ca clients da dang nhap
 * Goi khi co phong moi duoc tao hoac phong bi xoa
 */
int protocol_broadcast_room_list(server_t *server)
{
    if (!server)
    {
        return -1;
    }

    // Lay danh sach phong
    room_info_t room_list[50]; // MAX_ROOMS
    int room_count = room_get_list(server, room_list, 50);

    // Tinh kich thuoc payload
    uint16_t payload_size = sizeof(room_list_response_t) +
                            room_count * sizeof(room_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Loi: Room list qua lon (%d bytes)\n", payload_size);
        return -1;
    }

    // Tao payload
    uint8_t payload[BUFFER_SIZE];
    room_list_response_t *header = (room_list_response_t *)payload;
    header->room_count = htons((uint16_t)room_count);

    // Chuyen doi room_info_t sang room_info_protocol_t
    room_info_protocol_t *room_info_proto = (room_info_protocol_t *)(payload + sizeof(room_list_response_t));
    for (int i = 0; i < room_count; i++)
    {
        room_info_proto[i].room_id = htonl((uint32_t)room_list[i].room_id);
        strncpy(room_info_proto[i].room_name, room_list[i].room_name, MAX_ROOM_NAME_LEN - 1);
        room_info_proto[i].room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
        room_info_proto[i].player_count = room_list[i].player_count;
        room_info_proto[i].max_players = room_list[i].max_players;
        room_info_proto[i].state = (uint8_t)room_list[i].state;
        room_info_proto[i].owner_id = htonl((uint32_t)room_list[i].owner_id);
        
        // Tim username cua chu phong tu server->clients
        strncpy(room_info_proto[i].owner_username, "Unknown", MAX_USERNAME_LEN - 1);
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (server->clients[j].active &&
                server->clients[j].user_id == room_list[i].owner_id)
            {
                strncpy(room_info_proto[i].owner_username, server->clients[j].username, MAX_USERNAME_LEN - 1);
                room_info_proto[i].owner_username[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
        }
    }

    // Gui den tat ca clients da dang nhap (LOGGED_IN tro len)
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *client = &server->clients[i];

        // Chi gui cho clients da dang nhap (khong gui cho clients trong phong)
        if (client->active &&
            client->state >= CLIENT_STATE_LOGGED_IN &&
            client->user_id > 0)
        {
            if (protocol_send_message(client->fd, MSG_ROOM_LIST_RESPONSE,
                                      payload, payload_size) == 0)
            {
                sent_count++;
            }
        }
    }

    printf("Da broadcast ROOM_LIST_RESPONSE den %d clients (tong %d phong)\n",
           sent_count, room_count);

    return sent_count;
}

/**
 * Broadcast ROOM_PLAYERS_UPDATE den tat ca clients trong phong
 * Gui danh sach day du nguoi choi khi co thay doi
 */
int protocol_broadcast_room_players_update(server_t *server, room_t *room,
                                           uint8_t action, int changed_user_id,
                                           const char *changed_username,
                                           int exclude_client_index)
{
    if (!server || !room)
    {
        return -1;
    }

    // Tinh kich thuoc payload
    uint16_t payload_size = sizeof(room_players_update_t) +
                            room->player_count * sizeof(player_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Loi: Room players update qua lon (%d bytes)\n", payload_size);
        return -1;
    }

    // Tao payload
    uint8_t payload[BUFFER_SIZE];
    room_players_update_t *update = (room_players_update_t *)payload;

    // Thong tin phong day du
    update->room_id = htonl((uint32_t)room->room_id);
    strncpy(update->room_name, room->room_name, MAX_ROOM_NAME_LEN - 1);
    update->room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
    update->max_players = (uint8_t)room->max_players;
    update->state = (uint8_t)room->state;
    update->owner_id = htonl((uint32_t)room->owner_id);

    // Thong tin thay doi nguoi choi
    update->action = action; // 0 = JOIN, 1 = LEAVE
    update->changed_user_id = htonl((uint32_t)changed_user_id);
    if (changed_username) {
        strncpy(update->changed_username, changed_username, MAX_USERNAME_LEN - 1);
        update->changed_username[MAX_USERNAME_LEN - 1] = '\0';
    } else {
        // Nếu changed_username là NULL, set thành chuỗi rỗng
        update->changed_username[0] = '\0';
    }
    update->player_count = htons((uint16_t)room->player_count);

    // Lay danh sach nguoi choi hien tai
    player_info_protocol_t *players = (player_info_protocol_t *)(payload + sizeof(room_players_update_t));

    for (int i = 0; i < room->player_count; i++)
    {
        int player_user_id = room->players[i];
        players[i].user_id = htonl((uint32_t)player_user_id);
        players[i].is_owner = (player_user_id == room->owner_id) ? 1 : 0;
        
        // Xác định trạng thái active
        if (room->active_players[i] == -1) {
            // Người chơi đã rời phòng
            players[i].is_active = 255; // 0xFF = đã rời phòng
        } else if (room->active_players[i] == 1) {
            // Người chơi đang active
            players[i].is_active = 1;
        } else {
            // Người chơi đang chờ (join giữa chừng)
            players[i].is_active = 0;
        }

        // Tim username va avatar tu server->clients
        // Nếu người chơi đã rời (active_players[i] == -1), có thể không tìm thấy trong server->clients
        strncpy(players[i].username, "Unknown", MAX_USERNAME_LEN - 1);
        strncpy(players[i].avatar, "avt1.jpg", 32 - 1);  // Default avatar
        players[i].avatar[31] = '\0';
        
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (server->clients[j].active &&
                server->clients[j].user_id == player_user_id)
            {
                strncpy(players[i].username, server->clients[j].username, MAX_USERNAME_LEN - 1);
                players[i].username[MAX_USERNAME_LEN - 1] = '\0';
                strncpy(players[i].avatar, server->clients[j].avatar, 32 - 1);
                players[i].avatar[31] = '\0';
                break;
            }
        }
    }

    // Gui den tat ca clients trong phong
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *client = &server->clients[i];

        if (!client->active || i == exclude_client_index)
        {
            continue;
        }

        if (room_has_player(room, client->user_id))
        {
            if (protocol_send_message(client->fd, MSG_ROOM_PLAYERS_UPDATE,
                                      payload, payload_size) == 0)
            {
                sent_count++;
            }
        }
    }

    const char *action_str = (action == 0) ? "JOIN" : "LEAVE";
    printf("Da gui ROOM_PLAYERS_UPDATE (action=%s, user_id=%d) cho phong '%s' den %d clients\n",
           action_str, changed_user_id, room->room_name, sent_count);

    return (sent_count > 0) ? 0 : -1;
}

/**
 * Broadcast ROOM_UPDATE den tat ca clients trong phong
 */
int protocol_broadcast_room_update(server_t *server, room_t *room, int exclude_client_index)
{
    if (!server || !room)
    {
        return -1;
    }

    // Tao room_info_protocol_t tu room
    room_info_protocol_t room_info;
    room_info.room_id = htonl((uint32_t)room->room_id);
    strncpy(room_info.room_name, room->room_name, MAX_ROOM_NAME_LEN - 1);
    room_info.room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
    room_info.player_count = (uint8_t)room->player_count;
    room_info.max_players = (uint8_t)room->max_players;
    room_info.state = (uint8_t)room->state;
    room_info.owner_id = htonl((uint32_t)room->owner_id);

    // Gui den tat ca clients trong phong
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *client = &server->clients[i];

        // Bo qua client khong active hoac client bi loai tru
        if (!client->active || i == exclude_client_index)
        {
            continue;
        }

        // Kiem tra client co trong phong khong
        if (room_has_player(room, client->user_id))
        {
            if (protocol_send_message(client->fd, MSG_ROOM_UPDATE,
                                      (uint8_t *)&room_info, sizeof(room_info)) == 0)
            {
                sent_count++;
            }
        }
    }

    printf("Da gui ROOM_UPDATE cho phong '%s' (ID: %d) den %d clients\n",
           room->room_name, room->room_id, sent_count);

    return (sent_count > 0) ? 0 : -1;
}

/**
 * Tim room trong server theo room_id
 */
static room_t *protocol_find_room(server_t *server, int room_id)
{
    if (!server || room_id <= 0)
    {
        return NULL;
    }

    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (server->rooms[i] != NULL && server->rooms[i]->room_id == room_id)
        {
            return server->rooms[i];
        }
    }

    return NULL;
}

/**
 * Them room vao server
 */
static int protocol_add_room_to_server(server_t *server, room_t *room)
{
    if (!server || !room)
    {
        return -1;
    }

    // Tim slot trong
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (server->rooms[i] == NULL)
        {
            server->rooms[i] = room;
            server->room_count++;
            printf("Da them phong '%s' (ID: %d) vao server. Tong so phong: %d\n",
                   room->room_name, room->room_id, server->room_count);
            return 0;
        }
    }

    fprintf(stderr, "Loi: Server da day phong (MAX_ROOMS = %d)\n", MAX_ROOMS);
    return -1;
}

/**
 * Xoa room khoi server
 */
static int protocol_remove_room_from_server(server_t *server, room_t *room)
{
    if (!server || !room)
    {
        return -1;
    }

    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (server->rooms[i] == room)
        {
            server->rooms[i] = NULL;
            server->room_count--;
            printf("Da xoa phong '%s' (ID: %d) khoi server. Tong so phong: %d\n",
                   room->room_name, room->room_id, server->room_count);
            return 0;
        }
    }

    return -1;
}

/**
 * Xu ly ROOM_LIST_REQUEST
 */
int protocol_handle_room_list_request(server_t *server, int client_index, const message_t *msg)
{
    (void)msg; // Unused parameter
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS)
    {
        return -1;
    }

    client_t *client = &server->clients[client_index];
    if (!client->active)
    {
        return -1;
    }

    // Kiem tra client da dang nhap chua
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        fprintf(stderr, "Client %d chua dang nhap, khong the lay danh sach phong\n", client_index);
        return -1;
    }

    printf("Nhan ROOM_LIST_REQUEST tu client %d (user_id=%d)\n", client_index, client->user_id);

    return protocol_send_room_list(client->fd, server);
}

/**
 * Xu ly CREATE_ROOM
 */
int protocol_handle_create_room(server_t *server, int client_index, const message_t *msg)
{
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg)
    {
        return -1;
    }

    client_t *client = &server->clients[client_index];
    if (!client->active)
    {
        return -1;
    }

    // Kiem tra client da dang nhap chua
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Ban can dang nhap de tao phong");
        return -1;
    }

    // Kiem tra client da trong phong chua
    if (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Ban dang trong phong khac");
        return -1;
    }

    // Kiem tra payload size
    if (msg->length < sizeof(create_room_request_t))
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Du lieu khong hop le");
        return -1;
    }

    // Parse payload
    create_room_request_t *req = (create_room_request_t *)msg->payload;

    // Dam bao null-terminated
    char room_name[MAX_ROOM_NAME_LEN];
    strncpy(room_name, req->room_name, MAX_ROOM_NAME_LEN - 1);
    room_name[MAX_ROOM_NAME_LEN - 1] = '\0';

    int max_players = (int)req->max_players;
    int rounds = (int)req->rounds;
    
    char difficulty[16];
    strncpy(difficulty, req->difficulty, sizeof(difficulty) - 1);
    difficulty[sizeof(difficulty) - 1] = '\0';
    if (difficulty[0] == '\0') {
        strncpy(difficulty, "easy", sizeof(difficulty) - 1);
    }

    printf("Nhan CREATE_ROOM tu client %d: room_name=%s, max_players=%d, rounds=%d, difficulty=%s\n",
           client_index, room_name, max_players, rounds, difficulty);

    // Validate
    if (strlen(room_name) == 0)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Ten phong khong duoc de trong");
        return -1;
    }

    if (max_players < 2 || max_players > 10)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "So nguoi choi phai tu 2-10");
        return -1;
    }

    if (rounds < 1 || rounds > 10)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "So round phai tu 1-10");
        return -1;
    }

    // Kiem tra server da day phong chua
    if (server->room_count >= MAX_ROOMS)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Server da day phong");
        return -1;
    }

    // Tao phong
    room_t *room = room_create(room_name, client->user_id, max_players, rounds, difficulty);
    if (!room)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Khong the tao phong");
        return -1;
    }

    // Them phong vao server
    if (protocol_add_room_to_server(server, room) != 0)
    {
        room_destroy(room);
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Khong the them phong vao server");
        return -1;
    }

    // Cap nhat trang thai client
    client->state = CLIENT_STATE_IN_ROOM;

    // Gui response thanh cong
    protocol_send_create_room_response(client->fd, STATUS_SUCCESS, room->room_id,
                                       "Tao phong thanh cong");

    printf("Client %d da tao phong '%s' (ID: %d) thanh cong\n",
           client_index, room_name, room->room_id);

    // Broadcast danh sach phong cho tat ca clients da dang nhap
    protocol_broadcast_room_list(server);

    // Gui ROOM_PLAYERS_UPDATE cho chinh phong vua tao
    // Dam bao client tao phong nhin thay minh la owner va danh sach nguoi choi hien tai
    protocol_broadcast_room_players_update(
        server,
        room,
        0, // action = JOIN
        client->user_id,
        client->username,
        -1 // khong loai tru ai; gui cho tat ca trong phong (hien tai chi co owner)
    );

    return 0;
}

/**
 * Xu ly JOIN_ROOM
 */
int protocol_handle_join_room(server_t *server, int client_index, const message_t *msg)
{
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg)
    {
        return -1;
    }

    client_t *client = &server->clients[client_index];
    if (!client->active)
    {
        return -1;
    }

    // Kiem tra client da dang nhap chua
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Ban can dang nhap de tham gia phong");
        return -1;
    }

    // Kiem tra client da trong phong chua
    if (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Ban dang trong phong khac");
        return -1;
    }

    // Kiem tra payload size
    if (msg->length < sizeof(join_room_request_t))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Du lieu khong hop le");
        return -1;
    }

    // Parse payload
    join_room_request_t *req = (join_room_request_t *)msg->payload;
    int room_id = (int)ntohl((uint32_t)req->room_id); // Convert from network byte order

    printf("Nhan JOIN_ROOM tu client %d: room_id=%d\n", client_index, room_id);

    // Tim phong
    room_t *room = protocol_find_room(server, room_id);
    if (!room)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Khong tim thay phong");
        return -1;
    }

    // Kiem tra phong da day chua
    if (room_is_full(room))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Phong da day");
        return -1;
    }

    // Kiem tra nguoi choi da trong phong chua
    if (room_has_player(room, client->user_id))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Ban da trong phong nay");
        return -1;
    }

    // Them nguoi choi vao phong
    if (!room_add_player(room, client->user_id))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Khong the tham gia phong");
        return -1;
    }

    // Cap nhat trang thai client
    client->state = CLIENT_STATE_IN_ROOM;

    // Kiem tra neu phong da dat max players va dang WAITING, tu dong start game
    if (room->player_count >= room->max_players && room->state == ROOM_WAITING) {
        printf("Phong '%s' (ID: %d) da dat max players (%d/%d), tu dong bat dau game\n",
               room->room_name, room->room_id, room->player_count, room->max_players);
        
        if (room_start_game(room)) {
            printf("Game da tu dong bat dau trong phong '%s' (ID: %d)\n",
                   room->room_name, room->room_id);
            
            // Cap nhat trang thai client sang IN_GAME
            client->state = CLIENT_STATE_IN_GAME;
            
            // Cap nhat trang thai tat ca clients trong phong
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (server->clients[i].active && 
                    room_has_player(room, server->clients[i].user_id)) {
                    server->clients[i].state = CLIENT_STATE_IN_GAME;
                }
            }
        } else {
            fprintf(stderr, "Loi: Khong the tu dong bat dau game trong phong %d\n", room->room_id);
        }
    }

    // Gui response thanh cong
    protocol_send_join_room_response(client->fd, STATUS_SUCCESS, room_id,
                                     "Tham gia phong thanh cong");

    // Broadcast ROOM_PLAYERS_UPDATE voi danh sach day du (da bao gom tat ca thong tin phong)
    // Gui cho TAT CA clients trong phong, bao gom ca client vua join
    protocol_broadcast_room_players_update(server, room, 0, // 0 = JOIN
                                           client->user_id, client->username,
                                           -1); // -1 = khong exclude ai

    // Broadcast ROOM_UPDATE de thong bao trang thai phong (co the da chuyen sang PLAYING)
    protocol_broadcast_room_update(server, room, -1);

    printf("Client %d (user_id=%d, username=%s) da tham gia phong '%s' (ID: %d)\n",
           client_index, client->user_id, client->username, room->room_name, room_id);

    return 0;
}

/**
 * Xu ly LEAVE_ROOM
 */
int protocol_handle_leave_room(server_t *server, int client_index, const message_t *msg)
{
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg)
    {
        return -1;
    }

    client_t *client = &server->clients[client_index];
    if (!client->active)
    {
        return -1;
    }

    // Kiem tra client da dang nhap chua
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Ban chua dang nhap");
        return -1;
    }

    // Kiem tra client co trong phong khong
    if (client->state != CLIENT_STATE_IN_ROOM && client->state != CLIENT_STATE_IN_GAME)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Ban khong trong phong nao");
        return -1;
    }

    // Kiem tra payload size
    if (msg->length < sizeof(leave_room_request_t))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Du lieu khong hop le");
        return -1;
    }

    // Parse payload
    leave_room_request_t *req = (leave_room_request_t *)msg->payload;
    int room_id = (int)ntohl((uint32_t)req->room_id); // Convert from network byte order

    printf("Nhan LEAVE_ROOM tu client %d: room_id=%d\n", client_index, room_id);

    // Tim phong
    room_t *room = protocol_find_room(server, room_id);
    if (!room)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Khong tim thay phong");
        return -1;
    }

    // Kiem tra nguoi choi co trong phong khong
    if (!room_has_player(room, client->user_id))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Ban khong trong phong nay");
        return -1;
    }

    // Neu dang choi va nguoi roi la drawer -> end round ngay de game khong bi ket
    int was_playing = (room->state == ROOM_PLAYING && room->game != NULL);
    int was_drawer = (was_playing && room->game->drawer_id == client->user_id);
    char word_before[64];
    word_before[0] = '\0';
    if (was_playing) {
        strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
        word_before[sizeof(word_before) - 1] = '\0';
    }

    // Xoa nguoi choi khoi phong
    if (!room_remove_player(room, client->user_id))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Khong the roi phong");
        return -1;
    }

    // Luu thong tin truoc khi broadcast
    char room_name[ROOM_NAME_MAX_LENGTH];
    strncpy(room_name, room->room_name, ROOM_NAME_MAX_LENGTH - 1);
    room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';
    int leaving_user_id = client->user_id;
    char leaving_username[32];
    strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
    leaving_username[sizeof(leaving_username) - 1] = '\0';

    // Kiểm tra số người chơi active sau khi rời phòng
    int active_count = 0;
    for (int i = 0; i < room->player_count; i++) {
        if (room->active_players[i] == 1) {
            active_count++;
        }
    }
    
    // Neu phong trong hoặc không còn người chơi active nào, xoa phong
    if (room->player_count == 0 || active_count == 0)
    {
        protocol_remove_room_from_server(server, room);
        room_destroy(room);
        printf("Phong '%s' (ID: %d) da bi xoa vi khong con nguoi choi active (total: %d, active: %d)\n",
               room_name, room_id, room->player_count, active_count);

        // Broadcast danh sach phong cho tat ca clients da dang nhap
        protocol_broadcast_room_list(server);
    }
    else
    {
        // Đảm bảo owner là người chơi active
        if (!room_ensure_active_owner(room)) {
            // Không có người chơi active nào, xóa phòng
            protocol_remove_room_from_server(server, room);
            room_destroy(room);
            printf("Phong '%s' (ID: %d) da bi xoa vi khong co nguoi choi active\n",
                   room_name, room_id);
            protocol_broadcast_room_list(server);
            client->state = CLIENT_STATE_LOGGED_IN;
            protocol_send_leave_room_response(client->fd, STATUS_SUCCESS,
                                              "Roi phong thanh cong");
            return 0;
        }
        
        // Broadcast ROOM_PLAYERS_UPDATE voi danh sach day du (da bao gom tat ca thong tin phong)
        protocol_broadcast_room_players_update(server, room, 1, // 1 = LEAVE
                                               leaving_user_id, leaving_username,
                                               client_index);

        printf("Client %d (user_id=%d, username=%s) da roi phong '%s' (ID: %d)\n",
               client_index, leaving_user_id, leaving_username, room_name, room_id);
    }

    // Xu ly drawer roi phong trong game (sau khi da broadcast danh sach players)
    if (was_drawer && room->game && word_before[0] != '\0') {
        // End round hiện tại trước (để đảm bảo round được đếm đúng)
        game_end_round(room->game, false, -1);
        // Sau đó mới bắt đầu round mới
        protocol_handle_round_timeout(server, room, word_before);
    }

    // Cap nhat trang thai client
    client->state = CLIENT_STATE_LOGGED_IN;

    // Gui response thanh cong
    protocol_send_leave_room_response(client->fd, STATUS_SUCCESS,
                                      "Roi phong thanh cong");

    return 0;
}
