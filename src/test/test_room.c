#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../include/room.h"
#include "../include/server.h"

void test_room_create() {
    printf("Test 1: Tao phong\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    assert(room != NULL);
    assert(room->room_id == 1);
    assert(room->owner_id == 1);
    assert(room->player_count == 1);
    assert(room->max_players == 4);
    assert(room->total_rounds == 3);
    assert(room->state == ROOM_WAITING);
    printf("Tao phong thanh cong\n");
    room_destroy(room);
}

void test_room_add_player() {
    printf("\nTest 2: Them nguoi choi\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    
    assert(room_add_player(room, 2) == true);
    assert(room->player_count == 2);
    assert(room_has_player(room, 2) == true);
    printf("Them player 2 thanh cong\n");
    
    assert(room_add_player(room, 3) == true);
    assert(room->player_count == 3);
    printf("Them player 3 thanh cong\n");
    
    // Test them duplicate
    assert(room_add_player(room, 2) == false);
    printf("Khong the them player trung\n");
    
    room_destroy(room);
}

void test_room_full() {
    printf("\nTest 3: Phong day\n");
    room_t* room = room_create("Phong nho", 1, 3, 5);
    
    room_add_player(room, 2);
    room_add_player(room, 3);
    assert(room_is_full(room) == true);
    printf("Phong da day\n");    
    
    assert(room_add_player(room, 4) == false);
    printf("Khong the them player khi phong day\n");
    
    room_destroy(room);
}

void test_room_remove_player() {
    printf("\nTest 4: Xoa nguoi choi\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room_remove_player(room, 2) == true);
    assert(room->player_count == 2);
    assert(room_has_player(room, 2) == false);
    printf("Xoa player 2 thanh cong\n");
    
    // Test xoa player khong ton tai
    assert(room_remove_player(room, 999) == false);
    printf("Khong the xoa player khong ton tai\n");
    
    room_destroy(room);
}

void test_room_transfer_ownership() {
    printf("\nTest 5: Chuyen owner\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room_is_owner(room, 1) == true);
    assert(room_transfer_ownership(room, 2) == true);
    assert(room_is_owner(room, 2) == true);
    assert(room_is_owner(room, 1) == false);
    printf("Chuyen owner tu user 1 sang user 2 thanh cong\n");
    
    // Test chuyen owner cho nguoi khong trong phong
    assert(room_transfer_ownership(room, 999) == false);
    printf("Khong the chuyen owner cho nguoi khong trong phong\n");
    
    room_destroy(room);
}

void test_room_auto_transfer_ownership() {
    printf("\nTest 6: Tu dong chuyen owner khi owner roi phong\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room->owner_id == 1);
    room_remove_player(room, 1);
    assert(room->owner_id == 2);
    printf("Owner tu dong chuyen sang player tiep theo\n");
    
    room_destroy(room);
}

void test_room_start_game() {
    printf("\nTest 7: Bat dau game\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    
    // Test start game voi 1 nguoi
    assert(room_start_game(room) == false);
    printf("Khong the start game voi 1 nguoi\n");
    
    // Them nguoi choi thu 2
    room_add_player(room, 2);
    assert(room_start_game(room) == true);
    assert(room->state == ROOM_PLAYING);
    printf("Start game thanh cong voi 2 nguoi\n");
    
    // Test khong the start game dang choi
    assert(room_start_game(room) == false);
    printf("Khong the start game dang choi\n");
    
    room_destroy(room);
}

void test_room_end_game() {
    printf("\nTest 8: Ket thuc game\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    room_add_player(room, 2);
    room_start_game(room);
    
    room_end_game(room);
    assert(room->state == ROOM_FINISHED);
    printf("Ket thuc game thanh cong\n");
    
    room_destroy(room);
}

void test_room_join_while_playing() {
    printf("\nTest 9: Khong the join phong dang choi\n");
    room_t* room = room_create("Phong test", 1, 4, 3);
    room_add_player(room, 2);
    room_start_game(room);
    
    assert(room_add_player(room, 3) == false);
    printf("Khong the join phong dang choi\n");
    
    room_destroy(room);
}

void test_room_get_info() {
    printf("\nTest 10: Lay thong tin phong\n");
    room_t* room = room_create("Phong VIP", 100, 8, 5);
    room_add_player(room, 101);
    room_add_player(room, 102);
    
    room_info_t info;
    room_get_info(room, &info);
    
    assert(info.room_id == room->room_id);
    assert(strcmp(info.room_name, "Phong VIP") == 0);
    assert(info.player_count == 3);
    assert(info.max_players == 8);
    assert(info.state == ROOM_WAITING);
    assert(info.owner_id == 100);
    printf("Lay thong tin phong thanh cong\n");
    
    room_destroy(room);
}

void test_room_get_list() {
    printf("\nTest 11: Lay danh sach phong tu server\n");
    
    // Khoi tao mock server
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    // Tao 3 phong
    server.rooms[0] = room_create("Phong 1", 1, 4, 3);
    server.rooms[1] = room_create("Phong 2", 2, 6, 5);
    server.rooms[2] = room_create("Phong 3", 3, 8, 7);
    server.room_count = 3;
    
    // Lay danh sach
    room_info_t room_list[10];
    int count = room_get_list(&server, room_list, 10);
    
    assert(count == 3);
    assert(strcmp(room_list[0].room_name, "Phong 1") == 0);
    assert(strcmp(room_list[1].room_name, "Phong 2") == 0);
    assert(strcmp(room_list[2].room_name, "Phong 3") == 0);
    assert(room_list[0].player_count == 1);
    assert(room_list[1].max_players == 6);
    assert(room_list[2].owner_id == 3);
    printf("Lay danh sach %d phong thanh cong\n", count);
    
    // Cleanup
    room_destroy(server.rooms[0]);
    room_destroy(server.rooms[1]);
    room_destroy(server.rooms[2]);
}

void test_room_get_list_empty() {
    printf("\nTest 12: Lay danh sach khi khong co phong\n");
    
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    room_info_t room_list[10];
    int count = room_get_list(&server, room_list, 10);
    
    assert(count == 0);
    printf("Danh sach rong khi khong co phong\n");
}

void test_room_get_list_with_limit() {
    printf("\nTest 13: Lay danh sach voi gioi han\n");
    
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    // Tao 5 phong
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Phong %d", i + 1);
        server.rooms[i] = room_create(name, i + 1, 4, 3);
    }
    server.room_count = 5;
    
    // Chi lay 3 phong dau
    room_info_t room_list[3];
    int count = room_get_list(&server, room_list, 3);
    
    assert(count == 3);
    printf("Lay dung %d/%d phong theo gioi han\n", count, 5);
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        room_destroy(server.rooms[i]);
    }
}

int main() {
    test_room_create();
    test_room_add_player();
    test_room_full();
    test_room_remove_player();
    test_room_transfer_ownership();
    test_room_auto_transfer_ownership();
    test_room_start_game();
    test_room_end_game();
    test_room_join_while_playing();
    test_room_get_info();
    test_room_get_list();
    test_room_get_list_empty();
    test_room_get_list_with_limit();

    return 0;
}
