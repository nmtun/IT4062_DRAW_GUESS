#include "../include/game.h"
#include "../include/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

// db global dang duoc khai bao trong main.c (server hien chua nhung db vao server_t)
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
    if (time_limit_seconds <= 0) time_limit_seconds = 60;

    game_state_t* game = (game_state_t*)calloc(1, sizeof(game_state_t));
    if (!game) return NULL;

    // Tinh tong so tu can: so nguoi choi * so vong
    int total_words_needed = room->player_count * total_rounds;
    if (total_words_needed > MAX_WORDS_STACK) total_words_needed = MAX_WORDS_STACK;

    game->room = room;
    game->current_round = 0;
    game->total_rounds = total_words_needed;
    game->time_limit = time_limit_seconds;
    game->drawer_id = -1;
    game->drawer_index = 0;
    game->current_word[0] = '\0';
    game->current_category[0] = '\0';
    game->word_length = 0;
    game->word_guessed = false;
    game->round_start_time = 0;
    game->game_ended = false;
    game->db_round_id = 0;
    game->word_stack_top = 0;
    game->word_stack_size = 0;

    // Pre-select từ: lấy tất cả từ theo difficulty, shuffle 5 lần, lấy n từ đầu
    if (db) {
        char all_words[500][64];
        char all_categories[500][64];
        int word_count = db_get_all_words_by_difficulty(db, room->difficulty, all_words, all_categories, 500);
        
        if (word_count > 0) {
            // Chuyển sang word_entry_t để shuffle
            word_entry_t temp_words[500];
            for (int i = 0; i < word_count && i < 500; i++) {
                strncpy(temp_words[i].word, all_words[i], 63);
                temp_words[i].word[63] = '\0';
                strncpy(temp_words[i].category, all_categories[i], 63);
                temp_words[i].category[63] = '\0';
            }
            
            // Shuffle 5 lần
            srand((unsigned int)time(NULL));
            for (int t = 0; t < 5; t++) {
                for (int i = 0; i < word_count; i++) {
                    int j = rand() % word_count;
                    // Swap
                    word_entry_t temp = temp_words[i];
                    temp_words[i] = temp_words[j];
                    temp_words[j] = temp;
                }
            }
            
            // Lấy n từ đầu tiên (n = total_words_needed)
            int words_to_take = (word_count < total_words_needed) ? word_count : total_words_needed;
            for (int i = 0; i < words_to_take; i++) {
                strncpy(game->word_stack[i].word, temp_words[i].word, 63);
                game->word_stack[i].word[63] = '\0';
                strncpy(game->word_stack[i].category, temp_words[i].category, 63);
                game->word_stack[i].category[63] = '\0';
            }
            game->word_stack_size = words_to_take;
            
            printf("[GAME] Pre-selected %d words from difficulty '%s' (total available: %d)\n",
                   words_to_take, room->difficulty, word_count);
        } else {
            // Fallback: nếu không có từ, tạo từ mặc định
            printf("[GAME] Warning: No words found for difficulty '%s', using fallback\n", room->difficulty);
            strncpy(game->word_stack[0].word, "cat", 63);
            game->word_stack[0].word[63] = '\0';
            strncpy(game->word_stack[0].category, "animal", 63);
            game->word_stack[0].category[63] = '\0';
            game->word_stack_size = 1;
        }
    } else {
        // Fallback nếu không có DB
        strncpy(game->word_stack[0].word, "cat", 63);
        game->word_stack[0].word[63] = '\0';
        strncpy(game->word_stack[0].category, "animal", 63);
        game->word_stack[0].category[63] = '\0';
        game->word_stack_size = 1;
    }

    // chon drawer_index ban dau: owner index neu co, fallback 0
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

    // Chuyen sang nguoi tiep theo theo thu tu vao phong
    // Thu tu ve se la thu tu vao phong cho don gian
    int next_idx = (game->drawer_index + 1) % n;
    
    // Tim nguoi active tiep theo neu nguoi tiep theo khong active
    for (int step = 0; step < n; step++) {
        int idx = (next_idx + step) % n;
        if (game->room->active_players[idx]) return idx;
    }

    // neu khong co ai active, fallback 0
    return 0;
}

static void assign_new_word(game_state_t* game) {
    if (!game) return;

    // Pop từ stack (FIFO - lấy từ đầu)
    if (game->word_stack_top < game->word_stack_size) {
        word_entry_t* entry = &game->word_stack[game->word_stack_top];
        safe_copy_word(game->current_word, sizeof(game->current_word), entry->word);
        safe_copy_word(game->current_category, sizeof(game->current_category), entry->category);
        game->word_length = (int)strlen(game->current_word);
        game->word_stack_top++;
        
        printf("[GAME] Pop word from stack: '%s' (category: '%s'), remaining: %d\n",
               game->current_word, game->current_category, 
               game->word_stack_size - game->word_stack_top);
    } else {
        // Stack rỗng, fallback
        printf("[GAME] Warning: Word stack is empty, using fallback\n");
        safe_copy_word(game->current_word, sizeof(game->current_word), "cat");
        safe_copy_word(game->current_category, sizeof(game->current_category), "animal");
        game->word_length = (int)strlen(game->current_word);
    }
}

bool game_start_round(game_state_t* game) {
    if (!game || !game->room) return false;
    if (game->game_ended) return false;

    // het round thi end game
    if (game->current_round >= game->total_rounds) {
        game_end(game);
        return false;
    }

    game->current_round++;

    // drawer_id theo drawer_index hien tai
    if (game->drawer_index < 0 || game->drawer_index >= game->room->player_count) {
        game->drawer_index = 0;
    }
    game->drawer_id = game->room->players[game->drawer_index];
    if (game->drawer_id <= 0) {
        // tim drawer hop le
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
        // timeout → end round that bai (khong ai thang round)
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

    // drawer khong duoc doan
    if (guesser_user_id == game->drawer_id) return false;

    if (!words_equal_case_insensitive(guess_word, game->current_word)) {
        return false;
    }

    // dung → cong diem
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

    // chuan bi drawer cho round tiep theo
    game->drawer_index = pick_next_drawer_index(game);

    // reset word/timer cho round hien tai (round moi se set lai)
    game->current_word[0] = '\0';
    game->current_category[0] = '\0';
    game->word_length = 0;
    game->round_start_time = 0;

    // het game?
    if (game->current_round >= game->total_rounds) {
        game_end(game);
        return;
    }

    // Round tiep theo se do caller goi (phase 19 se schedule/broadcast)
    (void)winner_user_id;
}

void game_end(game_state_t* game) {
    if (!game || game->game_ended) return;
    game->game_ended = true;
    int winner = compute_winner_user_id(game);
    printf("[GAME] Room %d game ended. Winner user_id=%d\n",
           game->room ? game->room->room_id : -1, winner);
}
