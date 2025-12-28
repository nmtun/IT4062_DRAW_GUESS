#include "../include/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  // <== them dong nay
#include <ctype.h>

db_connection_t* db_connect(const char* host, const char* user, 
                           const char* password, const char* database) {
    // Cap phat bo nho cho connection
    db_connection_t* db = (db_connection_t*)malloc(sizeof(db_connection_t));
    if (!db) {
        fprintf(stderr, "Loi: Khong the cap phat bo nho cho database connection\n");
        return NULL;
    }
    
    // Khoi tao MySQL connection
    db->conn = mysql_init(NULL);
    if (!db->conn) {
        fprintf(stderr, "Loi: Khong the khoi tao MySQL connection\n");
        free(db);
        return NULL;
    }
    
    // Luu thong tin ket noi
    strncpy(db->host, host, sizeof(db->host) - 1);
    db->host[sizeof(db->host) - 1] = '\0';
    
    strncpy(db->user, user, sizeof(db->user) - 1);
    db->user[sizeof(db->user) - 1] = '\0';
    
    strncpy(db->password, password, sizeof(db->password) - 1);
    db->password[sizeof(db->password) - 1] = '\0';
    
    strncpy(db->database, database, sizeof(db->database) - 1);
    db->database[sizeof(db->database) - 1] = '\0';
    
    // Ket noi den MySQL server (port 3308 tu docker-compose mapping)
    if (!mysql_real_connect(db->conn, db->host, db->user, db->password, 
                           db->database, 3308, NULL, 0)) {
        fprintf(stderr, "Loi ket noi MySQL: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        free(db);
        return NULL;
    }
    
    // Set charset utf8mb4
    if (mysql_set_character_set(db->conn, "utf8mb4")) {
        fprintf(stderr, "Canh bao: Khong the set charset utf8mb4: %s\n", 
                mysql_error(db->conn));
        // Khong return NULL vi ket noi van co the hoat dong
    }
    
    printf("Da ket noi thanh cong den MySQL database: %s\n", db->database);

    // Best-effort schema ensure for Phase 6
    db_ensure_schema(db);
    
    return db;
}

void db_disconnect(db_connection_t* db) {
    if (!db) {
        return;
    }
    
    if (db->conn) {
        mysql_close(db->conn);
        db->conn = NULL;
    }
    
    free(db);
    printf("Da dong ket noi database\n");
}

/**
 * Kiểm tra và tự động kết nối lại nếu connection bị mất
 */
int db_check_and_reconnect(db_connection_t* db) {
    if (!db) {
        return 0;
    }
    
    // Nếu connection là NULL, tạo mới
    if (!db->conn) {
        fprintf(stderr, "MySQL connection is NULL, attempting to reconnect...\n");
        
        // Tạo connection mới
        db->conn = mysql_init(NULL);
        if (!db->conn) {
            fprintf(stderr, "Loi: Khong the khoi tao MySQL connection\n");
            return 0;
        }
        
        // Kết nối lại
        if (!mysql_real_connect(db->conn, db->host, db->user, db->password, 
                               db->database, 3308, NULL, 0)) {
            fprintf(stderr, "Loi ket noi MySQL: %s\n", mysql_error(db->conn));
            mysql_close(db->conn);
            db->conn = NULL;
            return 0;
        }
        
        // Set charset lại
        if (mysql_set_character_set(db->conn, "utf8mb4")) {
            fprintf(stderr, "Canh bao: Khong the set charset utf8mb4: %s\n", 
                    mysql_error(db->conn));
        }
        
        printf("Da ket noi lai thanh cong den MySQL database: %s\n", db->database);
        return 1;
    }
    
    // Kiểm tra connection còn sống
    if (mysql_ping(db->conn) != 0) {
        fprintf(stderr, "MySQL connection lost, attempting to reconnect...\n");
        
        // Đóng connection cũ
        mysql_close(db->conn);
        
        // Tạo connection mới
        db->conn = mysql_init(NULL);
        if (!db->conn) {
            fprintf(stderr, "Loi: Khong the khoi tao MySQL connection\n");
            return 0;
        }
        
        // Kết nối lại
        if (!mysql_real_connect(db->conn, db->host, db->user, db->password, 
                               db->database, 3308, NULL, 0)) {
            fprintf(stderr, "Loi ket noi MySQL: %s\n", mysql_error(db->conn));
            mysql_close(db->conn);
            db->conn = NULL;
            return 0;
        }
        
        // Set charset lại
        if (mysql_set_character_set(db->conn, "utf8mb4")) {
            fprintf(stderr, "Canh bao: Khong the set charset utf8mb4: %s\n", 
                    mysql_error(db->conn));
        }
        
        printf("Da ket noi lai thanh cong den MySQL database: %s\n", db->database);
        return 1;
    }
    
    return 1; // Connection vẫn OK
}

MYSQL_RES* db_execute_query(db_connection_t* db, const char* query, ...) {
    if (!db || !query) {
        fprintf(stderr, "Loi: Tham so khong hop le\n");
        return NULL;
    }
    
    // Kiểm tra và reconnect nếu cần
    if (!db_check_and_reconnect(db)) {
        fprintf(stderr, "Loi: Khong the ket noi den database\n");
        return NULL;
    }

    // Dem so luong placeholders (?) trong query
    int param_count = 0;
    const char* p = query;
    while (*p) {
        if (*p == '?') param_count++;
        p++;
    }

    // Neu khong co placeholder, thuc thi query truc tiep
    if (param_count == 0) {
        if (mysql_query(db->conn, query)) {
            fprintf(stderr, "Loi query: %s\n", mysql_error(db->conn));
            return NULL;
        }
        return mysql_store_result(db->conn);
    }

    // Thu thap tham so dang chuoi
    va_list args;
    va_start(args, query);

    const char** param_vals = (const char**)calloc(param_count, sizeof(char*));
    unsigned long* param_lens = (unsigned long*)calloc(param_count, sizeof(unsigned long));
    if (!param_vals || !param_lens) {
        fprintf(stderr, "Loi: Khong the cap phat bo nho cho parameters\n");
        if (param_vals) free(param_vals);
        if (param_lens) free(param_lens);
        va_end(args);
        return NULL;
    }

    for (int i = 0; i < param_count; i++) {
        const char* v = va_arg(args, const char*);
        if (!v) v = "";
        param_vals[i] = v;
        param_lens[i] = (unsigned long)strlen(v);
    }
    va_end(args);

    // Uoc luong kich thuoc buffer (escape toi da ~ gap doi) + 2 dau '
    size_t qlen = strlen(query);
    size_t buf_size = qlen + 1;
    for (int i = 0; i < param_count; i++) {
        buf_size += (param_lens[i] * 2) + 2;
    }

    char* final_sql = (char*)malloc(buf_size);
    if (!final_sql) {
        fprintf(stderr, "Loi: Khong the cap phat bo nho cho final SQL\n");
        free(param_vals);
        free(param_lens);
        return NULL;
    }

    // Xay dung cau SQL cuoi cung
    size_t out = 0;
    int pi = 0;
    for (size_t i = 0; i < qlen; i++) {
        if (query[i] == '?' && pi < param_count) {
            final_sql[out++] = '\'';
            // escape truc tiep vao buffer
            out += mysql_real_escape_string(db->conn, final_sql + out, param_vals[pi], param_lens[pi]);
            final_sql[out++] = '\'';
            pi++;
        } else {
            final_sql[out++] = query[i];
        }
    }
    final_sql[out] = '\0';

    // Thuc thi
    if (mysql_query(db->conn, final_sql)) {
        fprintf(stderr, "Loi query: %s\nSQL: %s\n", mysql_error(db->conn), final_sql);
        free(final_sql);
        free(param_vals);
        free(param_lens);
        return NULL;
    }

    MYSQL_RES* res = mysql_store_result(db->conn);

    free(final_sql);
    free(param_vals);
    free(param_lens);

    return res;
}

int db_register_user(db_connection_t* db, const char* username, 
                  const char* password_hash) {
    if (!db) {
        fprintf(stderr, "Khong ket noi duoc toi co so du lieu.\n");
        return -1;
    }

    if (!username || !password_hash) {
        fprintf(stderr, "Username va password khong duoc de trong.\n");
        return -1;
    }

    // Kiem tra username da ton tai chua
    const char* check_query = "SELECT id FROM users WHERE username = ?";
    MYSQL_RES* check_res = db_execute_query(db, check_query, username);
    if (check_res && mysql_fetch_row(check_res)) {
        mysql_free_result(check_res);
        fprintf(stderr, "Ten nguoi dung '%s' da ton tai.\n", username);
        return -1;
    }
    if (check_res) mysql_free_result(check_res);
    
    // Insert user moi
    const char* insert_query = "INSERT INTO users (username, password_hash) VALUES (?, ?)";
    MYSQL_RES* result = db_execute_query(db, insert_query, username, password_hash);
    
    if (mysql_errno(db->conn)) {
        fprintf(stderr, "Loi dang ky nguoi dung: %s\n", mysql_error(db->conn));
        if (result) mysql_free_result(result);
        return -1;
    }
    
    // Lay user_id vua insert
    int user_id = (int)mysql_insert_id(db->conn);
    
    if (result) mysql_free_result(result);
    
    printf("Dang ky thanh cong: user_id=%d, username=%s\n", user_id, username);
    return user_id;
}

int db_authenticate_user(db_connection_t* db, const char* username, 
               const char* password_hash) {
    if (!db) {
        fprintf(stderr, "Khong ket noi duoc toi co so du lieu.\n");
        return -1;
    }
    
    if (!username || !password_hash) {
        fprintf(stderr, "Username va password khong duoc de trong.\n");
        return -1;
    }

    // Xac thuc user
    const char* query = "SELECT id FROM users WHERE username = ? AND password_hash = ?";
    MYSQL_RES* result = db_execute_query(db, query, username, password_hash);
    
    if (!result) {
        fprintf(stderr, "Khong the thuc thi query xac thuc.\n");
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        fprintf(stderr, "Ten nguoi dung hoac mat khau khong dung.\n");
        return -1;
    }
    
    int user_id = atoi(row[0]);
    mysql_free_result(result);
    
    printf("Dang nhap thanh cong: user_id=%d, username=%s\n", user_id, username);
    return user_id;
}

// ---------------------------
// Words system (Phase 5 - #17)
// ---------------------------

static void str_trim_inplace(char* s) {
    if (!s) return;
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    if (start > 0) memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

static int difficulty_is_valid(const char* d) {
    if (!d) return 0;
    return strcmp(d, "easy") == 0 || strcmp(d, "medium") == 0 || strcmp(d, "hard") == 0;
}

int db_load_words_from_file(db_connection_t* db, const char* filepath) {
    if (!db || !db->conn || !filepath) {
        fprintf(stderr, "db_load_words_from_file: tham so khong hop le\n");
        return -1;
    }

    FILE* f = fopen(filepath, "r");
    if (!f) {
        // Khong in canh bao o day vi co the dang thu nhieu path
        // Caller se xu ly viec in canh bao neu tat ca deu that bai
        return -1;
    }

    char line[512];
    int inserted = 0;

    while (fgets(line, sizeof(line), f)) {
        // strip newline
        line[strcspn(line, "\r\n")] = '\0';
        str_trim_inplace(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        // Parse: word|difficulty|category
        char* word = line;
        char* diff = strchr(word, '|');
        char* cat = NULL;

        if (diff) {
            *diff++ = '\0';
            cat = strchr(diff, '|');
            if (cat) *cat++ = '\0';
        }

        str_trim_inplace(word);
        if (diff) str_trim_inplace(diff);
        if (cat) str_trim_inplace(cat);

        if (word[0] == '\0') continue;
        if (!diff || diff[0] == '\0') diff = "medium";
        if (!difficulty_is_valid(diff)) diff = "medium";
        if (!cat || cat[0] == '\0') cat = "general";

        // Upsert theo word
        // Luu y: db_execute_query chi bind string, nen ta truyen chuoi cho ENUM/difficulty luon duoc.
        const char* q =
            "INSERT INTO words (word, difficulty, category) "
            "VALUES (?, ?, ?) "
            "ON DUPLICATE KEY UPDATE difficulty=VALUES(difficulty), category=VALUES(category)";

        MYSQL_RES* r = db_execute_query(db, q, word, diff, cat);
        if (mysql_errno(db->conn)) {
            fprintf(stderr, "db_load_words_from_file: loi insert word='%s': %s\n", word, mysql_error(db->conn));
            if (r) mysql_free_result(r);
            continue;
        }
        if (r) mysql_free_result(r);
        inserted++;
    }

    fclose(f);
    printf("Words system: da nap %d tu tu '%s' vao database\n", inserted, filepath);
    return inserted;
}

int db_get_random_word(db_connection_t* db, const char* difficulty, char* out_word, size_t out_word_size) {
    if (!db || !db->conn || !out_word || out_word_size == 0) {
        fprintf(stderr, "db_get_random_word: tham so khong hop le\n");
        return -1;
    }

    out_word[0] = '\0';

    const int filter_by_diff = (difficulty && difficulty[0] != '\0' && difficulty_is_valid(difficulty));

    MYSQL_RES* res = NULL;
    if (filter_by_diff) {
        res = db_execute_query(db,
            "SELECT id, word FROM words WHERE difficulty = ? ORDER BY RAND() LIMIT 1",
            difficulty
        );
    } else {
        res = db_execute_query(db,
            "SELECT id, word FROM words ORDER BY RAND() LIMIT 1"
        );
    }

    if (!res) return -1;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row || !row[0] || !row[1]) {
        mysql_free_result(res);
        fprintf(stderr, "db_get_random_word: khong co tu phu hop trong database\n");
        return -1;
    }

    const char* id_str = row[0];
    const char* word = row[1];
    snprintf(out_word, out_word_size, "%s", word);
    mysql_free_result(res);

    // tang times_used
    MYSQL_RES* r2 = db_execute_query(db, "UPDATE words SET times_used = times_used + 1 WHERE id = ?", id_str);
    if (r2) mysql_free_result(r2);

    return 0;
}

int db_get_all_words_by_difficulty(db_connection_t* db, const char* difficulty, 
                                   char words[][64], char categories[][64], int max_words) {
    if (!db || !db->conn || !words || !categories || max_words <= 0) {
        fprintf(stderr, "db_get_all_words_by_difficulty: tham so khong hop le\n");
        return -1;
    }

    const int filter_by_diff = (difficulty && difficulty[0] != '\0' && difficulty_is_valid(difficulty));

    MYSQL_RES* res = NULL;
    if (filter_by_diff) {
        res = db_execute_query(db,
            "SELECT word, category FROM words WHERE difficulty = ? ORDER BY id",
            difficulty
        );
    } else {
        res = db_execute_query(db,
            "SELECT word, category FROM words ORDER BY id"
        );
    }

    if (!res) return -1;

    int count = 0;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) && count < max_words) {
        if (!row[0] || !row[1]) continue;
        
        const char* word = row[0];
        const char* category = row[1] ? row[1] : "general";
        
        strncpy(words[count], word, 63);
        words[count][63] = '\0';
        strncpy(categories[count], category, 63);
        categories[count][63] = '\0';
        count++;
    }

    mysql_free_result(res);
    return count;
}

