#include "../include/protocol.h"
#include "../include/room.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * Gửi CREATE_ROOM_RESPONSE
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
 * Gửi JOIN_ROOM_RESPONSE
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
 * Gửi LEAVE_ROOM_RESPONSE
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
 * Gửi ROOM_LIST_RESPONSE đến client
 */
int protocol_send_room_list(int client_fd, server_t *server)
{
    if (!server)
    {
        return -1;
    }

    // Lấy danh sách phòng
    room_info_t room_list[50]; // MAX_ROOMS
    int room_count = room_get_list(server, room_list, 50);

    // Tính kích thước payload
    uint16_t payload_size = sizeof(room_list_response_t) +
                            room_count * sizeof(room_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Lỗi: Room list quá lớn (%d bytes)\n", payload_size);
        return -1;
    }

    // Tạo payload
    uint8_t payload[BUFFER_SIZE];
    room_list_response_t *header = (room_list_response_t *)payload;
    header->room_count = htons((uint16_t)room_count);

    // Chuyển đổi room_info_t sang room_info_protocol_t
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
    }

    return protocol_send_message(client_fd, MSG_ROOM_LIST_RESPONSE,
                                 payload, payload_size);
}

/**
 * Broadcast ROOM_LIST_RESPONSE đến tất cả clients đã đăng nhập
 * Gọi khi có phòng mới được tạo hoặc phòng bị xóa
 */
int protocol_broadcast_room_list(server_t *server)
{
    if (!server)
    {
        return -1;
    }

    // Lấy danh sách phòng
    room_info_t room_list[50]; // MAX_ROOMS
    int room_count = room_get_list(server, room_list, 50);

    // Tính kích thước payload
    uint16_t payload_size = sizeof(room_list_response_t) +
                            room_count * sizeof(room_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Lỗi: Room list quá lớn (%d bytes)\n", payload_size);
        return -1;
    }

    // Tạo payload
    uint8_t payload[BUFFER_SIZE];
    room_list_response_t *header = (room_list_response_t *)payload;
    header->room_count = htons((uint16_t)room_count);

    // Chuyển đổi room_info_t sang room_info_protocol_t
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
        unsigned char *bytes = (unsigned char *)&room_info_proto[i].owner_id;
    }

    // Gửi đến tất cả clients đã đăng nhập (LOGGED_IN trở lên)
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *client = &server->clients[i];

        // Chỉ gửi cho clients đã đăng nhập (không gửi cho clients trong phòng)
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

    printf("Đã broadcast ROOM_LIST_RESPONSE đến %d clients (tổng %d phòng)\n",
           sent_count, room_count);

    return sent_count;
}

/**
 * Broadcast ROOM_PLAYERS_UPDATE đến tất cả clients trong phòng
 * Gửi danh sách đầy đủ người chơi khi có thay đổi
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

    // Tính kích thước payload
    uint16_t payload_size = sizeof(room_players_update_t) +
                            room->player_count * sizeof(player_info_protocol_t);

    if (payload_size > BUFFER_SIZE - 3)
    {
        fprintf(stderr, "Lỗi: Room players update quá lớn (%d bytes)\n", payload_size);
        return -1;
    }

    // Tạo payload
    uint8_t payload[BUFFER_SIZE];
    room_players_update_t *update = (room_players_update_t *)payload;

    // Thông tin phòng đầy đủ
    update->room_id = htonl((uint32_t)room->room_id);
    strncpy(update->room_name, room->room_name, MAX_ROOM_NAME_LEN - 1);
    update->room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
    update->max_players = (uint8_t)room->max_players;
    update->state = (uint8_t)room->state;
    update->owner_id = htonl((uint32_t)room->owner_id);

    // Thông tin thay đổi người chơi
    update->action = action; // 0 = JOIN, 1 = LEAVE
    update->changed_user_id = htonl((uint32_t)changed_user_id);
    strncpy(update->changed_username, changed_username, MAX_USERNAME_LEN - 1);
    update->changed_username[MAX_USERNAME_LEN - 1] = '\0';
    update->player_count = htons((uint16_t)room->player_count);

    // Lấy danh sách người chơi hiện tại
    player_info_protocol_t *players = (player_info_protocol_t *)(payload + sizeof(room_players_update_t));

    for (int i = 0; i < room->player_count; i++)
    {
        int player_user_id = room->players[i];
        players[i].user_id = htonl((uint32_t)player_user_id);
        players[i].is_owner = (player_user_id == room->owner_id) ? 1 : 0;

        // Tìm username từ server->clients
        strncpy(players[i].username, "Unknown", MAX_USERNAME_LEN - 1);
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (server->clients[j].active &&
                server->clients[j].user_id == player_user_id)
            {
                strncpy(players[i].username, server->clients[j].username, MAX_USERNAME_LEN - 1);
                players[i].username[MAX_USERNAME_LEN - 1] = '\0';
                break;
            }
        }
    }

    // Gửi đến tất cả clients trong phòng
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
    printf("Đã gửi ROOM_PLAYERS_UPDATE (action=%s, user_id=%d) cho phòng '%s' đến %d clients\n",
           action_str, changed_user_id, room->room_name, sent_count);

    return (sent_count > 0) ? 0 : -1;
}

/**
 * Broadcast ROOM_UPDATE đến tất cả clients trong phòng
 */
