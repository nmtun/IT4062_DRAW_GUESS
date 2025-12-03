#ifndef DRAWING_H
#define DRAWING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Drawing action types
typedef enum {
    DRAW_ACTION_MOVE = 0,
    DRAW_ACTION_LINE = 1,
    DRAW_ACTION_CLEAR = 2
} draw_action_type_t;

// Drawing action structure
typedef struct {
    draw_action_type_t action;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint32_t color;     // RGBA format
    uint8_t width;      // Độ rộng bút vẽ (1-20)
} draw_action_t;

// Canvas constraints
// MAX_CANVAS_WIDTH and MAX_CANVAS_HEIGHT specify the canvas dimensions in pixels.
// Valid coordinates are: x in [0, MAX_CANVAS_WIDTH-1], y in [0, MAX_CANVAS_HEIGHT-1]
#define MAX_CANVAS_WIDTH 1920   // Canvas width in pixels; valid x coordinates: 0 to MAX_CANVAS_WIDTH-1
#define MAX_CANVAS_HEIGHT 1080  // Canvas height in pixels; valid y coordinates: 0 to MAX_CANVAS_HEIGHT-1
#define MIN_BRUSH_WIDTH 1
#define MAX_BRUSH_WIDTH 20

/**
 * Parse dữ liệu vẽ từ payload nhị phân
 * @param payload Dữ liệu bytes thô từ network message
 * @param payload_len Độ dài của payload
 * @param action Cấu trúc draw_action_t đầu ra
 * @return 0 nếu thành công, -1 nếu lỗi
 */
int drawing_parse_action(const uint8_t* payload, size_t payload_len, draw_action_t* action);

/**
 * Serialize hành động vẽ sang định dạng nhị phân
 * @param action Cấu trúc draw_action_t đầu vào
 * @param buffer_out Buffer đầu ra (phải >= 14 bytes)
 * @return Số bytes đã ghi, hoặc -1 nếu lỗi
 */
int drawing_serialize_action(const draw_action_t* action, uint8_t* buffer_out);

/**
 * Kiểm tra tính hợp lệ của các tham số hành động vẽ
 * @param action Hành động vẽ cần kiểm tra
 * @return true nếu hợp lệ, false nếu không
 */
bool drawing_validate_action(const draw_action_t* action);

/**
 * Tạo hành động CLEAR (xóa canvas)
 * @param action Cấu trúc draw_action_t đầu ra
 */
void drawing_create_clear_action(draw_action_t* action);

/**
 * Tạo hành động LINE (vẽ đường)
 * @param x1, y1 Tọa độ điểm bắt đầu
 * @param x2, y2 Tọa độ điểm kết thúc
 * @param color Giá trị màu RGBA
 * @param width Độ rộng bút vẽ
 * @param action Cấu trúc draw_action_t đầu ra
 */
void drawing_create_line_action(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint32_t color, uint8_t width, draw_action_t* action);

#endif // DRAWING_H
