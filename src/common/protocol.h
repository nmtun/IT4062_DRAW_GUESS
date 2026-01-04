#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// ============================================
// MESSAGE FORMAT
// ============================================
// [TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]
// LENGTH được lưu dưới dạng network byte order (big-endian)

// ============================================
// MESSAGE TYPES
// ============================================

// Authentication (0x01 - 0x0F)
#define MSG_LOGIN_REQUEST        0x01
#define MSG_LOGIN_RESPONSE       0x02
#define MSG_REGISTER_REQUEST     0x03
#define MSG_REGISTER_RESPONSE    0x04
#define MSG_LOGOUT               0x05
#define MSG_CHANGE_PASSWORD_REQUEST  0x06
#define MSG_CHANGE_PASSWORD_RESPONSE 0x07

// Room Management (0x10 - 0x1F)
#define MSG_ROOM_LIST_REQUEST    0x10
#define MSG_ROOM_LIST_RESPONSE   0x11
#define MSG_CREATE_ROOM          0x12
#define MSG_JOIN_ROOM            0x13
#define MSG_LEAVE_ROOM           0x14
#define MSG_ROOM_UPDATE          0x15
#define MSG_START_GAME           0x16
#define MSG_ROOM_PLAYERS_UPDATE  0x17  // Broadcast danh sách người chơi khi có thay đổi

// Game Play (0x20 - 0x2F)
#define MSG_GAME_START           0x20
#define MSG_GAME_STATE           0x21
#define MSG_DRAW_DATA            0x22
#define MSG_DRAW_BROADCAST       0x23
#define MSG_GUESS_WORD           0x24
#define MSG_CORRECT_GUESS        0x25
#define MSG_WRONG_GUESS          0x26
#define MSG_ROUND_END            0x27
#define MSG_GAME_END             0x28
#define MSG_HINT                 0x29
#define MSG_TIMER_UPDATE         0x2A  // Server gửi thời gian còn lại định kỳ

// Chat (0x30 - 0x3F)
#define MSG_CHAT_MESSAGE         0x30
#define MSG_CHAT_BROADCAST       0x31

// History (0x40 - 0x4F)
#define MSG_GET_GAME_HISTORY     0x40
#define MSG_GAME_HISTORY_RESPONSE 0x41

// System/Server (0x50 - 0x5F)
#define MSG_SERVER_SHUTDOWN      0x50  // Server thông báo shutdown đến tất cả clients
#define MSG_ACCOUNT_LOGGED_IN_ELSEWHERE 0x51  // Server thông báo tài khoản đang được đăng nhập ở nơi khác

// ============================================
// CONSTANTS
// ============================================
#define MAX_USERNAME_LEN         32
#define MAX_PASSWORD_LEN         32
#define MAX_EMAIL_LEN            64
#define MAX_MESSAGE_LEN          256
#define MAX_ROOM_NAME_LEN        32
#define MAX_WORD_LEN             64

// ============================================
// STATUS CODES
// ============================================
#define STATUS_SUCCESS           0x00
#define STATUS_ERROR             0x01
#define STATUS_INVALID_USERNAME  0x02
#define STATUS_INVALID_PASSWORD  0x03
#define STATUS_USER_EXISTS       0x04
#define STATUS_AUTH_FAILED       0x05

// ============================================
// MESSAGE STRUCTS
// ============================================

// Cấu trúc message tổng quát
typedef struct {
    uint8_t type;
    uint16_t length;
    uint8_t* payload;
} message_t;

// LOGIN_REQUEST payload structure
typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    char avatar[32];  // Avatar filename (e.g., "avt1.jpg")
} login_request_t;

// LOGIN_RESPONSE payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;         // STATUS_SUCCESS hoặc STATUS_ERROR
    int32_t user_id;        // -1 nếu thất bại
    char username[MAX_USERNAME_LEN];
} login_response_t;
#pragma pack()

// REGISTER_REQUEST payload structure
typedef struct {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    char email[MAX_EMAIL_LEN];
} register_request_t;

// REGISTER_RESPONSE payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;              // STATUS_SUCCESS hoặc STATUS_ERROR
    char message[128];            // Thông báo lỗi hoặc thành công
} register_response_t;
#pragma pack()

