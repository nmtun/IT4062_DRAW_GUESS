#include "../include/drawing.h"
#include "../include/room.h"
#include "../include/server.h"
#include "../include/game.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/**
 * Xử lý DRAW_DATA từ client (drawer)
 * Client gửi dữ liệu vẽ, server sẽ broadcast đến các clients khác trong phòng
 */
int protocol_handle_draw_data(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg || client_index < 0 || client_index >= MAX_CLIENTS) {
        fprintf(stderr, "Lỗi: Tham số không hợp lệ trong protocol_handle_draw_data\n");
        return -1;
    }

    client_t* client = &server->clients[client_index];
    
    // Kiểm tra client có active và đã đăng nhập không
    if (!client->active || client->user_id <= 0) {
        fprintf(stderr, "Client %d chưa đăng nhập hoặc không active\n", client_index);
        return -1;
    }

    // Kiểm tra client có trong phòng không
    room_t* room = server_find_room_by_user(server, client->user_id);
    if (!room) {
        fprintf(stderr, "Client %d (user_id=%d) không ở trong phòng nào\n", 
                client_index, client->user_id);
        return -1;
    }

    // Kiểm tra phòng có đang chơi game không
    if (room->state != ROOM_PLAYING) {
        fprintf(stderr, "Phòng %d không đang chơi game (state=%d)\n", 
                room->room_id, room->state);
        return -1;
    }

    // Chỉ drawer mới được gửi DRAW_DATA
    if (!room->game || room->game->drawer_id != client->user_id) {
        fprintf(stderr, "Client %d (user_id=%d) không phải drawer, ignore DRAW_DATA\n",
                client_index, client->user_id);
        return -1;
    }

    // Parse draw action từ payload
    draw_action_t action;
    if (drawing_parse_action(msg->payload, msg->length, &action) != 0) {
        fprintf(stderr, "Lỗi: Không thể parse draw action từ client %d\n", client_index);
        return -1;
    }

    printf("Nhận DRAW_DATA từ client %d (user_id=%d): action=%d, x1=%d, y1=%d, x2=%d, y2=%d, color=0x%08X, width=%d\n",
           client_index, client->user_id, action.action, action.x1, action.y1, 
           action.x2, action.y2, action.color, action.width);

    // Serialize lại action để broadcast (payload đã đúng format)
    // Hoặc có thể dùng lại payload gốc nếu đã đúng format
    uint8_t draw_payload[14];
    int payload_len = drawing_serialize_action(&action, draw_payload);
    if (payload_len != 14) {
        fprintf(stderr, "Lỗi: Serialize draw action thất bại\n");
        return -1;
    }

    // Broadcast DRAW_BROADCAST đến tất cả clients trong phòng (trừ drawer)
    int broadcast_count = server_broadcast_to_room(server, room->room_id, 
                                                   MSG_DRAW_BROADCAST, 
                                                   draw_payload, 14,
                                                   client->user_id);  // Loại trừ drawer

    if (broadcast_count > 0) {
        printf("Đã broadcast DRAW_BROADCAST đến %d clients trong phòng %d\n", 
               broadcast_count, room->room_id);
    } else {
        printf("Cảnh báo: Không có client nào nhận được DRAW_BROADCAST\n");
    }

    return 0;
}

/**
 * Gửi DRAW_BROADCAST đến client
 * (Hàm này được gọi bởi server_broadcast_to_room, không cần implement riêng)
 * DRAW_BROADCAST có cùng format payload với DRAW_DATA
 */

