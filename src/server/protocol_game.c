#include "../include/protocol.h"
#include "../include/game.h"
#include "../include/room.h"
#include "../include/server.h"
#include "../include/database.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdint.h>

extern db_connection_t* db;

// Payload formats (binary protocol):
// - GAME_START: drawer_id(4) + word_length(1) + time_limit(2) + round_start_ms(8) + current_round(4) + player_count(1) + total_rounds(1) + word(64) + category(64) = 149 bytes
// - CORRECT_GUESS: player_id(4) + word(64) + guesser_points(2) + drawer_points(2) + username(32) = 104 bytes
// - WRONG_GUESS: player_id(4) + guess(64) = 68 bytes (không còn dùng nữa)
// - ROUND_END: word(64) + score_count(2) + pairs(user_id(4), score(4))...
// - GAME_END: winner_id(4) + score_count(2) + pairs(user_id(4), score(4))...

static void write_i32_be(uint8_t* p, int32_t v) {
    uint32_t u = (uint32_t)v;
    uint32_t be = htonl(u);
    memcpy(p, &be, sizeof(be));
}

static void write_u16_be(uint8_t* p, uint16_t v) {
    uint16_t be = htons(v);
    memcpy(p, &be, sizeof(be));
}

static void write_u64_be(uint8_t* p, uint64_t v) {
    // htonll portable
    uint32_t hi = (uint32_t)(v >> 32);
    uint32_t lo = (uint32_t)(v & 0xFFFFFFFFULL);
    uint32_t hi_be = htonl(hi);
    uint32_t lo_be = htonl(lo);
    memcpy(p, &hi_be, 4);
    memcpy(p + 4, &lo_be, 4);
}

static void write_fixed_string(uint8_t* p, size_t n, const char* s) {
    memset(p, 0, n);
    if (!s) return;
    strncpy((char*)p, s, n - 1);
    ((char*)p)[n - 1] = '\0';
}

static int find_client_index_by_user(server_t* server, int user_id) {
    if (!server || user_id <= 0) return -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active && server->clients[i].user_id == user_id) return i;
    }
    return -1;
}

static int room_player_db_id(room_t* room, int user_id) {
    if (!room) {
        printf("[PROTOCOL] ERROR: room_player_db_id called with NULL room\n");
        return 0;
    }
    if (user_id <= 0) {
        printf("[PROTOCOL] ERROR: room_player_db_id called with invalid user_id=%d\n", user_id);
        return 0;
    }
    if (room->player_count <= 0 || room->player_count > MAX_PLAYERS_PER_ROOM) {
        printf("[PROTOCOL] ERROR: Invalid player_count=%d\n", room->player_count);
        return 0;
    }
    for (int i = 0; i < room->player_count && i < MAX_PLAYERS_PER_ROOM; i++) {
        if (room->players[i] == user_id) {
            return room->db_player_ids[i];
        }
    }
    printf("[PROTOCOL] WARNING: user_id=%d not found in room players\n", user_id);
    return 0;
}

static int broadcast_game_start(server_t* server, room_t* room) {
    if (!server || !room || !room->game) return -1;

    game_state_t* game = room->game;
    // Payload: drawer_id(4) + word_length(1) + time_limit(2) + round_start_ms(8) 
    //          + current_round(4) + player_count(1) + total_rounds(1) + word(64) + category(64) = 149 bytes
    uint8_t payload[149];
    memset(payload, 0, sizeof(payload));
    write_i32_be(payload + 0, (int32_t)game->drawer_id);
    payload[4] = (uint8_t)game->word_length;
    write_u16_be(payload + 5, (uint16_t)game->time_limit);
    // round_start_time is seconds in game_state -> convert to ms
    uint64_t start_ms = (uint64_t)game->round_start_time * 1000ULL;
    write_u64_be(payload + 7, start_ms);
    // Thêm current_round, player_count và total_rounds để tính vòng hiện tại
    write_i32_be(payload + 15, (int32_t)game->current_round);
    payload[19] = (uint8_t)room->player_count;
    payload[20] = (uint8_t)room->total_rounds; // Số vòng gốc (trước khi nhân với player_count)
    printf("[PROTOCOL] GAME_START payload: current_round=%d, player_count=%d, total_rounds=%d\n",
           game->current_round, room->player_count, room->total_rounds);

    // Send to each client in room; drawer gets the word, others empty
    // Tất cả đều nhận category
    int sent = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* c = &server->clients[i];
        if (!c->active || c->user_id <= 0) continue;
        if (!room_has_player(room, c->user_id)) continue;

        if (c->user_id == game->drawer_id) {
            write_fixed_string(payload + 21, 64, game->current_word);
        } else {
            write_fixed_string(payload + 21, 64, "");
        }
        // Category được gửi cho tất cả người chơi
        write_fixed_string(payload + 85, 64, game->current_category);

        if (protocol_send_message(c->fd, MSG_GAME_START, payload, (uint16_t)sizeof(payload)) == 0) {
            sent++;
        }
    }
    return sent;
}

