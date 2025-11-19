#include "../include/database.h"
#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  // <== thêm dòng này

db_connection_t* db_connect(const char* host, const char* user, 
                           const char* password, const char* database) {
    // Cấp phát bộ nhớ cho connection
    db_connection_t* db = (db_connection_t*)malloc(sizeof(db_connection_t));
    if (!db) {
        fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho database connection\n");
        return NULL;
    }
    
    // Khởi tạo MySQL connection
    db->conn = mysql_init(NULL);
    if (!db->conn) {
        fprintf(stderr, "Lỗi: Không thể khởi tạo MySQL connection\n");
        free(db);
        return NULL;
    }
    
    // Lưu thông tin kết nối
    strncpy(db->host, host, sizeof(db->host) - 1);
    db->host[sizeof(db->host) - 1] = '\0';
    
    strncpy(db->user, user, sizeof(db->user) - 1);
    db->user[sizeof(db->user) - 1] = '\0';
    
    strncpy(db->password, password, sizeof(db->password) - 1);
    db->password[sizeof(db->password) - 1] = '\0';
    
    strncpy(db->database, database, sizeof(db->database) - 1);
    db->database[sizeof(db->database) - 1] = '\0';
    
    // Kết nối đến MySQL server (port 3308 từ docker-compose mapping)
    if (!mysql_real_connect(db->conn, db->host, db->user, db->password, 
                           db->database, 3308, NULL, 0)) {
        fprintf(stderr, "Lỗi kết nối MySQL: %s\n", mysql_error(db->conn));
        mysql_close(db->conn);
        free(db);
        return NULL;
    }
    
    // Set charset utf8mb4
    if (mysql_set_character_set(db->conn, "utf8mb4")) {
        fprintf(stderr, "Cảnh báo: Không thể set charset utf8mb4: %s\n", 
                mysql_error(db->conn));
        // Không return NULL vì kết nối vẫn có thể hoạt động
    }
    
    printf("Đã kết nối thành công đến MySQL database: %s\n", db->database);
    
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
    printf("Đã đóng kết nối database\n");
}

MYSQL_RES* db_execute_query(db_connection_t* db, const char* query, ...) {
    if (!db || !db->conn || !query) {
        fprintf(stderr, "Lỗi: Tham số không hợp lệ\n");
        return NULL;
    }

    // Đếm số lượng placeholders (?) trong query
    int param_count = 0;
    const char* p = query;
    while (*p) {
        if (*p == '?') param_count++;
        p++;
    }

    // Nếu không có placeholder, thực thi query trực tiếp
    if (param_count == 0) {
        if (mysql_query(db->conn, query)) {
            fprintf(stderr, "Lỗi query: %s\n", mysql_error(db->conn));
            return NULL;
        }
        return mysql_store_result(db->conn);
    }

    // Thu thập tham số dạng chuỗi
    va_list args;
    va_start(args, query);

    const char** param_vals = (const char**)calloc(param_count, sizeof(char*));
    unsigned long* param_lens = (unsigned long*)calloc(param_count, sizeof(unsigned long));
    if (!param_vals || !param_lens) {
        fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho parameters\n");
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

    // Ước lượng kích thước buffer (escape tối đa ~ gấp đôi) + 2 dấu '
    size_t qlen = strlen(query);
    size_t buf_size = qlen + 1;
    for (int i = 0; i < param_count; i++) {
        buf_size += (param_lens[i] * 2) + 2;
    }

    char* final_sql = (char*)malloc(buf_size);
    if (!final_sql) {
        fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho final SQL\n");
        free(param_vals);
        free(param_lens);
        return NULL;
    }

    // Xây dựng câu SQL cuối cùng
    size_t out = 0;
    int pi = 0;
    for (size_t i = 0; i < qlen; i++) {
        if (query[i] == '?' && pi < param_count) {
            final_sql[out++] = '\'';
            // escape trực tiếp vào buffer
            out += mysql_real_escape_string(db->conn, final_sql + out, param_vals[pi], param_lens[pi]);
            final_sql[out++] = '\'';
            pi++;
        } else {
            final_sql[out++] = query[i];
        }
    }
    final_sql[out] = '\0';

    // Thực thi
    if (mysql_query(db->conn, final_sql)) {
        fprintf(stderr, "Lỗi query: %s\nSQL: %s\n", mysql_error(db->conn), final_sql);
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

int register_user(db_connection_t* db, const char* username, const char* password) {
    if (!db) {
        fprintf(stderr, "Không kết nối được tới cơ sở dữ liệu.\n");
        return -1;
    }

    if (!username) {
        fprintf(stderr, "Phải nhập tên người dùng.\n");
        return -1;
    }

    if (!password) {
        fprintf(stderr, "Phải nhập mật khẩu.\n");
        return -1;
    }

    // Kiểm tra username đã tồn tại chưa
    const char* check_query = "SELECT id FROM users WHERE username = ?";
    MYSQL_RES* check_res = db_execute_query(db, check_query, username);
    if (check_res && mysql_fetch_row(check_res)) {
        mysql_free_result(check_res);
        fprintf(stderr, "Tên người dùng đã tồn tại.\n");
        return -1;
    }
    if (check_res) mysql_free_result(check_res);
    
    const char* query = "INSERT INTO users (username, password) VALUES (?, ?)";
    MYSQL_RES* result = db_execute_query(db, query, username, password);
    
    // Kiểm tra lỗi
    if (mysql_errno(db->conn)) {
        fprintf(stderr, "Lỗi đăng ký người dùng: %s\n", mysql_error(db->conn));
        if ( result ) mysql_free_result(result);
        return -1;
    }
    
    mysql_free_result(result);
    return 0;
}

int login_user(db_connection_t* db, const char* username, const char* password) {
    if (!db) {
        fprintf(stderr, "Không kết nối được tới cơ sở dữ liệu.\n");
        return -1;
    }
    if (!username) {
        fprintf(stderr, "Phải nhập tên người dùng.\n");
        return -1;
    }
    if (!password) {
        fprintf(stderr, "Phải nhập mật khẩu.\n");
        return -1;
    }

    const char* query = "SELECT id FROM users WHERE username = ? AND password = ?";
    MYSQL_RES* result = db_execute_query(db, query, username, password);
    if (!result) {
        fprintf(stderr, "Lỗi truy vấn hoặc không có kết quả.\n");
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        mysql_free_result(result);
        return atoi(row[0]);  // Trả về id người dùng
    } else {
        mysql_free_result(result);
        fprintf(stderr, "Tên người dùng hoặc mật khẩu không đúng.\n");
        return -1;
    }
}