int protocol_broadcast_room_update(server_t *server, room_t *room, int exclude_client_index)
{
    if (!server || !room)
    {
        return -1;
    }

    // Tạo room_info_protocol_t từ room
    room_info_protocol_t room_info;
    room_info.room_id = htonl((uint32_t)room->room_id);
    strncpy(room_info.room_name, room->room_name, MAX_ROOM_NAME_LEN - 1);
    room_info.room_name[MAX_ROOM_NAME_LEN - 1] = '\0';
    room_info.player_count = (uint8_t)room->player_count;
    room_info.max_players = (uint8_t)room->max_players;
    room_info.state = (uint8_t)room->state;
    room_info.owner_id = htonl((uint32_t)room->owner_id);

    // Gửi đến tất cả clients trong phòng
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_t *client = &server->clients[i];

        // Bỏ qua client không active hoặc client bị loại trừ
        if (!client->active || i == exclude_client_index)
        {
            continue;
        }

        // Kiểm tra client có trong phòng không
        if (room_has_player(room, client->user_id))
        {
            if (protocol_send_message(client->fd, MSG_ROOM_UPDATE,
                                      (uint8_t *)&room_info, sizeof(room_info)) == 0)
            {
                sent_count++;
            }
        }
    }

    printf("Đã gửi ROOM_UPDATE cho phòng '%s' (ID: %d) đến %d clients\n",
           room->room_name, room->room_id, sent_count);

    return (sent_count > 0) ? 0 : -1;
}

/**
 * Tìm room trong server theo room_id
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
 * Thêm room vào server
 */
static int protocol_add_room_to_server(server_t *server, room_t *room)
{
    if (!server || !room)
    {
        return -1;
    }

    // Tìm slot trống
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (server->rooms[i] == NULL)
        {
            server->rooms[i] = room;
            server->room_count++;
            printf("Đã thêm phòng '%s' (ID: %d) vào server. Tổng số phòng: %d\n",
                   room->room_name, room->room_id, server->room_count);
            return 0;
        }
    }

    fprintf(stderr, "Lỗi: Server đã đầy phòng (MAX_ROOMS = %d)\n", MAX_ROOMS);
    return -1;
}