static int broadcast_round_end(server_t* server, room_t* room, const char* word) {
    if (!server || !room || !room->game) return -1;
    game_state_t* game = room->game;

    // header + score pairs
    const uint16_t score_count = (uint16_t)game->score_count;
    const size_t payload_size = 64 + 2 + (size_t)score_count * (sizeof(int32_t) + sizeof(int32_t));
    if (payload_size > BUFFER_SIZE - 3) return -1;

    uint8_t payload[BUFFER_SIZE];
    memset(payload, 0, payload_size);
    write_fixed_string(payload + 0, 64, word ? word : "");
    write_u16_be(payload + 64, score_count);

    uint8_t* p = payload + 66;
    for (int i = 0; i < game->score_count; i++) {
        write_i32_be(p, (int32_t)game->scores[i].user_id);
        p += 4;
        write_i32_be(p, (int32_t)game->scores[i].score);
        p += 4;
    }

    return server_broadcast_to_room(server, room->room_id, MSG_ROUND_END, payload, (uint16_t)payload_size, -1);
}

static int broadcast_game_end(server_t* server, room_t* room) {
    if (!server || !room || !room->game) return -1;
    game_state_t* game = room->game;

    // compute winner (same logic as game.c, duplicated to avoid exposing helper)
    int winner_id = -1;
    int best = -2147483647;
    for (int i = 0; i < game->score_count; i++) {
        if (game->scores[i].score > best) {
            best = game->scores[i].score;
            winner_id = game->scores[i].user_id;
        }
    }

    // Lưu lịch sử chơi cho tất cả người chơi
    if (db && game->score_count > 0) {
        // Tạo mảng tạm để sắp xếp theo điểm giảm dần (để tính rank)
        typedef struct {
            int user_id;
            int score;
        } player_score_t;
        
        player_score_t sorted_scores[MAX_PLAYERS_PER_ROOM];
        for (int i = 0; i < game->score_count; i++) {
            sorted_scores[i].user_id = game->scores[i].user_id;
            sorted_scores[i].score = game->scores[i].score;
        }
        
        // Bubble sort giảm dần theo score
        for (int i = 0; i < game->score_count - 1; i++) {
            for (int j = 0; j < game->score_count - i - 1; j++) {
                if (sorted_scores[j].score < sorted_scores[j + 1].score) {
                    player_score_t temp = sorted_scores[j];
                    sorted_scores[j] = sorted_scores[j + 1];
                    sorted_scores[j + 1] = temp;
                }
            }
        }
        
        // Lưu lịch sử với rank
        for (int i = 0; i < game->score_count; i++) {
            int rank = i + 1; // rank từ 1, 2, 3, ...
            db_save_game_history(db, sorted_scores[i].user_id, sorted_scores[i].score, rank);
        }
        
        printf("[GAME_END] Saved game history for %d players\n", game->score_count);
    }

    const uint16_t score_count = (uint16_t)game->score_count;
    const size_t payload_size = 4 + 2 + (size_t)score_count * (sizeof(int32_t) + sizeof(int32_t));
    if (payload_size > BUFFER_SIZE - 3) return -1;

    uint8_t payload[BUFFER_SIZE];
    memset(payload, 0, payload_size);
    write_i32_be(payload + 0, (int32_t)winner_id);
    write_u16_be(payload + 4, score_count);

    uint8_t* p = payload + 6;
    for (int i = 0; i < game->score_count; i++) {
        write_i32_be(p, (int32_t)game->scores[i].user_id);
        p += 4;
        write_i32_be(p, (int32_t)game->scores[i].score);
        p += 4;
    }

    return server_broadcast_to_room(server, room->room_id, MSG_GAME_END, payload, (uint16_t)payload_size, -1);
}