// CHANGE_PASSWORD_REQUEST payload structure
typedef struct {
    char old_password[MAX_PASSWORD_LEN];
    char new_password[MAX_PASSWORD_LEN];
} change_password_request_t;

// CHANGE_PASSWORD_RESPONSE payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;              // STATUS_SUCCESS hoặc STATUS_ERROR
    char message[128];            // Thông báo lỗi hoặc thành công
} change_password_response_t;
#pragma pack()

// Room state enum (phải match với room.h)
typedef enum {
    ROOM_STATE_WAITING = 0,
    ROOM_STATE_PLAYING = 1,
    ROOM_STATE_FINISHED = 2
} room_state_protocol_t;

// CREATE_ROOM request payload structure
typedef struct {
    char room_name[MAX_ROOM_NAME_LEN];
    uint8_t max_players;          // 2-10
    uint8_t rounds;               // 1-10
    char difficulty[16];          // "easy", "medium", "hard" (mặc định "easy")
} create_room_request_t;

// CREATE_ROOM response payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;              // STATUS_SUCCESS hoặc STATUS_ERROR
    int32_t room_id;             // -1 nếu thất bại
    char message[128];            // Thông báo lỗi hoặc thành công
} create_room_response_t;
#pragma pack()

// JOIN_ROOM request payload structure
typedef struct {
    int32_t room_id;
} join_room_request_t;

// JOIN_ROOM response payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;              // STATUS_SUCCESS hoặc STATUS_ERROR
    int32_t room_id;             // -1 nếu thất bại
    char message[128];            // Thông báo lỗi hoặc thành công
} join_room_response_t;
#pragma pack()

// LEAVE_ROOM request payload structure
typedef struct {
    int32_t room_id;
} leave_room_request_t;

// LEAVE_ROOM response payload structure
#pragma pack(1)
typedef struct {
    uint8_t status;              // STATUS_SUCCESS hoặc STATUS_ERROR
    char message[128];            // Thông báo lỗi hoặc thành công
} leave_room_response_t;
#pragma pack()

// Room info structure (dùng cho ROOM_LIST_RESPONSE và ROOM_UPDATE)
#pragma pack(push, 1)
typedef struct {
    int32_t room_id;
    char room_name[MAX_ROOM_NAME_LEN];
    uint8_t player_count;
    uint8_t max_players;
    uint8_t state;                // room_state_protocol_t
    int32_t owner_id;
    char owner_username[MAX_USERNAME_LEN];  // Username của chủ phòng
} room_info_protocol_t;
#pragma pack(pop)

// ROOM_LIST_RESPONSE payload structure
typedef struct {
    uint16_t room_count;          // Số lượng phòng
    // Sau đó là mảng room_info_protocol_t[room_count]
} room_list_response_t;

// ROOM_UPDATE payload structure (giống room_info_protocol_t)
// Sử dụng room_info_protocol_t trực tiếp

// Player info structure (dùng trong ROOM_PLAYERS_UPDATE)
#pragma pack(1)
typedef struct {
    int32_t user_id;
    char username[MAX_USERNAME_LEN];
    char avatar[32];  // Avatar filename (e.g., "avt1.jpg")
    uint8_t is_owner;  // 1 nếu là owner, 0 nếu không
    uint8_t is_active; // 1 = active, 0 = đang chờ, 255 (0xFF) = đã rời phòng
} player_info_protocol_t;
#pragma pack()

// ROOM_PLAYERS_UPDATE payload structure (broadcast khi có thay đổi)
// Bao gồm tất cả thông tin phòng + thông tin thay đổi người chơi
#pragma pack(1)
typedef struct {
    int32_t room_id;
    char room_name[MAX_ROOM_NAME_LEN];
    uint8_t max_players;
    uint8_t state;                // room_state_protocol_t
    int32_t owner_id;
    uint8_t action;              // 0 = JOIN, 1 = LEAVE
    int32_t changed_user_id;     // User ID của người join/leave
    char changed_username[MAX_USERNAME_LEN]; // Username của người join/leave
    uint16_t player_count;       // Số lượng người chơi hiện tại
    // Sau đó là mảng: player_info_protocol_t[player_count] (danh sách đầy đủ)
} room_players_update_t;
#pragma pack()

#endif // PROTOCOL_H

