#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <time.h>
#include "room.h"

// Forward declarations để tránh include vòng
typedef struct server server_t;

// Scoring (theo README: guesser +10, drawer +5)
#define GAME_POINTS_GUESSER 10
#define GAME_POINTS_DRAWER  5

// Maximum words in stack (đủ cho 10 người * 10 vòng = 100 từ)
#define MAX_WORDS_STACK 200

typedef struct {
    int user_id;
    int score;
    int words_guessed;
    int rounds_won;
} player_score_t;

// Structure để lưu từ và category trong stack
typedef struct {
    char word[64];
    char category[64];
} word_entry_t;

typedef struct game_state {
    room_t* room;
    int current_round;
    int total_rounds;
    int drawer_id;
    int drawer_index; // index trong room->players[]
    char current_word[64];
    char current_category[64]; // Category của từ hiện tại
    int word_length;
    bool word_guessed;
    time_t round_start_time;
    int time_limit; // seconds
    int db_round_id; // game_rounds.id hiện tại (0 nếu chưa persist)
    player_score_t scores[MAX_PLAYERS_PER_ROOM];
    int score_count;
    bool game_ended;
    // Stack để lưu từ đã chọn trước
    word_entry_t word_stack[MAX_WORDS_STACK];
    int word_stack_top; // Index của từ tiếp theo sẽ pop (0 = từ đầu tiên)
    int word_stack_size; // Số lượng từ trong stack
    // Track những người đã đoán đúng trong round hiện tại
    int guessed_user_ids[MAX_PLAYERS_PER_ROOM];
    int guessed_count; // Số người đã đoán đúng (dùng để tính thứ tự và điểm)
} game_state_t;

/**
 * Khởi tạo game state cho 1 phòng.
 * Không tự start round; caller sẽ gọi game_start_round().
 */
game_state_t* game_init(room_t* room, int total_rounds, int time_limit_seconds);

/**
 * Giải phóng game state.
 */
void game_destroy(game_state_t* game);

/**
 * Bắt đầu một round mới: tăng current_round, chọn drawer, lấy word, reset timer.
 * (Broadcast protocol sẽ được implement ở mục 19; module này chỉ update state.)
 *
 * @return true nếu start được round, false nếu game đã end hoặc room invalid.
 */
bool game_start_round(game_state_t* game);

/**
 * Kiểm tra timeout round; nếu quá hạn thì tự end round (success=false).
 *
 * @return true nếu round đã bị timeout và bị end, false nếu chưa.
 */
bool game_check_timeout(game_state_t* game);

/**
 * Xử lý guess word của 1 user.
 * @return true nếu guess đúng và round bị end, false nếu guess sai/ignored.
 */
bool game_handle_guess(game_state_t* game, int guesser_user_id, const char* guess_word);

/**
 * Kết thúc round hiện tại, update state, quyết định start round tiếp theo hay end game.
 *
 * @param success true nếu có người đoán đúng, false nếu timeout.
 */
void game_end_round(game_state_t* game, bool success, int winner_user_id);

/**
 * Kết thúc game, set game_ended.
 */
void game_end(game_state_t* game);

#endif // GAME_H
