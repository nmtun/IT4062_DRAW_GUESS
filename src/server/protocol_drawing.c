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
 * Xu ly DRAW_DATA tu client (drawer)
 * Client gui du lieu ve, server se broadcast den cac clients khac trong phong
 */
int protocol_handle_draw_data(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg || client_index < 0 || client_index >= MAX_CLIENTS) {
        fprintf(stderr, "Loi: Tham so khong hop le trong protocol_handle_draw_data\n");
        return -1;
    }

    client_t* client = &server->clients[client_index];
    
    // Kiem tra client co active va da dang nhap khong
    if (!client->active || client->user_id <= 0) {
        fprintf(stderr, "Client %d chua dang nhap hoac khong active\n", client_index);
        return -1;
    }

    // Kiem tra client co trong phong khong
    room_t* room = server_find_room_by_user(server, client->user_id);
    if (!room) {
        fprintf(stderr, "Client %d (user_id=%d) khong o trong phong nao\n", 
                client_index, client->user_id);
        return -1;
    }

    // Kiem tra phong co dang choi game khong
    if (room->state != ROOM_PLAYING) {
        fprintf(stderr, "Phong %d khong dang choi game (state=%d)\n", 
                room->room_id, room->state);
        return -1;
    }

    // Chi drawer moi duoc gui DRAW_DATA
    if (!room->game || room->game->drawer_id != client->user_id) {
        fprintf(stderr, "Client %d (user_id=%d) khong phai drawer, ignore DRAW_DATA\n",
                client_index, client->user_id);
        return -1;
    }

    // Parse draw action tu payload
    draw_action_t action;
    if (drawing_parse_action(msg->payload, msg->length, &action) != 0) {
        fprintf(stderr, "Loi: Khong the parse draw action tu client %d\n", client_index);
        return -1;
    }

    printf("Nhan DRAW_DATA tu client %d (user_id=%d): action=%d, x1=%d, y1=%d, x2=%d, y2=%d, color=0x%08X, width=%d\n",
           client_index, client->user_id, action.action, action.x1, action.y1, 
           action.x2, action.y2, action.color, action.width);

    // Serialize lai action de broadcast (payload da dung format)
    // Hoac co the dung lai payload goc neu da dung format
    uint8_t draw_payload[14];
    int payload_len = drawing_serialize_action(&action, draw_payload);
    if (payload_len != 14) {
        fprintf(stderr, "Loi: Serialize draw action that bai\n");
        return -1;
    }

    // Broadcast DRAW_BROADCAST den tat ca clients trong phong (tru drawer)
    int broadcast_count = server_broadcast_to_room(server, room->room_id, 
                                                   MSG_DRAW_BROADCAST, 
                                                   draw_payload, 14,
                                                   client->user_id);  // Loai tru drawer

    if (broadcast_count > 0) {
        printf("Da broadcast DRAW_BROADCAST den %d clients trong phong %d\n", 
               broadcast_count, room->room_id);
    } else {
        printf("Canh bao: Khong co client nao nhan duoc DRAW_BROADCAST\n");
    }

    return 0;
}

/**
 * Gui DRAW_BROADCAST den client
 * (Ham nay duoc goi boi server_broadcast_to_room, khong can implement rieng)
 * DRAW_BROADCAST co cung format payload voi DRAW_DATA
 */