int protocol_handle_start_game(server_t* server, int client_index, const message_t* msg) {
    (void)msg;
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS) return -1;

    client_t* client = &server->clients[client_index];
    if (!client->active || client->user_id <= 0) return -1;
    if (client->state != CLIENT_STATE_IN_ROOM) {
        // only allow start from waiting room state
        return -1;
    }

    room_t* room = server_find_room_by_user(server, client->user_id);
    if (!room) return -1;
    if (!room_is_owner(room, client->user_id)) {
        return -1;
    }

    if (!room_start_game(room)) {
        return -1;
    }

    // mark all clients in room as IN_GAME
    for (int i = 0; i < room->player_count; i++) {
        int uid = room->players[i];
        int cidx = find_client_index_by_user(server, uid);
        if (cidx >= 0) {
            server->clients[cidx].state = CLIENT_STATE_IN_GAME;
        }
    }

    if (!room->game) return -1;
    if (!game_start_round(room->game)) return -1;

    // Create db game_round for first round (best-effort)
    if (db && room->db_room_id > 0) {
        int drawer_pid = room_player_db_id(room, room->game->drawer_id);
        if (drawer_pid > 0) {
            room->game->db_round_id = db_create_game_round(db, room->db_room_id, room->game->current_round, room->game->drawer_index, drawer_pid, room->game->current_word);
        }
        // mark room in progress
        char rid_buf[32];
        snprintf(rid_buf, sizeof(rid_buf), "%d", room->db_room_id);
        MYSQL_RES* r = db_execute_query(db, "UPDATE rooms SET status='in_progress' WHERE id = ?", rid_buf);
        if (r) mysql_free_result(r);
    }

    broadcast_game_start(server, room);
    return 0;
}

int protocol_handle_guess_word(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) return -1;
    client_t* client = &server->clients[client_index];
    if (!client->active || client->user_id <= 0) return -1;
    if (client->state != CLIENT_STATE_IN_GAME) return -1;

    room_t* room = server_find_room_by_user(server, client->user_id);
    if (!room || !room->game) return -1;

    // Parse guess word (payload: word[64])
    char guess[MAX_WORD_LEN];
    memset(guess, 0, sizeof(guess));
    if (msg->payload && msg->length > 0) {
        size_t n = msg->length;
        if (n >= MAX_WORD_LEN) n = MAX_WORD_LEN - 1;
        memcpy(guess, msg->payload, n);
        guess[n] = '\0';
        // strip null padding
        for (int i = 0; i < MAX_WORD_LEN; i++) {
            if (guess[i] == '\0') break;
        }
    }

    if (guess[0] == '\0') return -1;

    return protocol_process_guess(server, client_index, room, guess);
}

