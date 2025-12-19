#include "../include/room.h"
#include "../include/server.h"
#include "../include/game.h"
#include "../include/database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern db_connection_t* db;

// Room ID tự động tăng
static int next_room_id = 1;

// Tạo phòng chơi mới
room_t *room_create(const char *room_name, int owner_id, int max_players, int rounds)
{
    // Validate input
    if (!room_name || owner_id <= 0)
    {
        fprintf(stderr, "Tham số không hợp lệ khi tạo phòng\n");
        return NULL;
    }

    if (max_players < MIN_PLAYERS_PER_ROOM || max_players > MAX_PLAYERS_PER_ROOM)
    {
        fprintf(stderr, "Số người chơi phải từ %d-%d\n", MIN_PLAYERS_PER_ROOM, MAX_PLAYERS_PER_ROOM);
        return NULL;
    }

    if (rounds < 1 || rounds > MAX_ROUNDS)
    {
        fprintf(stderr, "Số round phải từ 1-%d\n", MAX_ROUNDS);
        return NULL;
    }

    // Cấp phát bộ nhớ cho phòng
    room_t *room = (room_t *)malloc(sizeof(room_t));
    if (!room)
    {
        fprintf(stderr, "Không thể cấp phát bộ nhớ cho phòng\n");
        return NULL;
    }

    // Khởi tạo phòng
    room->room_id = next_room_id++;
    room->db_room_id = 0;
    strncpy(room->room_name, room_name, ROOM_NAME_MAX_LENGTH - 1);
    room->room_name[ROOM_NAME_MAX_LENGTH - 1] = '\0';

    room->owner_id = owner_id;
    room->max_players = max_players;
    room->total_rounds = rounds;
    room->state = ROOM_WAITING;
    room->game = NULL;
    room->created_at = time(NULL);

    // Khởi tạo array người chơi
    room->player_count = 0;
    for (int i = 0; i < MAX_PLAYERS_PER_ROOM; i++)
    {
        room->players[i] = 0;
        room->db_player_ids[i] = 0;
        room->active_players[i] = 0;
    }

    // Thêm owner làm người chơi đầu tiên (active)
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

    printf("Phòng '%s' (ID: %d) đã được tạo bởi user %d\n",
           room->room_name, room->room_id, owner_id);

    return room;
}

// Hủy phòng chơi
void room_destroy(room_t *room)
{
    if (!room)
    {
        return;
    }

    printf("Đang hủy phòng '%s' (ID: %d)\n", room->room_name, room->room_id);

    // Free game state nếu có
    if (room->game)
    {
        game_destroy(room->game);
        room->game = NULL;
    }

    free(room);
}

// Thêm người chơi vào phòng
bool room_add_player(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    // Kiểm tra phòng đã đầy chưa
    if (room->player_count >= room->max_players)
    {
        fprintf(stderr, "Phòng '%s' đã đầy\n", room->room_name);
        return false;
    }

    // Kiểm tra người chơi đã trong phòng chưa
    if (room_has_player(room, user_id))
    {
        fprintf(stderr, "User %d đã có trong phòng\n", user_id);
        return false;
    }

    // Thêm player vào array người chơi
    room->players[room->player_count] = user_id;

    // Nếu phòng đang chơi, đánh dấu là đang chờ (inactive)
    // Sẽ được active vào round tiếp theo
    if (room->state == ROOM_PLAYING)
    {
        room->active_players[room->player_count] = 0; // Chờ đến round sau
        if (db && room->db_room_id > 0) {
            int db_player_id = db_add_room_player(db, room->db_room_id, user_id, room->player_count + 1);
            if (db_player_id > 0) room->db_player_ids[room->player_count] = db_player_id;
        }
        room->player_count++;
        printf("User %d đã tham gia phòng '%s' (ID: %d) và sẽ chơi từ round tiếp theo. Số người: %d/%d\n",
               user_id, room->room_name, room->room_id,
               room->player_count, room->max_players);
    }
    else
    {
        // Phòng chưa chơi, active ngay
        room->active_players[room->player_count] = 1;
        if (db && room->db_room_id > 0) {
            int db_player_id = db_add_room_player(db, room->db_room_id, user_id, room->player_count + 1);
            if (db_player_id > 0) room->db_player_ids[room->player_count] = db_player_id;
        }
        room->player_count++;
        printf("User %d đã tham gia phòng '%s' (ID: %d). Số người: %d/%d\n",
               user_id, room->room_name, room->room_id,
               room->player_count, room->max_players);
    }

    return true;
}

