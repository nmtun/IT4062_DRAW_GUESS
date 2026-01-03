#include "../include/room.h"
#include "../include/server.h"
#include "../include/game.h"
#include "../include/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern db_connection_t* db;

// Room ID tu dong tang
static int next_room_id = 1;

// Tao phong choi moi
room_t *room_create(const char *room_name, int owner_id, int max_players, int rounds, const char *difficulty)
{
    // Validate input
    if (!room_name || owner_id <= 0)
    {
        fprintf(stderr, "Tham so khong hop le khi tao phong\n");
        return NULL;
    }

    if (max_players < MIN_PLAYERS_PER_ROOM || max_players > MAX_PLAYERS_PER_ROOM)
    {
        fprintf(stderr, "So nguoi choi phai tu %d-%d\n", MIN_PLAYERS_PER_ROOM, MAX_PLAYERS_PER_ROOM);
        return NULL;
    }

    if (rounds < 1 || rounds > MAX_ROUNDS)
    {
        fprintf(stderr, "So round phai tu 1-%d\n", MAX_ROUNDS);
        return NULL;
    }

    // Cap phat bo nho cho phong
    room_t *room = (room_t *)malloc(sizeof(room_t));
    if (!room)
    {
        fprintf(stderr, "Khong the cap phat bo nho cho phong\n");
        return NULL;
    }

    // Khoi tao phong
    room->room_id = next_room_id++;
    room->db_room_id = 0;
    strncpy(room->room_name, room_name, ROOM_NAME_MAX_LENGTH - 1);
    room->room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';

    room->owner_id = owner_id;
    room->max_players = max_players;
    room->total_rounds = rounds;
    
    // Set difficulty (mặc định "easy" nếu không có hoặc không hợp lệ)
    if (difficulty && difficulty[0] != '\0') {
        if (strcmp(difficulty, "easy") == 0 || strcmp(difficulty, "medium") == 0 || strcmp(difficulty, "hard") == 0) {
            strncpy(room->difficulty, difficulty, sizeof(room->difficulty) - 1);
            room->difficulty[sizeof(room->difficulty) - 1] = '\0';
        } else {
            strncpy(room->difficulty, "easy", sizeof(room->difficulty) - 1);
            room->difficulty[sizeof(room->difficulty) - 1] = '\0';
        }
    } else {
        strncpy(room->difficulty, "easy", sizeof(room->difficulty) - 1);
        room->difficulty[sizeof(room->difficulty) - 1] = '\0';
    }
    
    room->state = ROOM_WAITING;
    room->game = NULL;
    room->created_at = time(NULL);

    // Khoi tao array nguoi choi
    room->player_count = 0;
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++)
    {
        room->players[i] = 0;
        room->db_player_ids[i] = 0;
        room->active_players[i] = 0;
    }

    // Them owner lam nguoi choi dau tien (active)
    room->players[0] = owner_id;
    room->active_players[0] = 1; // Owner active ngay
    room->player_count = 1;

    // Persist room + owner player (best-effort)
    if (db) {
        char code[16];
        snprintf(code, sizeof(code), "R%d", room->room_id);
        int db_room_id = db_create_room(db, code, owner_id, max_players, rounds);
        if (db_room_id > 0) {
            room->db_room_id = db_room_id;
            int db_player_id = db_add_room_player(db, db_room_id, owner_id, 1);
            if (db_player_id > 0) {
                room->db_player_ids[0] = db_player_id;
            }
        }
    }

    printf("Phong '%s' (ID: %d) da duoc tao boi user %d\n",
           room->room_name, room->room_id, owner_id);

    return room;
}

// Huy phong choi
void room_destroy(room_t *room)
{
    if (!room)
    {
        return;
    }

    printf("Dang huy phong '%s' (ID: %d)\n", room->room_name, room->room_id);

    // Free game state neu co
    if (room->game)
    {
        game_destroy(room->game);
        room->game = NULL;
    }

    free(room);
}

// Them nguoi choi vao phong
bool room_add_player(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    // Kiem tra phong da day chua
    if (room->player_count >= room->max_players)
    {
        fprintf(stderr, "Phong '%s' da day\n", room->room_name);
        return false;
    }

    // Kiem tra nguoi choi da trong phong chua
    if (room_has_player(room, user_id))
    {
        fprintf(stderr, "User %d da co trong phong\n", user_id);
        return false;
    }

    // Them player vao array nguoi choi
    room->players[room->player_count] = user_id;

    // Neu phong dang choi, danh dau la dang cho (inactive)
    // Se duoc active vao round tiep theo
    if (room->state == ROOM_PLAYING)
    {
        room->active_players[room->player_count] = 0; // Cho den round sau
        if (db && room->db_room_id > 0) {
            int db_player_id = db_add_room_player(db, room->db_room_id, user_id, room->player_count + 1);
            if (db_player_id > 0) room->db_player_ids[room->player_count] = db_player_id;
        }
        room->player_count++;
        printf("User %d da tham gia phong '%s' (ID: %d) va se choi tu round tiep theo. So nguoi: %d/%d\n",
               user_id, room->room_name, room->room_id,
               room->player_count, room->max_players);
    }
    else
    {
        // Phong chua choi, active ngay
        room->active_players[room->player_count] = 1;
        if (db && room->db_room_id > 0) {
            int db_player_id = db_add_room_player(db, room->db_room_id, user_id, room->player_count + 1);
            if (db_player_id > 0) room->db_player_ids[room->player_count] = db_player_id;
        }
        room->player_count++;
        printf("User %d da tham gia phong '%s' (ID: %d). So nguoi: %d/%d\n",
               user_id, room->room_name, room->room_id,
               room->player_count, room->max_players);
    }

    return true;
}