int protocol_process_guess(server_t* server, int client_index, room_t* room, const char* guess) {
    if (!server || !room || !room->game || !guess) {
        printf("[PROTOCOL] ERROR: Invalid parameters in protocol_process_guess\n");
        return -1;
    }
    client_t* client = &server->clients[client_index];
    if (!client->active || client->user_id <= 0) {
        printf("[PROTOCOL] ERROR: Invalid client in protocol_process_guess\n");
        return -1;
    }

    // Keep the word before game_end_round clears it
    char current_word[MAX_WORD_LEN];
    memset(current_word, 0, sizeof(current_word));
    if (room->game->current_word[0] != '\0') {
        strncpy(current_word, room->game->current_word, MAX_WORD_LEN - 1);
    }

    printf("[PROTOCOL] Processing guess from user_id=%d, word='%s'\n", client->user_id, guess);
    const bool correct = game_handle_guess(room->game, client->user_id, guess);
    printf("[PROTOCOL] game_handle_guess returned: %d\n", correct ? 1 : 0);

    // Kiểm tra lại room->game sau khi gọi game_handle_guess
    if (!room->game) {
        printf("[PROTOCOL] ERROR: room->game became NULL after game_handle_guess\n");
        return -1;
    }

    // Persist guess (best-effort)
    if (db && room->game && room->game->db_round_id > 0 && room->db_room_id > 0) {
        int pid = room_player_db_id(room, client->user_id);
        if (pid > 0) {
            db_save_guess(db, room->game->db_round_id, pid, guess, correct ? 1 : 0);
        }
    }

    if (!correct) {
        // Không broadcast MSG_WRONG_GUESS nữa, chỉ return -1 để báo cần broadcast chat message
        return -1;
    }

    // Tính điểm theo thứ tự: 1st (10/5), 2nd (7/4), 3rd (5/3), 4th+ (3/2)
    // guessed_count đã được tăng trong game_handle_guess, nên nó là số người đã đoán đúng (1, 2, 3, ...)
    if (!room->game) {
        printf("[PROTOCOL] ERROR: room->game is NULL before calculating points\n");
        return -1;
    }
    int guesser_order = room->game->guessed_count;
    printf("[PROTOCOL] guessed_count=%d, calculating points...\n", guesser_order);
    int guesser_points, drawer_points;

    if (guesser_order <= 0) {
        // Fallback nếu có lỗi
        printf("[PROTOCOL] WARNING: guessed_count=%d, using default points\n", guesser_order);
        guesser_points = 10;
        drawer_points = 5;
    } else if (guesser_order == 1) {
        guesser_points = 10;
        drawer_points = 5;
    } else if (guesser_order == 2) {
        guesser_points = 7;
        drawer_points = 4;
    } else if (guesser_order == 3) {
        guesser_points = 5;
        drawer_points = 3;
    } else {
        // Từ người thứ 4 trở đi
        guesser_points = 3;
        drawer_points = 2;
    }

    // Kiểm tra lại room->game trước khi broadcast
    if (!room->game) {
        printf("[PROTOCOL] ERROR: room->game is NULL before broadcast\n");
        return -1;
    }

    // correct guess broadcast: player_id(4) + word(64) + guesser_points(2) + drawer_points(2) + username(32) = 104 bytes
    uint8_t cp[104];
    memset(cp, 0, sizeof(cp));
    write_i32_be(cp + 0, (int32_t)client->user_id);
    write_fixed_string(cp + 4, 64, current_word);
    write_u16_be(cp + 68, (uint16_t)guesser_points);
    write_u16_be(cp + 70, (uint16_t)drawer_points);
    write_fixed_string(cp + 72, 32, client->username);
    printf("[PROTOCOL] Broadcasting CORRECT_GUESS: user_id=%d, username=%s, guesser_points=%d, drawer_points=%d\n",
           client->user_id, client->username, guesser_points, drawer_points);
    server_broadcast_to_room(server, room->room_id, MSG_CORRECT_GUESS, cp, (uint16_t)sizeof(cp), -1);

    // Persist score details (best-effort)
    if (db && room->game && room->game->db_round_id > 0 && room->db_room_id > 0) {
        int guesser_pid = room_player_db_id(room, client->user_id);
        int drawer_pid = 0;
        if (room->game && room->game->drawer_id > 0) {
            drawer_pid = room_player_db_id(room, room->game->drawer_id);
        }
        if (guesser_pid > 0) {
            db_save_score_detail(db, room->game->db_round_id, guesser_pid, guesser_points);
        }
        if (drawer_pid > 0) {
            db_save_score_detail(db, room->game->db_round_id, drawer_pid, drawer_points);
        }
    }

    // KHÔNG broadcast ROOM_PLAYERS_UPDATE ở đây vì nó không chứa scores
    // Frontend sẽ cập nhật điểm từ CORRECT_GUESS message
    // ROOM_PLAYERS_UPDATE chỉ dùng để cập nhật danh sách người chơi, không phải scores

    // Kiểm tra xem tất cả người chơi active (trừ người vẽ) đã đoán đúng chưa
    // Nếu đã đoán đúng hết, kết thúc round sớm và chuyển sang lượt tiếp theo
    if (room->game && !room->game->game_ended) {
        // Đếm số người chơi active (trừ drawer)
        int total_active_guessers = 0;
        int drawer_index = -1;
        
        // Tìm drawer index
        for (int i = 0; i < room->player_count; i++) {
            if (room->players[i] == room->game->drawer_id) {
                drawer_index = i;
                break;
            }
        }
        
        // Đếm số người chơi active (không tính drawer)
        for (int i = 0; i < room->player_count; i++) {
            // Chỉ đếm người chơi active (active_players[i] == 1) và không phải drawer
            if (room->active_players[i] == 1 && i != drawer_index) {
                total_active_guessers++;
            }
        }
        
        int guessed_count = room->game->guessed_count; // Số người đã đoán đúng
        
        if (total_active_guessers > 0 && guessed_count >= total_active_guessers) {
            printf("[PROTOCOL] Tất cả người chơi active (trừ người vẽ) đã đoán đúng (%d/%d). Kết thúc round sớm.\n",
                   guessed_count, total_active_guessers);
            // Lưu word trước khi game_end_round xóa nó
            char word_before_clear[64];
            memset(word_before_clear, 0, sizeof(word_before_clear));
            strncpy(word_before_clear, current_word, sizeof(word_before_clear) - 1);
            
            // Kết thúc round với success = true
            game_end_round(room->game, true, -1);
            
            // Broadcast round_end và chuyển sang round tiếp theo
            protocol_handle_round_timeout(server, room, word_before_clear);
            return 0;
        }
    }

    // Nếu chưa đủ người đoán đúng, tiếp tục chờ đến hết giờ
    return 0;
}

