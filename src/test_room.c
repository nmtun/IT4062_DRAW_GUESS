#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "include/room.h"
#include "include/server.h"

void test_room_create() {
    printf("Test 1: Tạo phòng\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    assert(room != NULL);
    assert(room->room_id == 1);
    assert(room->owner_id == 1);
    assert(room->player_count == 1);
    assert(room->max_players == 4);
    assert(room->total_rounds == 3);
    assert(room->state == ROOM_WAITING);
    printf("Tạo phòng thành công\n");
    room_destroy(room);
}

void test_room_add_player() {
    printf("\nTest 2: Thêm người chơi\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    
    assert(room_add_player(room, 2) == true);
    assert(room->player_count == 2);
    assert(room_has_player(room, 2) == true);
    printf("Thêm player 2 thành công\n");
    
    assert(room_add_player(room, 3) == true);
    assert(room->player_count == 3);
    printf("Thêm player 3 thành công\n");
    
    // Test thêm duplicate
    assert(room_add_player(room, 2) == false);
    printf("Không thể thêm player trùng\n");
    
    room_destroy(room);
}

void test_room_full() {
    printf("\nTest 3: Phòng đầy\n");
    room_t* room = room_create("Phòng nhỏ", 1, 3, 5);
    
    room_add_player(room, 2);
    room_add_player(room, 3);
    assert(room_is_full(room) == true);
    printf("Phòng đã đầy\n");    
    
    assert(room_add_player(room, 4) == false);
    printf("Không thể thêm player khi phòng đầy\n");
    
    room_destroy(room);
}

void test_room_remove_player() {
    printf("\nTest 4: Xóa người chơi\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room_remove_player(room, 2) == true);
    assert(room->player_count == 2);
    assert(room_has_player(room, 2) == false);
    printf("Xóa player 2 thành công\n");
    
    // Test xóa player không tồn tại
    assert(room_remove_player(room, 999) == false);
    printf("Không thể xóa player không tồn tại\n");
    
    room_destroy(room);
}

void test_room_transfer_ownership() {
    printf("\nTest 5: Chuyển owner\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room_is_owner(room, 1) == true);
    assert(room_transfer_ownership(room, 2) == true);
    assert(room_is_owner(room, 2) == true);
    assert(room_is_owner(room, 1) == false);
    printf("Chuyển owner từ user 1 sang user 2 thành công\n");
    
    // Test chuyển owner cho người không trong phòng
    assert(room_transfer_ownership(room, 999) == false);
    printf("Không thể chuyển owner cho người không trong phòng\n");
    
    room_destroy(room);
}

void test_room_auto_transfer_ownership() {
    printf("\nTest 6: Tự động chuyển owner khi owner rời phòng\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    room_add_player(room, 2);
    room_add_player(room, 3);
    
    assert(room->owner_id == 1);
    room_remove_player(room, 1);
    assert(room->owner_id == 2);
    printf("Owner tự động chuyển sang player tiếp theo\n");
    
    room_destroy(room);
}

void test_room_start_game() {
    printf("\nTest 7: Bắt đầu game\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    
    // Test start game với 1 người
    assert(room_start_game(room) == false);
    printf("Không thể start game với 1 người\n");
    
    // Thêm người chơi thứ 2
    room_add_player(room, 2);
    assert(room_start_game(room) == true);
    assert(room->state == ROOM_PLAYING);
    printf("Start game thành công với 2 người\n");
    
    // Test không thể start game đang chơi
    assert(room_start_game(room) == false);
    printf("Không thể start game đang chơi\n");
    
    room_destroy(room);
}

void test_room_end_game() {
    printf("\nTest 8: Kết thúc game\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    room_add_player(room, 2);
    room_start_game(room);
    
    room_end_game(room);
    assert(room->state == ROOM_FINISHED);
    printf("Kết thúc game thành công\n");
    
    room_destroy(room);
}

void test_room_join_while_playing() {
    printf("\nTest 9: Không thể join phòng đang chơi\n");
    room_t* room = room_create("Phòng test", 1, 4, 3);
    room_add_player(room, 2);
    room_start_game(room);
    
    assert(room_add_player(room, 3) == false);
    printf("Không thể join phòng đang chơi\n");
    
    room_destroy(room);
}

void test_room_get_info() {
    printf("\nTest 10: Lấy thông tin phòng\n");
    room_t* room = room_create("Phòng VIP", 100, 8, 5);
    room_add_player(room, 101);
    room_add_player(room, 102);
    
    room_info_t info;
    room_get_info(room, &info);
    
    assert(info.room_id == room->room_id);
    assert(strcmp(info.room_name, "Phòng VIP") == 0);
    assert(info.player_count == 3);
    assert(info.max_players == 8);
    assert(info.state == ROOM_WAITING);
    assert(info.owner_id == 100);
    printf("Lấy thông tin phòng thành công\n");
    
    room_destroy(room);
}

void test_room_get_list() {
    printf("\nTest 11: Lấy danh sách phòng từ server\n");
    
    // Khởi tạo mock server
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    // Tạo 3 phòng
    server.rooms[0] = room_create("Phòng 1", 1, 4, 3);
    server.rooms[1] = room_create("Phòng 2", 2, 6, 5);
    server.rooms[2] = room_create("Phòng 3", 3, 8, 7);
    server.room_count = 3;
    
    // Lấy danh sách
    room_info_t room_list[10];
    int count = room_get_list(&server, room_list, 10);
    
    assert(count == 3);
    assert(strcmp(room_list[0].room_name, "Phòng 1") == 0);
    assert(strcmp(room_list[1].room_name, "Phòng 2") == 0);
    assert(strcmp(room_list[2].room_name, "Phòng 3") == 0);
    assert(room_list[0].player_count == 1);
    assert(room_list[1].max_players == 6);
    assert(room_list[2].owner_id == 3);
    printf("Lấy danh sách %d phòng thành công\n", count);
    
    // Cleanup
    room_destroy(server.rooms[0]);
    room_destroy(server.rooms[1]);
    room_destroy(server.rooms[2]);
}

void test_room_get_list_empty() {
    printf("\nTest 12: Lấy danh sách khi không có phòng\n");
    
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    room_info_t room_list[10];
    int count = room_get_list(&server, room_list, 10);
    
    assert(count == 0);
    printf("Danh sách rỗng khi không có phòng\n");
}

void test_room_get_list_with_limit() {
    printf("\nTest 13: Lấy danh sách với giới hạn\n");
    
    server_t server;
    memset(&server, 0, sizeof(server_t));
    
    // Tạo 5 phòng
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Phòng %d", i + 1);
        server.rooms[i] = room_create(name, i + 1, 4, 3);
    }
    server.room_count = 5;
    
    // Chỉ lấy 3 phòng đầu
    room_info_t room_list[3];
    int count = room_get_list(&server, room_list, 3);
    
    assert(count == 3);
    printf("Lấy đúng %d/%d phòng theo giới hạn\n", count, 5);
    
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
