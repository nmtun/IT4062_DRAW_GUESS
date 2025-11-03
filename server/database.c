#include <stdio.h>
#include <mysql/mysql.h>
#include <string.h>
#include "database.h"

MYSQL *db_conn;

int db_connect(const char *host, unsigned int port, const char *user, const char *pass, const char *dbname) {
    db_conn = mysql_init(NULL);
    if (db_conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return 0;
    }

    if (mysql_real_connect(db_conn, host, user, pass, dbname, port, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(db_conn));
        mysql_close(db_conn);
        return 0;
    }

    printf("[DB] Connected to MySQL successfully.\n");
    return 1;
}

void db_close() {
    if (db_conn) mysql_close(db_conn);
}

// int db_register_user(const char *username, const char *password) {
//     char query[256];
//     sprintf(query, "INSERT INTO users(username, password, score_total) VALUES('%s', '%s', 0)", username, password);
//     if (mysql_query(db_conn, query)) {
//         fprintf(stderr, "[DB] Insert failed: %s\n", mysql_error(db_conn));
//         return 0;
//     }
//     return 1;
// }

int db_login_user(const char *username, const char *password) {
    char query[256];
    sprintf(query, "SELECT id FROM users WHERE username='%s' AND password='%s'", username, password);
    if (mysql_query(db_conn, query)) {
        fprintf(stderr, "[DB] Query failed: %s\n", mysql_error(db_conn));
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db_conn);
    if (!res) return -1;

    int user_id = -1;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) user_id = atoi(row[0]);

    mysql_free_result(res);
    return user_id;
}