// Xoa nguoi choi khoi phong
bool room_remove_player(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    // Tim index cua player
    int player_index = -1;
    for (int i = 0; i < room->player_count; i++)
    {
        if (room->players[i] == user_id)
        {
            player_index = i;
            break;
        }
    }

    if (player_index == -1)
    {
        fprintf(stderr, "User %d khong co trong phong\n", user_id);
        return false;
    }

    // Neu dang choi game, khong xoa nguoi choi khoi mảng mà chỉ đánh dấu inactive
    // để vẫn hiển thị trong bảng điểm
    if (room->state == ROOM_PLAYING)
    {
        // Đánh dấu inactive (-1 = đã rời phòng)
        room->active_players[player_index] = -1;
        
        printf("User %d da roi phong '%s' (ID: %d) trong luc dang choi. Danh dau inactive.\n",
               user_id, room->room_name, room->room_id);
        
        // Nếu owner rời phòng, chuyển owner cho người chơi active đầu tiên
        if (user_id == room->owner_id) {
            int new_owner_id = -1;
            for (int i = 0; i < room->player_count; i++) {
                if (room->active_players[i] == 1) {
                    new_owner_id = room->players[i];
                    break;
                }
            }
            if (new_owner_id > 0) {
                room->owner_id = new_owner_id;
                printf("Quyen chu phong '%s' duoc chuyen cho user %d (owner cu roi phong trong luc choi)\n",
                       room->room_name, room->owner_id);
            } else {
                printf("Khong co nguoi choi active nao de chuyen owner. Phong se bi xoa khi game ket thuc.\n");
            }
        }
        
        // Đếm số người chơi active (không tính người đã rời)
        int active_count = 0;
        for (int i = 0; i < room->player_count; i++) {
            if (room->active_players[i] == 1) {
                active_count++;
            }
        }
        
        // Neu con < 2 nguoi choi active thi end game ngay de tranh treo state
        if (active_count < 2) {
            room_end_game(room);
            printf("Game ket thuc do khong du nguoi choi active\n");
        }
        
        return true;
    }

    // Neu khong phai dang choi game, xoa nguoi choi khoi mảng như bình thường
    // Xoa player bang cach shift array nguoi choi
    for (int i = player_index; i < room->player_count - 1; i++)
    {
        room->players[i] = room->players[i + 1];
        room->active_players[i] = room->active_players[i + 1];
        room->db_player_ids[i] = room->db_player_ids[i + 1];
    }
    room->players[room->player_count - 1] = 0;
    room->active_players[room->player_count - 1] = 0;
    room->db_player_ids[room->player_count - 1] = 0;
    room->player_count--;

    printf("User %d da roi phong '%s' (ID: %d). So nguoi con lai: %d\n",
           user_id, room->room_name, room->room_id, room->player_count);

    // Neu la owner thi chuyen owner cho nguoi chơi active đầu tiên
    if (user_id == room->owner_id && room->player_count > 0)
    {
        int new_owner_id = -1;
        // Tìm người chơi active đầu tiên (không phải inactive)
        for (int i = 0; i < room->player_count; i++) {
            // Chỉ chọn người chơi active (active_players[i] == 1)
            // Không chọn người đã rời (active_players[i] == -1) hoặc đang chờ (active_players[i] == 0)
            if (room->active_players[i] == 1) {
                new_owner_id = room->players[i];
                break;
            }
        }
        
        if (new_owner_id > 0) {
            room->owner_id = new_owner_id;
            printf("Quyen chu phong '%s' duoc chuyen cho user %d\n",
                   room->room_name, room->owner_id);
        } else {
            // Không có người chơi active nào, owner vẫn là user_id cũ (sẽ xử lý khi xóa phòng)
            printf("Khong co nguoi choi active nao de chuyen owner trong phong '%s'\n",
                   room->room_name);
        }
    }

    return true;
}

// Kiem tra nguoi choi co trong phong khong
bool room_has_player(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    for (int i = 0; i < room->player_count; i++)
    {
        if (room->players[i] == user_id)
        {
            return true;
        }
    }

    return false;
}

// Kiem tra nguoi choi co phai la owner khong
bool room_is_owner(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    return room->owner_id == user_id;
}