// ---------------------------
// Phase 6 (21-24): Persistence
// ---------------------------

// Helper function to check if a column exists in a table
static int db_column_exists(db_connection_t* db, const char* table_name, const char* column_name) {
    if (!db || !db->conn || !table_name || !column_name) return 0;
    
    char query[512];
    snprintf(query, sizeof(query),
        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = DATABASE() "
        "AND TABLE_NAME = '%s' AND COLUMN_NAME = '%s'",
        table_name, column_name);
    
    MYSQL_RES* result = db_execute_query(db, query);
    if (!result) return 0;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    int exists = (row && row[0] && atoi(row[0]) > 0);
    mysql_free_result(result);
    
    return exists;
}

// Helper function to safely add a column if it doesn't exist
static void db_add_column_if_not_exists(db_connection_t* db, const char* table_name, 
                                        const char* column_name, const char* column_def) {
    if (!db || !db->conn || !table_name || !column_name || !column_def) return;
    
    if (!db_column_exists(db, table_name, column_name)) {
        char query[512];
        snprintf(query, sizeof(query), 
            "ALTER TABLE %s ADD COLUMN %s %s",
            table_name, column_name, column_def);
        db_execute_query(db, query);
    }
}

int db_ensure_schema(db_connection_t* db) {
    if (!db || !db->conn) return -1;

    // Add aggregate columns if missing (check first, then add)
    db_add_column_if_not_exists(db, "users", "total_games", "INT NOT NULL DEFAULT 0");
    db_add_column_if_not_exists(db, "users", "total_wins", "INT NOT NULL DEFAULT 0");
    db_add_column_if_not_exists(db, "users", "total_score", "INT NOT NULL DEFAULT 0");

    // Ensure rooms table exists (simple; may already exist)
    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS rooms ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "room_code VARCHAR(10) NOT NULL UNIQUE,"
        "host_id INT NOT NULL,"
        "max_players INT NOT NULL,"
        "total_rounds INT NOT NULL,"
        "status ENUM('waiting','in_progress','finished') DEFAULT 'waiting',"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"
    );

    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS room_players ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "room_id INT NOT NULL,"
        "user_id INT NOT NULL,"
        "join_order INT NOT NULL,"
        "score INT DEFAULT 0,"
        "is_ready BOOLEAN DEFAULT FALSE,"
        "connected BOOLEAN DEFAULT TRUE"
        ")"
    );

    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS game_rounds ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "room_id INT NOT NULL,"
        "round_number INT NOT NULL,"
        "turn_index INT NOT NULL,"
        "draw_id INT NOT NULL,"
        "word VARCHAR(100) NOT NULL,"
        "started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "ended_at TIMESTAMP NULL"
        ")"
    );

    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS guesses ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "round_id INT NOT NULL,"
        "player_id INT NOT NULL,"
        "guess_text VARCHAR(100) NOT NULL,"
        "is_correct BOOLEAN NOT NULL,"
        "guessed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"
    );

    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS score_details ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "round_id INT NOT NULL,"
        "player_id INT NOT NULL,"
        "score INT NOT NULL,"
        "awarded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"
    );

    // Chat messages (simple)
    db_execute_query(db,
        "CREATE TABLE IF NOT EXISTS chat_messages ("
        "id INT AUTO_INCREMENT PRIMARY KEY,"
        "room_id INT NOT NULL,"
        "player_id INT NOT NULL,"
        "message_text VARCHAR(500) NOT NULL,"
        "sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")"
    );

    return 0;
}