// Xóa người chơi khỏi phòng
bool room_remove_player(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    // Tìm index của player
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
        fprintf(stderr, "User %d không có trong phòng\n", user_id);
        return false;
    }

    // Xóa player bằng cách shift array người chơi
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

    printf("User %d đã rời phòng '%s' (ID: %d). Số người còn lại: %d\n",
           user_id, room->room_name, room->room_id, room->player_count);

    // Nếu là owner thì chuyển owner cho người khác
    if (user_id == room->owner_id && room->player_count > 0)
    {
        room->owner_id = room->players[0];
        printf("Quyền chủ phòng '%s' được chuyển cho user %d\n",
               room->room_name, room->owner_id);
    }

    // Nếu đang chơi game, cần xử lý logic game
    if (room->state == ROOM_PLAYING)
    {
        // Nếu còn < 2 người chơi thì end game ngay để tránh treo state
        if (room->player_count < 2) {
            room_end_game(room);
            printf("Game kết thúc do không đủ người chơi\n");
        } else {
            printf("Player rời phòng trong lúc đang chơi (server sẽ xử lý round tiếp theo nếu cần)\n");
        }
    }

    return true;
}

// Kiểm tra người chơi có trong phòng không
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

// Kiểm tra người chơi có phải là owner không
bool room_is_owner(room_t *room, int user_id)
{
    if (!room || user_id <= 0)
    {
        return false;
    }

    return room->owner_id == user_id;
}

// Kiểm tra phòng đã đầy chưa
bool room_is_full(room_t *room)
{
    if (!room)
    {
        return true;
    }

    return room->player_count >= room->max_players;
}

// Chuyển quyền owner cho người chơi khác
bool room_transfer_ownership(room_t *room, int new_owner_id)
{
    if (!room || new_owner_id <= 0)
    {
        return false;
    }

    // Kiểm tra new_owner có trong phòng không
    if (!room_has_player(room, new_owner_id))
    {
        fprintf(stderr, "User %d không có trong phòng\n", new_owner_id);
        return false;
    }

    int old_owner_id = room->owner_id;
    room->owner_id = new_owner_id;

    printf("Quyền chủ phòng '%s' được chuyển từ user %d sang user %d\n",
           room->room_name, old_owner_id, new_owner_id);

    return true;
}

// Bắt đầu game
bool room_start_game(room_t *room)
{
    if (!room)
    {
        return false;
    }

    // Kiểm tra trạng thái phòng
    if (room->state != ROOM_WAITING)
    {
        fprintf(stderr, "Phòng không ở trạng thái WAITING\n");
        return false;
    }

    // Kiểm tra số người chơi
    if (room->player_count < 2)
    {
        fprintf(stderr, "Cần ít nhất 2 người chơi để bắt đầu game\n");
        return false;
    }

    // Đánh dấu tất cả người chơi hiện tại là active
    for (int i = 0; i < room->player_count; i++)
    {
        room->active_players[i] = 1;
    }

    room->state = ROOM_PLAYING;

    printf("Game trong phòng '%s' (ID: %d) đã bắt đầu với %d người chơi\n",
           room->room_name, room->room_id, room->player_count);

    // Phase 5 - #18: Khởi tạo game_state_t (round sẽ được start ở handler protocol - mục 19)
    if (!room->game) {
        // yêu cầu mới: mỗi lượt vẽ 30s
        room->game = game_init(room, room->total_rounds, 30);
        if (!room->game) {
            fprintf(stderr, "Không thể khởi tạo game state\n");
            room->state = ROOM_WAITING;
            return false;
        }
    }

    return true;
}

// Kết thúc game
void room_end_game(room_t *room)
{
    if (!room)
    {
        return;
    }

    printf("Game trong phòng '%s' (ID: %d) đã kết thúc\n",
           room->room_name, room->room_id);

    // Free game state
    if (room->game)
    {
        game_destroy(room->game);
        room->game = NULL;
    }

    // Kết thúc trạng thái PLAYING
    room->state = ROOM_FINISHED;
}

// Lấy thông tin cơ bản của phòng cho sảnh chờ
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

// Lấy danh sách tất cả phòng trong server
int room_get_list(server_t *server, room_info_t *room_info_array, int max_rooms)
{
    if (!server || !room_info_array || max_rooms <= 0)
    {
        return 0;
    }

    int count = 0;

    // Duyệt qua tất cả rooms trong server
    for (int i = 0; i < 50 && count < max_rooms; i++)
    { // 50 là MAX_ROOMS
        if (server->rooms[i] != NULL)
        {
            room_get_info(server->rooms[i], &room_info_array[count]);
            count++;
        }
    }

    return count;
}
