#include "../include/drawing.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Test 1: Tao hanh dong LINE (ve duong)
 * Muc dich: Kiem tra viec tao hanh dong ve duong voi cac tham so cu the
 */
void test_create_line_action()
{
    printf("Test 1: Create LINE action... ");
    draw_action_t action;
    
    // Tao LINE: tu diem (100,200) den (300,400), mau do (RGBA), do rong 5 pixels
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);

    // Kiem tra tat ca cac truong duoc gan dung gia tri
    assert(action.action == DRAW_ACTION_LINE);
    assert(action.x1 == 100);
    assert(action.y1 == 200);
    assert(action.x2 == 300);
    assert(action.y2 == 400);
    assert(action.color == 0xFF0000FF);
    assert(action.width == 5);
    printf("PASSED\n");
}

/**
 * Test 2: Tao hanh dong CLEAR (xoa canvas)
 * Muc dich: Kiem tra viec tao hanh dong xoa toan bo canvas
 */
void test_create_clear_action()
{
    printf("Test 2: Create CLEAR action... ");
    draw_action_t action;
    
    // Tao CLEAR action - xoa toan bo canvas
    drawing_create_clear_action(&action);

    // Kiem tra loai action duoc set dung
    assert(action.action == DRAW_ACTION_CLEAR);
    printf("PASSED\n");
}

/**
 * Test 3: Kiem tra validation (xac thuc du lieu)
 * Muc dich: Test cac truong hop hop le va khong hop le cua drawing action
 */
void test_validate_action()
{
    printf("Test 3: Validate action... ");
    draw_action_t action;

    // Case 1: Action hop le - toa do va width trong gioi han
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);
    assert(drawing_validate_action(&action) == true);

    // Case 2: Khong hop le - toa do x1 vuot qua kich thuoc canvas (5000 > MAX_CANVAS_WIDTH)
    drawing_create_line_action(5000, 200, 300, 400, 0xFF0000FF, 5, &action);
    assert(drawing_validate_action(&action) == false);

    // Case 3: Khong hop le - do rong but ve vuot gioi han (50 > MAX_BRUSH_WIDTH=20)
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 50, &action);
    assert(drawing_validate_action(&action) == false);

    // Case 4: CLEAR action luon hop le (toa do va width khong quan trong)
    drawing_create_clear_action(&action);
    assert(drawing_validate_action(&action) == true);

    printf("PASSED\n");
}

/**
 * Test 4: Serialize va Parse (chuyen doi hai chieu)
 * Muc dich: Kiem tra viec chuyen doi qua lai giua struct va binary format
 */
void test_serialize_and_parse()
{
    printf("Test 4: Serialize and parse... ");
    draw_action_t original, parsed;
    uint8_t buffer[14];

    // Buoc 1: Tao hanh dong LINE mau
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &original);

    // Buoc 2: Serialize (struct -> binary) - chuyen struct thanh 14 bytes
    int bytes_written = drawing_serialize_action(&original, buffer);
    assert(bytes_written == 14); // Phai dung 14 bytes

    // Buoc 3: Parse (binary -> struct) - chuyen 14 bytes thanh struct
    int result = drawing_parse_action(buffer, 14, &parsed);
    assert(result == 0); // Parse thanh cong (return 0)

    // Buoc 4: Verify tat ca cac truong khop voi gia tri original
    assert(parsed.action == original.action);
    assert(parsed.x1 == original.x1);
    assert(parsed.y1 == original.y1);
    assert(parsed.x2 == original.x2);
    assert(parsed.y2 == original.y2);
    assert(parsed.color == original.color);
    assert(parsed.width == original.width);

    printf("PASSED\n");
}

/**
 * Test 5: Parse payload khong hop le
 * Muc dich: Kiem tra xu ly loi khi buffer qua ngan hoac du lieu khong hop le
 */
void test_parse_invalid_payload()
{
    printf("Test 5: Parse payload khong hop le... ");
    draw_action_t action;
    uint8_t buffer[10]; // Buffer chi 10 bytes (can toi thieu 14 bytes)

    // Parse phai fail voi buffer qua ngan
    int result = drawing_parse_action(buffer, 10, &action);
    assert(result == -1); // Tra ve -1 (loi)

    printf("PASSED\n");
}

/**
 * Test 6: Gia tri bien (boundary values)
 * Muc dich: Kiem tra cac gia tri max cua canvas size va brush width
 */
void test_boundary_values()
{
    printf("Test 6: Gia tri bien... ");
    draw_action_t action;
    uint8_t buffer[14];

    // Tao action voi gia tri max: MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT, MAX_BRUSH_WIDTH
    drawing_create_line_action(MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT,
                               MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT,
                               0xFFFFFFFF, MAX_BRUSH_WIDTH, &action);
    
    // Action voi gia tri max phai van hop le
    assert(drawing_validate_action(&action) == true);

    // Serialize va parse lai de verify khong bi mat du lieu
    drawing_serialize_action(&action, buffer);
    draw_action_t parsed;
    drawing_parse_action(buffer, 14, &parsed);

    // Verify cac gia tri max duoc giu nguyen sau serialize/parse
    assert(parsed.x1 == MAX_CANVAS_WIDTH);
    assert(parsed.y1 == MAX_CANVAS_HEIGHT);
    assert(parsed.width == MAX_BRUSH_WIDTH);

    printf("PASSED\n");
}

/**
 * Ham helper: In buffer duoi dang hex de kiem tra truc quan
 * @param buffer Mang bytes can in
 * @param len Do dai cua buffer
 */
void print_buffer_hex(const uint8_t *buffer, size_t len)
{
    printf("Buffer (hex): ");
    for (size_t i = 0; i < len; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}

/**
 * Test 7: Kiem tra truc quan binary format
 * Muc dich: In ra buffer hex de verify format co dung specification khong
 */
void test_visual_inspection()
{
    printf("\nTest 7: Kiem tra truc quan du lieu serialized\n");
    draw_action_t action;
    uint8_t buffer[14];

    // Test case 1: LINE action voi tham so cu the
    // Ve tu (100,200) den (300,400), mau do (0xFF0000FF), do rong 5
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);
    drawing_serialize_action(&action, buffer);

    printf("Action: LINE (100,200) -> (300,400), color=0xFF0000FF, width=5\n");
    print_buffer_hex(buffer, 14);
    printf("Ky vong: 01 0064 00C8 012C 0190 FF0000FF 05\n");
    printf("         │  │    │    │    │    │        └─ width=5\n");
    printf("         │  │    │    │    │    └────────── color=0xFF0000FF\n");
    printf("         │  │    │    │    └─────────────── y2=400 (0x0190)\n");
    printf("         │  │    │    └──────────────────── x2=300 (0x012C)\n");
    printf("         │  │    └───────────────────────── y1=200 (0x00C8)\n");
    printf("         │  └────────────────────────────── x1=100 (0x0064)\n");
    printf("         └───────────────────────────────── action=LINE(1)\n");

    // Test case 2: CLEAR action
    drawing_create_clear_action(&action);
    drawing_serialize_action(&action, buffer);

    printf("\nAction: CLEAR\n");
    print_buffer_hex(buffer, 14);
    printf("Ky vong: 02 (cac byte con lai = 0)\n");
}

int main()
{
    printf("=== Drawing Module Tests ===\n\n");

    test_create_line_action();
    test_create_clear_action();
    test_validate_action();
    test_serialize_and_parse();
    test_parse_invalid_payload();
    test_boundary_values();
    test_visual_inspection();

    printf("\n=== Tat ca tests PASSED! ===\n");
    return 0;
}
