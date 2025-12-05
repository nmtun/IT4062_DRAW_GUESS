#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "../common/protocol.h"
#include "server.h"
#include "database.h"

/**
 * Parse message từ buffer nhận được
 * @param buffer Buffer chứa raw data từ socket
 * @param buffer_len Độ dài của buffer
 * @param msg_out Con trỏ đến message_t để lưu kết quả parse
 * @return 0 nếu thành công, -1 nếu lỗi
 * 
 * Note: Hàm này sẽ cấp phát bộ nhớ cho msg_out->payload
 * Caller phải free() sau khi sử dụng xong
 */
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out);

/**
 * Tạo message theo format protocol
 * @param type Message type
 * @param payload Dữ liệu payload
 * @param payload_len Độ dài payload
 * @param buffer_out Buffer để lưu message đã tạo (phải đủ lớn)
 * @return Tổng độ dài message (3 + payload_len) nếu thành công, -1 nếu lỗi
 */
int protocol_create_message(uint8_t type, const uint8_t* payload, uint16_t payload_len, uint8_t* buffer_out);

/**
 * Xử lý message nhận được từ client
 * @param server Con trỏ đến server_t
 * @param client_index Index của client trong server->clients[]
 * @param msg Message đã được parse
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_handle_message(server_t* server, int client_index, const message_t* msg);

/**
 * Gửi LOGIN_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param status Status code (STATUS_SUCCESS hoặc STATUS_ERROR)
 * @param user_id User ID (hoặc -1 nếu thất bại)
 * @param username Username
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_login_response(int client_fd, uint8_t status, int32_t user_id, const char* username);

/**
 * Gửi REGISTER_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param status Status code (STATUS_SUCCESS hoặc STATUS_ERROR)
 * @param message Thông báo (lỗi hoặc thành công)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_register_response(int client_fd, uint8_t status, const char* message);

/**
 * Helper function: Gửi message đến client
 * @param client_fd File descriptor của client socket
 * @param type Message type
 * @param payload Payload data
 * @param payload_len Payload length
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len);

/**
 * Gửi ROOM_LIST_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param server Con trỏ đến server_t để lấy danh sách phòng
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_room_list(int client_fd, server_t* server);

/**
 * Gửi ROOM_UPDATE đến tất cả clients trong phòng
 * @param server Con trỏ đến server_t
 * @param room Con trỏ đến room_t cần broadcast
 * @param exclude_client_index Client index cần loại trừ (hoặc -1 nếu không loại trừ)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_broadcast_room_update(server_t* server, room_t* room, int exclude_client_index);

/**
 * Gửi CREATE_ROOM_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param status Status code (STATUS_SUCCESS hoặc STATUS_ERROR)
 * @param room_id Room ID (hoặc -1 nếu thất bại)
 * @param message Thông báo (lỗi hoặc thành công)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_create_room_response(int client_fd, uint8_t status, int32_t room_id, const char* message);

/**
 * Gửi JOIN_ROOM_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param status Status code (STATUS_SUCCESS hoặc STATUS_ERROR)
 * @param room_id Room ID (hoặc -1 nếu thất bại)
 * @param message Thông báo (lỗi hoặc thành công)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_join_room_response(int client_fd, uint8_t status, int32_t room_id, const char* message);

/**
 * Gửi LEAVE_ROOM_RESPONSE đến client
 * @param client_fd File descriptor của client socket
 * @param status Status code (STATUS_SUCCESS hoặc STATUS_ERROR)
 * @param message Thông báo (lỗi hoặc thành công)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_send_leave_room_response(int client_fd, uint8_t status, const char* message);

/**
 * Broadcast ROOM_PLAYERS_UPDATE đến tất cả clients trong phòng
 * Gửi danh sách đầy đủ người chơi khi có thay đổi
 * @param server Con trỏ đến server_t
 * @param room Con trỏ đến room_t
 * @param action 0 = JOIN, 1 = LEAVE
 * @param changed_user_id User ID của người join/leave
 * @param changed_username Username của người join/leave
 * @param exclude_client_index Client index cần loại trừ (hoặc -1 nếu không loại trừ)
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int protocol_broadcast_room_players_update(server_t* server, room_t* room, 
                                           uint8_t action, int changed_user_id, 
                                           const char* changed_username, 
                                           int exclude_client_index);

/**
 * Broadcast ROOM_LIST_RESPONSE đến tất cả clients đã đăng nhập
 * Gọi khi có phòng mới được tạo hoặc phòng bị xóa
 * @param server Con trỏ đến server_t
 * @return Số lượng clients đã nhận được message
 */
int protocol_broadcast_room_list(server_t* server);

#endif // PROTOCOL_HANDLER_H