/**
 * Xóa room khỏi server
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
            printf("Đã xóa phòng '%s' (ID: %d) khỏi server. Tổng số phòng: %d\n",
                   room->room_name, room->room_id, server->room_count);
            return 0;
        }
    }

    return -1;
}

/**
 * Xử lý ROOM_LIST_REQUEST
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

    // Kiểm tra client đã đăng nhập chưa
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        fprintf(stderr, "Client %d chưa đăng nhập, không thể lấy danh sách phòng\n", client_index);
        return -1;
    }

    printf("Nhận ROOM_LIST_REQUEST từ client %d (user_id=%d)\n", client_index, client->user_id);

    return protocol_send_room_list(client->fd, server);
}

/**
 * Xử lý CREATE_ROOM
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

    // Kiểm tra client đã đăng nhập chưa
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Bạn cần đăng nhập để tạo phòng");
        return -1;
    }

    // Kiểm tra client đã trong phòng chưa
    if (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Bạn đang trong phòng khác");
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(create_room_request_t))
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Dữ liệu không hợp lệ");
        return -1;
    }

    // Parse payload
    create_room_request_t *req = (create_room_request_t *)msg->payload;

    // Đảm bảo null-terminated
    char room_name[MAX_ROOM_NAME_LEN];
    strncpy(room_name, req->room_name, MAX_ROOM_NAME_LEN - 1);
    room_name[MAX_ROOM_NAME_LEN - 1] = '\0';

    int max_players = (int)req->max_players;
    int rounds = (int)req->rounds;

    printf("Nhận CREATE_ROOM từ client %d: room_name=%s, max_players=%d, rounds=%d\n",
           client_index, room_name, max_players, rounds);

    // Validate
    if (strlen(room_name) == 0)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Tên phòng không được để trống");
        return -1;
    }

    if (max_players < 2 || max_players > 10)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Số người chơi phải từ 2-10");
        return -1;
    }

    if (rounds < 1 || rounds > 10)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Số round phải từ 1-10");
        return -1;
    }

    // Kiểm tra server đã đầy phòng chưa
    if (server->room_count >= MAX_ROOMS)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Server đã đầy phòng");
        return -1;
    }

    // Tạo phòng
    room_t *room = room_create(room_name, client->user_id, max_players, rounds);
    if (!room)
    {
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Không thể tạo phòng");
        return -1;
    }

    // Thêm phòng vào server
    if (protocol_add_room_to_server(server, room) != 0)
    {
        room_destroy(room);
        protocol_send_create_room_response(client->fd, STATUS_ERROR, -1,
                                           "Không thể thêm phòng vào server");
        return -1;
    }

    // Cập nhật trạng thái client
    client->state = CLIENT_STATE_IN_ROOM;

    // Gửi response thành công
    protocol_send_create_room_response(client->fd, STATUS_SUCCESS, room->room_id,
                                       "Tạo phòng thành công");

    printf("Client %d đã tạo phòng '%s' (ID: %d) thành công\n",
           client_index, room_name, room->room_id);

    // Broadcast danh sách phòng cho tất cả clients đã đăng nhập
    protocol_broadcast_room_list(server);

    // Gửi ROOM_PLAYERS_UPDATE cho chính phòng vừa tạo
    // Đảm bảo client tạo phòng nhìn thấy mình là owner và danh sách người chơi hiện tại
    protocol_broadcast_room_players_update(
        server,
        room,
        0, // action = JOIN
        client->user_id,
        client->username,
        -1 // không loại trừ ai; gửi cho tất cả trong phòng (hiện tại chỉ có owner)
    );

    return 0;
}

/**
 * Xử lý JOIN_ROOM
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

    // Kiểm tra client đã đăng nhập chưa
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Bạn cần đăng nhập để tham gia phòng");
        return -1;
    }

    // Kiểm tra client đã trong phòng chưa
    if (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Bạn đang trong phòng khác");
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(join_room_request_t))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Dữ liệu không hợp lệ");
        return -1;
    }

    // Parse payload
    join_room_request_t *req = (join_room_request_t *)msg->payload;
    int room_id = (int)ntohl((uint32_t)req->room_id); // Convert from network byte order

    printf("Nhận JOIN_ROOM từ client %d: room_id=%d\n", client_index, room_id);

    // Tìm phòng
    room_t *room = protocol_find_room(server, room_id);
    if (!room)
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Không tìm thấy phòng");
        return -1;
    }

    // Kiểm tra phòng đã đầy chưa
    if (room_is_full(room))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Phòng đã đầy");
        return -1;
    }

    // Kiểm tra người chơi đã trong phòng chưa
    if (room_has_player(room, client->user_id))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Bạn đã trong phòng này");
        return -1;
    }

    // Thêm người chơi vào phòng
    if (!room_add_player(room, client->user_id))
    {
        protocol_send_join_room_response(client->fd, STATUS_ERROR, -1,
                                         "Không thể tham gia phòng");
        return -1;
    }

    // Cập nhật trạng thái client
    client->state = CLIENT_STATE_IN_ROOM;

    // Gửi response thành công
    protocol_send_join_room_response(client->fd, STATUS_SUCCESS, room_id,
                                     "Tham gia phòng thành công");

    // Broadcast ROOM_PLAYERS_UPDATE với danh sách đầy đủ (đã bao gồm tất cả thông tin phòng)
    // Gửi cho TẤT CẢ clients trong phòng, bao gồm cả client vừa join
    protocol_broadcast_room_players_update(server, room, 0, // 0 = JOIN
                                           client->user_id, client->username,
                                           -1); // -1 = không exclude ai

    printf("Client %d (user_id=%d, username=%s) đã tham gia phòng '%s' (ID: %d)\n",
           client_index, client->user_id, client->username, room->room_name, room_id);

    return 0;
}

/**
 * Xử lý LEAVE_ROOM
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

    // Kiểm tra client đã đăng nhập chưa
    if (client->state == CLIENT_STATE_LOGGED_OUT || client->user_id <= 0)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Bạn chưa đăng nhập");
        return -1;
    }

    // Kiểm tra client có trong phòng không
    if (client->state != CLIENT_STATE_IN_ROOM && client->state != CLIENT_STATE_IN_GAME)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Bạn không trong phòng nào");
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(leave_room_request_t))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Dữ liệu không hợp lệ");
        return -1;
    }

    // Parse payload
    leave_room_request_t *req = (leave_room_request_t *)msg->payload;
    int room_id = (int)ntohl((uint32_t)req->room_id); // Convert from network byte order

    printf("Nhận LEAVE_ROOM từ client %d: room_id=%d\n", client_index, room_id);

    // Tìm phòng
    room_t *room = protocol_find_room(server, room_id);
    if (!room)
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Không tìm thấy phòng");
        return -1;
    }

    // Kiểm tra người chơi có trong phòng không
    if (!room_has_player(room, client->user_id))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Bạn không trong phòng này");
        return -1;
    }

    // Xóa người chơi khỏi phòng
    if (!room_remove_player(room, client->user_id))
    {
        protocol_send_leave_room_response(client->fd, STATUS_ERROR,
                                          "Không thể rời phòng");
        return -1;
    }

    // Lưu thông tin trước khi broadcast
    char room_name[ROOM_NAME_MAX_LENGTH];
    strncpy(room_name, room->room_name, ROOM_NAME_MAX_LENGTH - 1);
    room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';
    int leaving_user_id = client->user_id;
    char leaving_username[32];
    strncpy(leaving_username, client->username, sizeof(leaving_username) - 1);
    leaving_username[sizeof(leaving_username) - 1] = '\0';

    // Nếu phòng trống sau khi rời, xóa phòng
    if (room->player_count == 0)
    {
        protocol_remove_room_from_server(server, room);
        room_destroy(room);
        printf("Phòng '%s' (ID: %d) đã bị xóa vì không còn người chơi\n",
               room_name, room_id);

        // Broadcast danh sách phòng cho tất cả clients đã đăng nhập
        protocol_broadcast_room_list(server);
    }
    else
    {
        // Broadcast ROOM_PLAYERS_UPDATE với danh sách đầy đủ (đã bao gồm tất cả thông tin phòng)
        protocol_broadcast_room_players_update(server, room, 1, // 1 = LEAVE
                                               leaving_user_id, leaving_username,
                                               client_index);

        printf("Client %d (user_id=%d, username=%s) đã rời phòng '%s' (ID: %d)\n",
               client_index, leaving_user_id, leaving_username, room_name, room_id);
    }

    // Cập nhật trạng thái client
    client->state = CLIENT_STATE_LOGGED_IN;

    // Gửi response thành công
    protocol_send_leave_room_response(client->fd, STATUS_SUCCESS,
                                      "Rời phòng thành công");

    return 0;
}
