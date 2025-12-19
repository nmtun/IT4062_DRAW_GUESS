#include "../include/protocol.h"
#include "../include/server.h"
#include "../include/room.h"
#include "../include/game.h"
#include "../include/database.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

extern db_connection_t* db;

static void write_u64_be(uint8_t* p, uint64_t v) {
    uint32_t hi = (uint32_t)(v >> 32);
    uint32_t lo = (uint32_t)(v & 0xFFFFFFFFULL);
    uint32_t hi_be = htonl(hi);
    uint32_t lo_be = htonl(lo);
    memcpy(p, &hi_be, 4);
    memcpy(p + 4, &lo_be, 4);
}

static int room_player_db_id(room_t* room, int user_id) {
    if (!room) return 0;
    for (int i = 0; i < room->player_count; i++) {
        if (room->players[i] == user_id) return room->db_player_ids[i];
    }
    return 0;
}

// forward from protocol_game.c
extern int protocol_process_guess(server_t* server, int client_index, room_t* room, const char* guess);

int protocol_handle_chat_message(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg || client_index < 0 || client_index >= MAX_CLIENTS) return -1;
    client_t* client = &server->clients[client_index];
    if (!client->active || client->user_id <= 0) return -1;

    room_t* room = server_find_room_by_user(server, client->user_id);
    if (!room) return -1;

    // Parse message text (max 256)
    char text[MAX_MESSAGE_LEN + 1];
    memset(text, 0, sizeof(text));
    if (msg->payload && msg->length > 0) {
        size_t n = msg->length;
        if (n > MAX_MESSAGE_LEN) n = MAX_MESSAGE_LEN;
        memcpy(text, msg->payload, n);
        text[n] = '\0';
    }
    if (text[0] == '\0') return -1;

    // Filter guess words: nếu đang chơi và message == current_word thì treat như guess (không broadcast chat)
    if (room->state == ROOM_PLAYING && room->game && room->game->current_word[0] != '\0') {
        if (client->user_id != room->game->drawer_id) {
            // delegate to guess flow
            if (protocol_process_guess(server, client_index, room, text) == 0) {
                return 0;
            }
        }
    }

    // Build CHAT_BROADCAST payload: username(32) + message(256) + timestamp(8 ms)
    uint8_t payload[MAX_USERNAME_LEN + MAX_MESSAGE_LEN + 8];
    memset(payload, 0, sizeof(payload));
    strncpy((char*)payload, client->username, MAX_USERNAME_LEN - 1);
    ((char*)payload)[MAX_USERNAME_LEN - 1] = '\0';
    strncpy((char*)(payload + MAX_USERNAME_LEN), text, MAX_MESSAGE_LEN - 1);
    ((char*)(payload + MAX_USERNAME_LEN))[MAX_MESSAGE_LEN - 1] = '\0';

    uint64_t ts_ms = (uint64_t)time(NULL) * 1000ULL;
    write_u64_be(payload + MAX_USERNAME_LEN + MAX_MESSAGE_LEN, ts_ms);

    // Persist (best-effort): chat_messages uses room_id/player_id
    if (db && room->db_room_id > 0) {
        int pid = room_player_db_id(room, client->user_id);
        if (pid > 0) {
            char room_buf[32], pid_buf[32];
            snprintf(room_buf, sizeof(room_buf), "%d", room->db_room_id);
            snprintf(pid_buf, sizeof(pid_buf), "%d", pid);
            MYSQL_RES* r = db_execute_query(db, "INSERT INTO chat_messages (room_id, player_id, message_text) VALUES (?, ?, ?)",
                room_buf, pid_buf, text
            );
            if (r) mysql_free_result(r);
        }
    }

    return server_broadcast_to_room(server, room->room_id, MSG_CHAT_BROADCAST, payload, (uint16_t)sizeof(payload), -1);
}