int db_create_room(db_connection_t* db, const char* room_code, int host_id, int max_players, int total_rounds) {
    if (!db || !db->conn || !room_code || host_id <= 0) return -1;
    char host_buf[32], max_buf[32], rounds_buf[32];
    snprintf(host_buf, sizeof(host_buf), "%d", host_id);
    snprintf(max_buf, sizeof(max_buf), "%d", max_players);
    snprintf(rounds_buf, sizeof(rounds_buf), "%d", total_rounds);

    MYSQL_RES* r = db_execute_query(db,
        "INSERT INTO rooms (room_code, host_id, max_players, total_rounds, status) VALUES (?, ?, ?, ?, 'waiting')",
        room_code, host_buf, max_buf, rounds_buf
    );
    if (r) mysql_free_result(r);
    if (mysql_errno(db->conn)) return -1;
    return (int)mysql_insert_id(db->conn);
}

int db_add_room_player(db_connection_t* db, int db_room_id, int user_id, int join_order) {
    if (!db || !db->conn || db_room_id <= 0 || user_id <= 0) return -1;
    char room_buf[32], user_buf[32], order_buf[32];
    snprintf(room_buf, sizeof(room_buf), "%d", db_room_id);
    snprintf(user_buf, sizeof(user_buf), "%d", user_id);
    snprintf(order_buf, sizeof(order_buf), "%d", join_order);

    MYSQL_RES* r = db_execute_query(db,
        "INSERT INTO room_players (room_id, user_id, join_order, score, is_ready, connected) VALUES (?, ?, ?, 0, 1, 1)",
        room_buf, user_buf, order_buf
    );
    if (r) mysql_free_result(r);
    if (mysql_errno(db->conn)) return -1;
    return (int)mysql_insert_id(db->conn);
}