// Kiem tra phong da day chua
bool room_is_full(room_t *room)
{
    if (!room)
    {
        return true;
    }

    return room->player_count >= room->max_players;
}

// Chuyen quyen owner cho nguoi choi khac
bool room_transfer_ownership(room_t *room, int new_owner_id)
{
    if (!room || new_owner_id <= 0)
    {
        return false;
    }

    // Kiem tra new_owner co trong phong khong
    if (!room_has_player(room, new_owner_id))
    {
        fprintf(stderr, "User %d khong co trong phong\n", new_owner_id);
        return false;
    }

    int old_owner_id = room->owner_id;
    room->owner_id = new_owner_id;

    printf("Quyen chu phong '%s' duoc chuyen tu user %d sang user %d\n",
           room->room_name, old_owner_id, new_owner_id);

    return true;
}

// Đảm bảo owner là người chơi active, nếu không thì chuyển cho người chơi active đầu tiên
// Trả về true nếu owner hợp lệ hoặc đã chuyển thành công, false nếu không có người chơi active nào
bool room_ensure_active_owner(room_t *room)
{
    if (!room) return false;
    
    // Kiểm tra owner hiện tại có active không
    bool owner_is_active = false;
    for (int i = 0; i < room->player_count; i++) {
        if (room->players[i] == room->owner_id && room->active_players[i] == 1) {
            owner_is_active = true;
            break;
        }
    }
    
    if (owner_is_active) {
        return true; // Owner đã active, không cần làm gì
    }
    
    // Tìm người chơi active đầu tiên để chuyển owner
    int new_owner_id = -1;
    for (int i = 0; i < room->player_count; i++) {
        if (room->active_players[i] == 1) {
            new_owner_id = room->players[i];
            break;
        }
    }
    
    if (new_owner_id > 0) {
        int old_owner_id = room->owner_id;
        room->owner_id = new_owner_id;
        printf("Owner cua phong '%s' (ID: %d) duoc chuyen tu user %d sang user %d (owner cu khong active)\n",
               room->room_name, room->room_id, old_owner_id, new_owner_id);
        return true;
    }
    
    // Không có người chơi active nào
    return false;
}

// Bat dau game
bool room_start_game(room_t *room)
{
    if (!room)
    {
        return false;
    }

    // Kiem tra trang thai phong
    if (room->state != ROOM_WAITING)
    {
        fprintf(stderr, "Phong khong o trang thai WAITING\n");
        return false;
    }

    // Kiem tra so nguoi choi
    if (room->player_count < 2)
    {
        fprintf(stderr, "Can it nhat 2 nguoi choi de bat dau game\n");
        return false;
    }

    // Danh dau tat ca nguoi choi hien tai la active
    for (int i = 0; i < room->player_count; i++)
    {
        room->active_players[i] = 1;
    }

    room->state = ROOM_PLAYING;

    printf("Game trong phong '%s' (ID: %d) da bat dau voi %d nguoi choi\n",
           room->room_name, room->room_id, room->player_count);

    // Phase 5 - #18: Khoi tao game_state_t (round se duoc start o handler protocol - muc 19)
    if (!room->game) {
        // yeu cau moi: moi luot ve 30s
        room->game = game_init(room, room->total_rounds, 30);
        if (!room->game) {
            fprintf(stderr, "Khong the khoi tao game state\n");
            room->state = ROOM_WAITING;
            return false;
        }
    }

    return true;
}

// Ket thuc game
void room_end_game(room_t *room)
{
    if (!room)
    {
        return;
    }

    printf("Game trong phong '%s' (ID: %d) da ket thuc\n",
           room->room_name, room->room_id);

    // Free game state
    if (room->game)
    {
        game_destroy(room->game);
        room->game = NULL;
    }

    // Ket thuc trang thai PLAYING
    room->state = ROOM_FINISHED;
}

// Lay thong tin co ban cua phong cho sanh cho
void room_get_info(room_t *room, room_info_t *room_info)
{
    if (!room || !room_info)
    {
        return;
    }

    room_info->room_id = room->room_id;
    strncpy(room_info->room_name, room->room_name, ROOM_NAME_MAX_LENGTH - 1);
    room_info->room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';
    room_info->player_count = room->player_count;
    room_info->max_players = room->max_players;
    room_info->state = room->state;
    room_info->owner_id = room->owner_id;
}

// Lay danh sach tat ca phong trong server
int room_get_list(server_t *server, room_info_t *room_info_array, int max_rooms)
{
    if (!server || !room_info_array || max_rooms <= 0)
    {
        return 0;
    }

    int count = 0;

    // Duyet qua tat ca rooms trong server
    for (int i = 0; i < 50 && count < max_rooms; i++)
    { // 50 la MAX_ROOMS
        if (server->rooms[i] != NULL)
        {
            room_get_info(server->rooms[i], &room_info_array[count]);
            count++;
        }
    }

    return count;
}
