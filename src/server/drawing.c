#include "../include/drawing.h"
#include <string.h>
#include <arpa/inet.h>

/**
 * Parse du lieu ve tu payload nhi phan
 * Format: [action:1][x1:2][y1:2][x2:2][y2:2][color:4][width:1] = 14 bytes
 */
int drawing_parse_action(const uint8_t *payload, size_t payload_len, draw_action_t *action)
{
    if (!payload || !action)
    {
        return -1;
    }

    // Kiem tra do dai payload
    if (payload_len < 14)
    {
        return -1;
    }

    // Parse action type
    action->action = (draw_action_type_t)payload[0];

    // Parse toa do (network byte order)
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
 * Serialize du lieu ve sang dinh dang nhi phan
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

    // Toa do (chuyen sang network byte order)
    uint16_t x1_net = htons(action->x1);
    uint16_t y1_net = htons(action->y1);
    uint16_t x2_net = htons(action->x2);
    uint16_t y2_net = htons(action->y2);

    memcpy(buffer_out + 1, &x1_net, 2);
    memcpy(buffer_out + 3, &y1_net, 2);
    memcpy(buffer_out + 5, &x2_net, 2);
    memcpy(buffer_out + 7, &y2_net, 2);

    // Color (chuyen sang network byte order)
    uint32_t color_net = htonl(action->color);
    memcpy(buffer_out + 9, &color_net, 4);

    // Width
    buffer_out[13] = action->width;

    return 14; // Tong so bytes da ghi
}

/**
 * Kiem tra tinh hop le cua cac tham so hanh dong ve
 */
bool drawing_validate_action(const draw_action_t *action)
{
    if (!action)
    {
        return false;
    }

    // Kiem tra loai hanh dong
    if (action->action != DRAW_ACTION_MOVE &&
        action->action != DRAW_ACTION_LINE &&
        action->action != DRAW_ACTION_CLEAR &&
        action->action != DRAW_ACTION_ERASE)
    {
        return false;
    }

    // Voi hanh dong CLEAR, toa do va do rong khong quan trong
    if (action->action == DRAW_ACTION_CLEAR)
    {
        return true;
    }

    // Voi hanh dong ERASE, can toa do va do rong hop le
    if (action->action == DRAW_ACTION_ERASE)
    {
        // Kiem tra toa do
        if (action->x1 >= MAX_CANVAS_WIDTH || action->y1 >= MAX_CANVAS_HEIGHT ||
            action->x2 >= MAX_CANVAS_WIDTH || action->y2 >= MAX_CANVAS_HEIGHT)
        {
            return false;
        }

        // Kiem tra do rong but xoa
        if (action->width < MIN_BRUSH_WIDTH || action->width > MAX_BRUSH_WIDTH)
        {
            return false;
        }

        return true;
    }

    // Kiem tra toa do
    if (action->x1 >= MAX_CANVAS_WIDTH || action->y1 >= MAX_CANVAS_HEIGHT ||
        action->x2 >=  MAX_CANVAS_WIDTH || action->y2 >= MAX_CANVAS_HEIGHT)
    {
        return false;
    }

    // Kiem tra do rong but ve
    if (action->width < MIN_BRUSH_WIDTH || action->width > MAX_BRUSH_WIDTH)
    {
        return false;
    }

    return true;
}

/**
 * Tao hanh dong CLEAR (xoa canvas)
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
 * Tao hanh dong LINE (ve duong)
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

/**
 * Tao hanh dong ERASE (xoa tung phan)
 * Color se duoc set thanh 0 (khong quan trong vi client se dung destination-out)
 */
void drawing_create_erase_action(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 uint8_t width, draw_action_t *action)
{
    if (!action)
    {
        return;
    }

    action->action = DRAW_ACTION_ERASE;
    action->x1 = x1;
    action->y1 = y1;
    action->x2 = x2;
    action->y2 = y2;
    action->color = 0;  // Khong quan trong, client se dung destination-out
    action->width = width;
}
