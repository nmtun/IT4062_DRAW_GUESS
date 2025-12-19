#include "../include/game.h"
#include "../include/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// db global đang được khai báo trong main.c (server hiện chưa nhúng db vào server_t)
extern db_connection_t* db;

static void str_to_lower_ascii(char* s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void safe_copy_word(char* dst, size_t dst_sz, const char* src) {
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    snprintf(dst, dst_sz, "%s", src);
}

static int find_score_index(game_state_t* game, int user_id) {
    if (!game) return -1;
    for (int i = 0; i < game->score_count; i++) {
        if (game->scores[i].user_id == user_id) return i;
    }
    return -1;
}

static void ensure_scores_initialized(game_state_t* game) {
    if (!game || !game->room) return;

    game->score_count = 0;
    for (int i = 0; i < game->room->player_count && i < MAX_PLAYERS_PER_ROOM; i++) {
        const int uid = game->room->players[i];
        if (uid <= 0) continue;
        game->scores[game->score_count].user_id = uid;
        game->scores[game->score_count].score = 0;
        game->scores[game->score_count].words_guessed = 0;
        game->scores[game->score_count].rounds_won = 0;
        game->score_count++;
    }
}

game_state_t* game_init(room_t* room, int total_rounds, int time_limit_seconds) {
    if (!room) return NULL;
    if (total_rounds <= 0) total_rounds = 1;
    if (total_rounds > MAX_ROUNDS) total_rounds = MAX_ROUNDS;
    if (time_limit_seconds <= 0) time_limit_seconds = 60;

    game_state_t* game = (game_state_t*)calloc(1, sizeof(game_state_t));
    if (!game) return NULL;

    game->room = room;
    game->current_round = 0;
    game->total_rounds = total_rounds;
    game->time_limit = time_limit_seconds;
    game->drawer_id = -1;
    game->drawer_index = 0;
    game->current_word[0] = '\0';
    game->word_length = 0;
    game->word_guessed = false;
    game->round_start_time = 0;
    game->game_ended = false;
    game->db_round_id = 0;

    // chọn drawer_index ban đầu: owner index nếu có, fallback 0
    int owner_idx = -1;
    for (int i = 0; i < room->player_count; i++) {
        if (room->players[i] == room->owner_id) {
            owner_idx = i;
            break;
        }
    }
    game->drawer_index = (owner_idx >= 0) ? owner_idx : 0;

    ensure_scores_initialized(game);
    return game;
}

void game_destroy(game_state_t* game) {
    if (!game) return;
    free(game);
}

static int pick_next_drawer_index(game_state_t* game) {
    if (!game || !game->room) return -1;
    const int n = game->room->player_count;
    if (n <= 0) return -1;

    // tìm user active tiếp theo (active_players[i] == 1)
    for (int step = 1; step <= n; step++) {
        int idx = (game->drawer_index + step) % n;
        if (game->room->active_players[idx]) return idx;
    }

    // nếu không có ai active, fallback 0
    return 0;
}

static void assign_new_word(game_state_t* game) {
    if (!game) return;

    char word[64] = {0};
    int ok = -1;

    // Tạm thời: chọn difficulty theo round (1/3 easy, 1/3 medium, 1/3 hard)
    const char* diff = "medium";
    if (game->total_rounds > 0) {
        int phase = (game->current_round - 1) % 3;
        diff = (phase == 0) ? "easy" : (phase == 1) ? "medium" : "hard";
    }

    if (db) {
        ok = db_get_random_word(db, diff, word, sizeof(word));
    }

    if (ok != 0 || word[0] == '\0') {
        // fallback nếu DB không sẵn sàng
        safe_copy_word(word, sizeof(word), "cat");
    }

    safe_copy_word(game->current_word, sizeof(game->current_word), word);
    game->word_length = (int)strlen(game->current_word);
}

bool game_start_round(game_state_t* game) {
    if (!game || !game->room) return false;
    if (game->game_ended) return false;

    // hết round thì end game
    if (game->current_round >= game->total_rounds) {
        game_end(game);
        return false;
    }

    game->current_round++;

    // drawer_id theo drawer_index hiện tại
    if (game->drawer_index < 0 || game->drawer_index >= game->room->player_count) {
        game->drawer_index = 0;
    }
    game->drawer_id = game->room->players[game->drawer_index];
    if (game->drawer_id <= 0) {
        // tìm drawer hợp lệ
        game->drawer_index = pick_next_drawer_index(game);
        game->drawer_id = (game->drawer_index >= 0) ? game->room->players[game->drawer_index] : -1;
    }

    assign_new_word(game);
    game->word_guessed = false;
    game->round_start_time = time(NULL);

    printf("[GAME] Room %d start round %d/%d, drawer=%d, word_len=%d\n",
           game->room->room_id, game->current_round, game->total_rounds, game->drawer_id, game->word_length);

    return true;
}

bool game_check_timeout(game_state_t* game) {
    if (!game || game->game_ended) return false;
    if (game->round_start_time == 0) return false;

    time_t now = time(NULL);
    if ((int)(now - game->round_start_time) > game->time_limit) {
        // timeout → end round thất bại (không ai thắng round)
        game_end_round(game, false, -1);
        return true;
    }
    return false;
}

static bool words_equal_case_insensitive(const char* a, const char* b) {
    if (!a || !b) return false;
    char aa[128], bb[128];
    safe_copy_word(aa, sizeof(aa), a);
    safe_copy_word(bb, sizeof(bb), b);
    str_to_lower_ascii(aa);
    str_to_lower_ascii(bb);
    return strcmp(aa, bb) == 0;
}

bool game_handle_guess(game_state_t* game, int guesser_user_id, const char* guess_word) {
    if (!game || !game->room || game->game_ended) return false;
    if (!guess_word || guess_word[0] == '\0') return false;
    if (game->drawer_id <= 0 || game->current_word[0] == '\0') return false;

    // drawer không được đoán
    if (guesser_user_id == game->drawer_id) return false;

    if (!words_equal_case_insensitive(guess_word, game->current_word)) {
        return false;
    }

    // đúng → cộng điểm
    int gidx = find_score_index(game, guesser_user_id);
    if (gidx >= 0) {
        game->scores[gidx].score += GAME_POINTS_GUESSER;
        game->scores[gidx].words_guessed += 1;
        game->scores[gidx].rounds_won += 1;
    }

    int didx = find_score_index(game, game->drawer_id);
    if (didx >= 0) {
        game->scores[didx].score += GAME_POINTS_DRAWER;
    }

    game->word_guessed = true;
    game_end_round(game, true, guesser_user_id);
    return true;
}

static int compute_winner_user_id(game_state_t* game) {
    if (!game || game->score_count <= 0) return -1;
    int best_uid = game->scores[0].user_id;
    int best_score = game->scores[0].score;
    for (int i = 1; i < game->score_count; i++) {
        if (game->scores[i].score > best_score) {
            best_score = game->scores[i].score;
            best_uid = game->scores[i].user_id;
        }
    }
    return best_uid;
}

void game_end_round(game_state_t* game, bool success, int winner_user_id) {
    if (!game || !game->room || game->game_ended) return;

    printf("[GAME] Room %d end round %d: %s, word='%s'\n",
           game->room->room_id, game->current_round, success ? "SUCCESS" : "TIMEOUT", game->current_word);

    // chuẩn bị drawer cho round tiếp theo
    game->drawer_index = pick_next_drawer_index(game);

    // reset word/timer cho round hiện tại (round mới sẽ set lại)
    game->current_word[0] = '\0';
    game->word_length = 0;
    game->round_start_time = 0;

    // hết game?
    if (game->current_round >= game->total_rounds) {
        game_end(game);
        return;
    }

    // Round tiếp theo sẽ do caller gọi (phase 19 sẽ schedule/broadcast)
    (void)winner_user_id;
}

void game_end(game_state_t* game) {
    if (!game || game->game_ended) return;
    game->game_ended = true;
    int winner = compute_winner_user_id(game);
    printf("[GAME] Room %d game ended. Winner user_id=%d\n",
           game->room ? game->room->room_id : -1, winner);
}
