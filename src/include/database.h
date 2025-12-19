#ifndef DATABASE_H
#define DATABASE_H

#include <mysql/mysql.h>
#include <stdarg.h>
#include <stddef.h>

// Cấu trúc lưu thông tin kết nối database
typedef struct {
    MYSQL* conn;
    char host[64];
    char user[32];
    char password[64];
    char database[32];
} db_connection_t;

/**
 * Kết nối đến MySQL database
 * @param host Địa chỉ host MySQL
 * @param user Tên người dùng
 * @param password Mật khẩu
 * @param database Tên database
 * @return Con trỏ đến db_connection_t nếu thành công, NULL nếu thất bại
 */
db_connection_t* db_connect(const char* host, const char* user, 
                           const char* password, const char* database);

/**
 * Đóng kết nối database và giải phóng tài nguyên
 * @param db Con trỏ đến db_connection_t
 */
void db_disconnect(db_connection_t* db);

/**
 * Thực thi query với prepared statement
 * @param db Con trỏ đến db_connection_t
 * @param query Câu lệnh SQL với placeholders (?)
 * @param ... Các tham số để bind vào placeholders (chỉ hỗ trợ string)
 * @return MYSQL_RES* nếu thành công, NULL nếu thất bại
 * 
 * Ví dụ:
 *   db_execute_query(db, "SELECT * FROM users WHERE username = ?", "john");
 *   db_execute_query(db, "INSERT INTO users (username, password) VALUES (?, ?)", "user1", "pass1");
 */
MYSQL_RES* db_execute_query(db_connection_t* db, const char* query, ...);

/**
 * Đăng ký người dùng mới
 * @param db Con trỏ đến db_connection_t
 * @param username Tên người dùng
 * @param password_hash Mật khẩu đã hash (SHA256)
 * @return user_id nếu thành công, -1 nếu thất bại
*/
int db_register_user(db_connection_t* db, const char* username, const char* password_hash);

/**
 * Xác thực người dùng (login)
 * @param db Con trỏ đến db_connection_t
 * @param username Tên người dùng
 * @param password_hash Mật khẩu đã hash (SHA256)
 * @return user_id nếu thành công, -1 nếu thất bại
 */
int db_authenticate_user(db_connection_t* db, const char* username, const char* password_hash);

/**
 * Nạp danh sách từ từ file vào database (bảng words).
 *
 * Format mỗi dòng (bỏ qua dòng trống hoặc bắt đầu bằng #):
 *   word|difficulty|category
 *
 * difficulty: easy | medium | hard
 * category: chuỗi tự do (vd: animal, object, ...)
 *
 * @return số lượng từ insert/update thành công, -1 nếu lỗi.
 */
int db_load_words_from_file(db_connection_t* db, const char* filepath);

/**
 * Lấy 1 từ ngẫu nhiên theo difficulty (nếu difficulty == NULL hoặc rỗng thì lấy bất kỳ).
 * Hàm sẽ tăng times_used cho từ được chọn.
 *
 * @return 0 nếu thành công, -1 nếu thất bại (không có từ / lỗi DB).
 */
int db_get_random_word(db_connection_t* db, const char* difficulty, char* out_word, size_t out_word_size);

// ============================
// Phase 6 (21-24): Persistence APIs
// ============================

// Ensure required columns/tables exist (best-effort). Called from db_connect.
int db_ensure_schema(db_connection_t* db);

// Create a persistent room record. Returns rooms.id or -1.
int db_create_room(db_connection_t* db, const char* room_code, int host_id, int max_players, int total_rounds);

// Add a player to room_players; returns room_players.id or -1.
int db_add_room_player(db_connection_t* db, int db_room_id, int user_id, int join_order);

// Create a game_rounds row; returns game_rounds.id or -1.
int db_create_game_round(db_connection_t* db, int db_room_id, int round_number, int turn_index, int draw_player_db_id, const char* word);

// Save a guess row (guesses); returns guesses.id or -1.
int db_save_guess(db_connection_t* db, int db_round_id, int player_db_id, const char* guess_text, int is_correct);

// Save score detail row (score_details); returns id or -1.
int db_save_score_detail(db_connection_t* db, int db_round_id, int player_db_id, int score);

// Update final scores in room_players for a room.
int db_update_room_player_score(db_connection_t* db, int player_db_id, int score);

// Update users aggregate stats (total_games/total_wins/total_score).
int db_update_user_stats(db_connection_t* db, int user_id, int score, int is_win);

#endif // DATABASE_H