int db_create_game_round(db_connection_t* db, int db_room_id, int round_number, int turn_index, int draw_player_db_id, const char* word) {
    if (!db || !db->conn || db_room_id <= 0 || draw_player_db_id <= 0 || !word) return -1;
    char room_buf[32], round_buf[32], turn_buf[32], draw_buf[32];
    snprintf(room_buf, sizeof(room_buf), "%d", db_room_id);
    snprintf(round_buf, sizeof(round_buf), "%d", round_number);
    snprintf(turn_buf, sizeof(turn_buf), "%d", turn_index);
    snprintf(draw_buf, sizeof(draw_buf), "%d", draw_player_db_id);

    MYSQL_RES* r = db_execute_query(db,
        "INSERT INTO game_rounds (room_id, round_number, turn_index, draw_id, word) VALUES (?, ?, ?, ?, ?)",
        room_buf, round_buf, turn_buf, draw_buf, word
    );
    if (r) mysql_free_result(r);
    if (mysql_errno(db->conn)) return -1;
    return (int)mysql_insert_id(db->conn);
}

int db_save_guess(db_connection_t* db, int db_round_id, int player_db_id, const char* guess_text, int is_correct) {
    if (!db || !db->conn || db_round_id <= 0 || player_db_id <= 0 || !guess_text) return -1;
    char round_buf[32], player_buf[32], correct_buf[8];
    snprintf(round_buf, sizeof(round_buf), "%d", db_round_id);
    snprintf(player_buf, sizeof(player_buf), "%d", player_db_id);
    snprintf(correct_buf, sizeof(correct_buf), "%d", is_correct ? 1 : 0);

    MYSQL_RES* r = db_execute_query(db,
        "INSERT INTO guesses (round_id, player_id, guess_text, is_correct) VALUES (?, ?, ?, ?)",
        round_buf, player_buf, guess_text, correct_buf
    );
    if (r) mysql_free_result(r);
    if (mysql_errno(db->conn)) return -1;
    return (int)mysql_insert_id(db->conn);
}

