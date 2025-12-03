#include "../include/drawing.h"
#include <string.h>
#include <arpa/inet.h>

/**
 * Parse dữ liệu vẽ từ payload nhị phân
 * Format: [action:1][x1:2][y1:2][x2:2][y2:2][color:4][width:1] = 14 bytes
 */
int drawing_parse_action(const uint8_t *payload, size_t payload_len, draw_action_t *action)
{
    if (!payload || !action)
    {
        return -1;
    }

    // Kiểm tra độ dài payload
    if (payload_len < 14)
    {
        return -1;
    }

    // Parse action type
    action->action = (draw_action_type_t)payload[0];

    // Parse tọa độ (network byte order)
    uint16_t x1_net, y1_net, x2_net, y2_net;
    memcpy(&x1_net, payload + 1, 2);
    memcpy(&y1_net, payload + 3, 2);
    memcpy(&x2_net, payload + 5, 2);
    memcpy(&y2_net, payload + 7, 2);

    action->x1 = ntohs(x1_net);
    action->y1 = ntohs(y1_net);
    action->x2 = ntohs(x2_net);
    action->y2 = ntohs(y2_net);

    // Parse color (network byte order)
    uint32_t color_net;
    memcpy(&color_net, payload + 9, 4);
    action->color = ntohl(color_net);

    // Parse width
    action->width = payload[13];

    // Validate
    if (!drawing_validate_action(action))
    {
        return -1;
    }

    return 0;
}

/**
 * Serialize dữ liệu vẽ sang định dạng nhị phân
 */
int drawing_serialize_action(const draw_action_t *action, uint8_t *buffer_out)
{
    if (!action || !buffer_out)
    {
        return -1;
    }

    if (!drawing_validate_action(action))
    {
        return -1;
    }

    // Action type
    buffer_out[0] = (uint8_t)action->action;

    // Tọa độ (chuyển sang network byte order)
    uint16_t x1_net = htons(action->x1);
    uint16_t y1_net = htons(action->y1);
    uint16_t x2_net = htons(action->x2);
    uint16_t y2_net = htons(action->y2);

    memcpy(buffer_out + 1, &x1_net, 2);
    memcpy(buffer_out + 3, &y1_net, 2);
    memcpy(buffer_out + 5, &x2_net, 2);
    memcpy(buffer_out + 7, &y2_net, 2);

    // Color (chuyển sang network byte order)
    uint32_t color_net = htonl(action->color);
    memcpy(buffer_out + 9, &color_net, 4);

    // Width
    buffer_out[13] = action->width;

    return 14; // Tổng số bytes đã ghi
}

/**
 * Kiểm tra tính hợp lệ của các tham số hành động vẽ
 */
bool drawing_validate_action(const draw_action_t *action)
{
    if (!action)
    {
        return false;
    }

    // Kiểm tra loại hành động
    if (action->action != DRAW_ACTION_MOVE &&
        action->action != DRAW_ACTION_LINE &&
        action->action != DRAW_ACTION_CLEAR)
    {
        return false;
    }

    // Với hành động CLEAR, tọa độ và độ rộng không quan trọng
    if (action->action == DRAW_ACTION_CLEAR)
    {
        return true;
    }

    // Kiểm tra tọa độ
    if (action->x1 >= MAX_CANVAS_WIDTH || action->y1 >= MAX_CANVAS_HEIGHT ||
        action->x2 >=  MAX_CANVAS_WIDTH || action->y2 >= MAX_CANVAS_HEIGHT)
    {
        return false;
    }

    // Kiểm tra độ rộng bút vẽ
    if (action->width < MIN_BRUSH_WIDTH || action->width > MAX_BRUSH_WIDTH)
    {
        return false;
    }

    return true;
}

/**
 * Tạo hành động CLEAR (xóa canvas)
 */
void drawing_create_clear_action(draw_action_t *action)
{
    if (!action)
    {
        return;
    }

    memset(action, 0, sizeof(draw_action_t));
    action->action = DRAW_ACTION_CLEAR;
}

/**
 * Tạo hành động LINE (vẽ đường)
 */
void drawing_create_line_action(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint32_t color, uint8_t width, draw_action_t *action)
{
    if (!action)
    {
        return;
    }

    action->action = DRAW_ACTION_LINE;
    action->x1 = x1;
    action->y1 = y1;
    action->x2 = x2;
    action->y2 = y2;
    action->color = color;
    action->width = width;
}
