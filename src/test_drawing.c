#include "include/drawing.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * Test 1: Tạo hành động LINE (vẽ đường)
 * Mục đích: Kiểm tra việc tạo hành động vẽ đường với các tham số cụ thể
 */
void test_create_line_action()
{
    printf("Test 1: Create LINE action... ");
    draw_action_t action;
    
    // Tạo LINE: từ điểm (100,200) đến (300,400), màu đỏ (RGBA), độ rộng 5 pixels
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);

    // Kiểm tra tất cả các trường được gán đúng giá trị
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
 * Test 2: Tạo hành động CLEAR (xóa canvas)
 * Mục đích: Kiểm tra việc tạo hành động xóa toàn bộ canvas
 */
void test_create_clear_action()
{
    printf("Test 2: Create CLEAR action... ");
    draw_action_t action;
    
    // Tạo CLEAR action - xóa toàn bộ canvas
    drawing_create_clear_action(&action);

    // Kiểm tra loại action được set đúng
    assert(action.action == DRAW_ACTION_CLEAR);
    printf("PASSED\n");
}

/**
 * Test 3: Kiểm tra validation (xác thực dữ liệu)
 * Mục đích: Test các trường hợp hợp lệ và không hợp lệ của drawing action
 */
void test_validate_action()
{
    printf("Test 3: Validate action... ");
    draw_action_t action;

    // Case 1: Action hợp lệ - tọa độ và width trong giới hạn
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);
    assert(drawing_validate_action(&action) == true);

    // Case 2: Không hợp lệ - tọa độ x1 vượt quá kích thước canvas (5000 > MAX_CANVAS_WIDTH)
    drawing_create_line_action(5000, 200, 300, 400, 0xFF0000FF, 5, &action);
    assert(drawing_validate_action(&action) == false);

    // Case 3: Không hợp lệ - độ rộng bút vẽ vượt giới hạn (50 > MAX_BRUSH_WIDTH=20)
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 50, &action);
    assert(drawing_validate_action(&action) == false);

    // Case 4: CLEAR action luôn hợp lệ (tọa độ và width không quan trọng)
    drawing_create_clear_action(&action);
    assert(drawing_validate_action(&action) == true);

    printf("PASSED\n");
}

/**
 * Test 4: Serialize và Parse (chuyển đổi hai chiều)
 * Mục đích: Kiểm tra việc chuyển đổi qua lại giữa struct và binary format
 */
void test_serialize_and_parse()
{
    printf("Test 4: Serialize and parse... ");
    draw_action_t original, parsed;
    uint8_t buffer[14];

    // Bước 1: Tạo hành động LINE mẫu
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &original);

    // Bước 2: Serialize (struct -> binary) - chuyển struct thành 14 bytes
    int bytes_written = drawing_serialize_action(&original, buffer);
    assert(bytes_written == 14); // Phải đúng 14 bytes

    // Bước 3: Parse (binary -> struct) - chuyển 14 bytes thành struct
    int result = drawing_parse_action(buffer, 14, &parsed);
    assert(result == 0); // Parse thành công (return 0)

    // Bước 4: Verify tất cả các trường khớp với giá trị original
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
 * Test 5: Parse payload không hợp lệ
 * Mục đích: Kiểm tra xử lý lỗi khi buffer quá ngắn hoặc dữ liệu không hợp lệ
 */
void test_parse_invalid_payload()
{
    printf("Test 5: Parse payload không hợp lệ... ");
    draw_action_t action;
    uint8_t buffer[10]; // Buffer chỉ 10 bytes (cần tối thiểu 14 bytes)

    // Parse phải fail với buffer quá ngắn
    int result = drawing_parse_action(buffer, 10, &action);
    assert(result == -1); // Trả về -1 (lỗi)

    printf("PASSED\n");
}

/**
 * Test 6: Giá trị biên (boundary values)
 * Mục đích: Kiểm tra các giá trị max của canvas size và brush width
 */
void test_boundary_values()
{
    printf("Test 6: Giá trị biên... ");
    draw_action_t action;
    uint8_t buffer[14];

    // Tạo action với giá trị max: MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT, MAX_BRUSH_WIDTH
    drawing_create_line_action(MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT,
                               MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT,
                               0xFFFFFFFF, MAX_BRUSH_WIDTH, &action);
    
    // Action với giá trị max phải vẫn hợp lệ
    assert(drawing_validate_action(&action) == true);

    // Serialize và parse lại để verify không bị mất dữ liệu
    drawing_serialize_action(&action, buffer);
    draw_action_t parsed;
    drawing_parse_action(buffer, 14, &parsed);

    // Verify các giá trị max được giữ nguyên sau serialize/parse
    assert(parsed.x1 == MAX_CANVAS_WIDTH);
    assert(parsed.y1 == MAX_CANVAS_HEIGHT);
    assert(parsed.width == MAX_BRUSH_WIDTH);

    printf("PASSED\n");
}

/**
 * Hàm helper: In buffer dưới dạng hex để kiểm tra trực quan
 * @param buffer Mảng bytes cần in
 * @param len Độ dài của buffer
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
 * Test 7: Kiểm tra trực quan binary format
 * Mục đích: In ra buffer hex để verify format có đúng specification không
 */
void test_visual_inspection()
{
    printf("\nTest 7: Kiểm tra trực quan dữ liệu serialized\n");
    draw_action_t action;
    uint8_t buffer[14];

    // Test case 1: LINE action với tham số cụ thể
    // Vẽ từ (100,200) đến (300,400), màu đỏ (0xFF0000FF), độ rộng 5
    drawing_create_line_action(100, 200, 300, 400, 0xFF0000FF, 5, &action);
    drawing_serialize_action(&action, buffer);

    printf("Action: LINE (100,200) -> (300,400), color=0xFF0000FF, width=5\n");
    print_buffer_hex(buffer, 14);
    printf("Kỳ vọng: 01 0064 00C8 012C 0190 FF0000FF 05\n");
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
    printf("Kỳ vọng: 02 (các byte còn lại = 0)\n");
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

    printf("\n=== Tất cả tests PASSED! ===\n");
    return 0;
}
