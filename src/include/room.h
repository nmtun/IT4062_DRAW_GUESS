#ifndef ROOM_H
#define ROOM_H

#include <stdbool.h>
#include <time.h>

// Forward declarations
typedef struct game_state game_state_t;
typedef struct server server_t;

// Constants
#define MAX_PLAYERS_PER_ROOM 10
#define MIN_PLAYERS_PER_ROOM 2
#define ROOM_NAME_MAX_LENGTH 32
#define MAX_ROUNDS 10

// Room states
typedef enum
{
    ROOM_WAITING, // Đang chờ người chơi
    ROOM_PLAYING, // Đang chơi game
    ROOM_FINISHED // Game đã kết thúc
} room_state_t;

// Room structure
typedef struct
{
    int room_id;
    char room_name[ROOM_NAME_MAX_LENGTH];
    int owner_id;                             // User ID của người tạo phòng
    int players[MAX_PLAYERS_PER_ROOM];        // Mảng user IDs (bao gồm cả người chờ)
    int player_count;                         // Tổng số người trong phòng
    int active_players[MAX_PLAYERS_PER_ROOM]; // Mảng đánh dấu người chơi đang active (1) hay đang chờ (0)
    int max_players;                          // Số người chơi tối đa
    int total_rounds;                         // Số round trong game
    room_state_t state;                       // Trạng thái phòng
    game_state_t *game;                       // Con trỏ đến game state (NULL nếu chưa chơi)
    time_t created_at;                        // Thời gian tạo phòng
} room_t;

/**
 * Tạo phòng chơi mới
 * @param room_name Tên phòng
 * @param owner_id User ID của người tạo phòng
 * @param max_players Số người chơi tối đa (2-8)
 * @param rounds Số round trong game
 * @return Con trỏ đến room_t nếu thành công, NULL nếu thất bại
 */
room_t *room_create(const char *room_name, int owner_id, int max_players, int rounds);

/**
 * Hủy phòng chơi và giải phóng bộ nhớ
 * @param room Con trỏ đến room_t
 */
void room_destroy(room_t *room);

/**
 * Thêm người chơi vào phòng
 * @param room Con trỏ đến room_t
 * @param user_id User ID của người chơi
 * @return true nếu thành công, false nếu phòng đã đầy hoặc lỗi
 */
bool room_add_player(room_t *room, int user_id);

/**
 * Xóa người chơi khỏi phòng
 * @param room Con trỏ đến room_t
 * @param user_id User ID của người chơi cần xóa
 * @return true nếu thành công, false nếu không tìm thấy người chơi
 */
bool room_remove_player(room_t *room, int user_id);

/**
 * Kiểm tra xem người chơi có trong phòng không
 * @param room Con trỏ đến room_t
 * @param user_id User ID cần kiểm tra
 * @return true nếu có trong phòng, false nếu không
 */
bool room_has_player(room_t *room, int user_id);

/**
 * Kiểm tra xem người chơi có phải là owner không
 * @param room Con trỏ đến room_t
 * @param user_id User ID cần kiểm tra
 * @return true nếu là owner, false nếu không
 */
bool room_is_owner(room_t *room, int user_id);

/**
 * Kiểm tra xem phòng đã đầy chưa
 * @param room Con trỏ đến room_t
 * @return true nếu đã đầy, false nếu còn chỗ
 */
bool room_is_full(room_t *room);

/**
 * Chuyển quyền owner cho người chơi khác
 * @param room Con trỏ đến room_t
 * @param new_owner_id User ID của owner mới
 * @return true nếu thành công, false nếu user không trong phòng
 */
bool room_transfer_ownership(room_t *room, int new_owner_id);

/**
 * Bắt đầu game trong phòng
 * @param room Con trỏ đến room_t
 * @return true nếu thành công, false nếu không đủ điều kiện
 *
 * Yêu cầu: Phải có ít nhất 2 người chơi và phòng đang ở trạng thái WAITING
 */
bool room_start_game(room_t *room);

/**
 * Kết thúc game trong phòng
 * @param room Con trỏ đến room_t
 */
void room_end_game(room_t *room);

/**
 * Lấy thông tin cơ bản của phòng cho sảnh chờ
 * @param room Con trỏ đến room_t
 * @param room_info Con trỏ đến struct để lưu thông tin
 */
typedef struct
{
    int room_id;
    char room_name[ROOM_NAME_MAX_LENGTH];
    int player_count;
    int max_players;
    room_state_t state;
    int owner_id;
} room_info_t;

void room_get_info(room_t *room, room_info_t *room_info);

/**
 * Lấy danh sách tất cả phòng trong server
 * @param server Con trỏ đến server_t
 * @param room_info_array Mảng để lưu thông tin các phòng
 * @param max_rooms Kích thước tối đa của mảng
 * @return Số lượng phòng thực tế
 */
int room_get_list(server_t *server, room_info_t *room_info_array, int max_rooms);

#endif // ROOM_H