int db_save_score_detail(db_connection_t* db, int db_round_id, int player_db_id, int score) {
    if (!db || !db->conn || db_round_id <= 0 || player_db_id <= 0) return -1;
    char round_buf[32], player_buf[32], score_buf[32];
    snprintf(round_buf, sizeof(round_buf), "%d", db_round_id);
    snprintf(player_buf, sizeof(player_buf), "%d", player_db_id);
    snprintf(score_buf, sizeof(score_buf), "%d", score);

    MYSQL_RES* r = db_execute_query(db,
        "INSERT INTO score_details (round_id, player_id, score) VALUES (?, ?, ?)",
        round_buf, player_buf, score_buf
    );
    if (r) mysql_free_result(r);
    if (mysql_errno(db->conn)) return -1;
    return (int)mysql_insert_id(db->conn);
}

int db_update_room_player_score(db_connection_t* db, int player_db_id, int score) {
    if (!db || !db->conn || player_db_id <= 0) return -1;
    char pid_buf[32], score_buf[32];
    snprintf(pid_buf, sizeof(pid_buf), "%d", player_db_id);
    snprintf(score_buf, sizeof(score_buf), "%d", score);
    MYSQL_RES* r = db_execute_query(db, "UPDATE room_players SET score = ? WHERE id = ?", score_buf, pid_buf);
    if (r) mysql_free_result(r);
    return mysql_errno(db->conn) ? -1 : 0;
}

int db_update_user_stats(db_connection_t* db, int user_id, int score, int is_win) {
    if (!db || !db->conn || user_id <= 0) return -1;
    char uid_buf[32], score_buf[32], win_buf[8];
    snprintf(uid_buf, sizeof(uid_buf), "%d", user_id);
    snprintf(score_buf, sizeof(score_buf), "%d", score);
    snprintf(win_buf, sizeof(win_buf), "%d", is_win ? 1 : 0);

    MYSQL_RES* r = db_execute_query(db,
        "UPDATE users SET total_games = total_games + 1, total_wins = total_wins + ?, total_score = total_score + ? WHERE id = ?",
        win_buf, score_buf, uid_buf
    );
    if (r) mysql_free_result(r);
    return mysql_errno(db->conn) ? -1 : 0;
}
