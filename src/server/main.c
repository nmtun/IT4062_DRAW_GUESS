#include "../include/server.h"
#include "../include/database.h"
#include "../include/auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

server_t server;
db_connection_t* db = NULL;

// Xu ly tin hieu de dung server mot cach an toan
void signal_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    printf("\nNhan tin hieu dung, dang dong server...\n");
    if (db) {
        db_disconnect(db);
        db = NULL;
    }
    server_cleanup(&server);
    exit(0);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    
    // Doc port tu tham so dong lenh neu co
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Port khong hop le. Su dung port mac dinh: %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    // Dang ky xu ly tin hieu
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Ket noi den database
    // NOTE: pass the hostname (here localhost) as the first argument.
    // The code in database.c currently uses port 3308 when calling mysql_real_connect
    // so we connect to 127.0.0.1 (host) and port 3308 will be used inside db_connect.
    db = db_connect("127.0.0.1", "root", "123456", "draw_guess");
    if (!db) {
        fprintf(stderr, "Khong the ket noi den database. Server van se chay nhung khong co database.\n");
        // Tiep tuc chay server du khong co database
    } else {
        // Phase 5 - #17: load words vao database tu file
        // Thu mot vai path pho bien tuy theo working directory khi chay binary
        const char* candidates[] = {
            "src/data/words.txt",
            "data/words.txt",
            "../data/words.txt",
            NULL
        };
        int loaded = -1;
        int tried = 0;
        for (int i = 0; candidates[i]; i++) {
            loaded = db_load_words_from_file(db, candidates[i]);
            tried++;
            if (loaded >= 0) {
                // Thanh cong, khong can thu tiep
                break;
            }
        }
        // Chi in canh bao neu tat ca cac path deu that bai
        if (loaded < 0 && tried > 0) {
            fprintf(stderr, "Canh bao: Khong the tim thay file words.txt o bat ky vi tri nao da thu.\n");
        }
    }
    
    // Khoi tao server
    if (server_init(&server, port) < 0) {
        fprintf(stderr, "Khong the khoi tao server\n");
        return 1;
    }
    
    // Bat dau lang nghe
    if (server_listen(&server) < 0) {
        fprintf(stderr, "Khong the bat dau lang nghe\n");
        server_cleanup(&server);
        return 1;
    }
    
    // Test authentication module
    if (db) {
        
        // Test dang ky cho demo_user
        char hash[65];
        auth_hash_password("mypass123", hash);
        int user_id = db_register_user(db, "demo_user", hash);
        if(user_id > 0) {
            printf("Dang ky thanh cong: ID=%d\n", user_id);
        } else {
            printf("Dang ky that bai cho demo_user\n");
        }

        // Dang ky them tai khoan taphuc1 voi mat khau phuc1234
        char hash2[65];
        auth_hash_password("phuc1234", hash2);
        int user_id2 = db_register_user(db, "taphuc1", hash2);
        if(user_id2 > 0) {
            printf("Dang ky thanh cong: ID=%d cho tai khoan taphuc1\n", user_id2);
        } else {
            printf("Dang ky that bai cho tai khoan taphuc1\n");
        }
    }

    // Bat dau vong lap su kien
    server_event_loop(&server);
    
    // Don dep (thuong khong den day vi vong lap vo han)
    if (db) {
        db_disconnect(db);
        db = NULL;
    }
    server_cleanup(&server);
    
    return 0;
}

