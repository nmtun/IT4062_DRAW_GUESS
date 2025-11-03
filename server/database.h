#ifndef DATABASE_H
#define DATABASE_H

#include <mysql/mysql.h>

extern MYSQL *db_conn;

int db_connect(const char *host, const char *user, const char *pass, const char *dbname);
void db_close();
int db_register_user(const char *username, const char *password);
int db_login_user(const char *username, const char *password);

#endif