// Called by server tick when timeout happens
// Note: Nếu được gọi sau khi drawer rời phòng, game_end_round đã được gọi trước đó
// nên không cần gọi lại game_end_round ở đây
int protocol_handle_round_timeout(server_t* server, room_t* room, const char* word_before_clear) {
    if (!server || !room || !room->game) return -1;

    // Chỉ broadcast round_end nếu round chưa được end (word vẫn còn)
    // Nếu round đã được end (word đã bị clear), không cần broadcast lại
    if (word_before_clear && word_before_clear[0] != '\0') {
        broadcast_round_end(server, room, word_before_clear);
    }

    if (room->game->game_ended) {
        broadcast_game_end(server, room);
        room_end_game(room);
        return 0;
    }

    // Thử bắt đầu round mới
    // Nếu drawer không active, game_start_round sẽ end round ngay và trả về false
    // Trong trường hợp đó, cần tiếp tục thử bắt đầu round tiếp theo
    int max_attempts = room->player_count + 1; // Tối đa thử qua tất cả người chơi
    int attempts = 0;
    
    while (attempts < max_attempts && !room->game->game_ended) {
        if (game_start_round(room->game)) {
            // Round bắt đầu thành công với drawer active
            broadcast_game_start(server, room);
            return 0;
        }
        
        // Round không bắt đầu được (có thể do drawer không active hoặc game đã kết thúc)
        // Kiểm tra game đã kết thúc chưa (game_start_round có thể gọi game_end nếu là round cuối)
        if (room->game->game_ended) {
            // Game đã kết thúc, broadcast và return
            broadcast_game_end(server, room);
            room_end_game(room);
            return 0;
        }
        
        // Nếu vẫn còn round, thử chuyển drawer và bắt đầu lại
        // (game_start_round đã gọi game_end_round và chuyển drawer)
        attempts++;
    }
    
    // Nếu không thể bắt đầu round nào (tất cả drawer đều không active hoặc game đã kết thúc)
    if (room->game && !room->game->game_ended) {
        printf("[PROTOCOL] Khong the bat dau round: khong co drawer active. Ket thuc game.\n");
        game_end(room->game);
        broadcast_game_end(server, room);
        room_end_game(room);
    } else if (room->game && room->game->game_ended) {
        // Game đã kết thúc trong vòng lặp, broadcast
        broadcast_game_end(server, room);
        room_end_game(room);
    }
    
    return 0;
}



