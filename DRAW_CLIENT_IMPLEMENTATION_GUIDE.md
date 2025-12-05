# Hướng Dẫn Triển Khai Chức Năng Draw - Client Side

Tài liệu này hướng dẫn team Frontend triển khai chức năng vẽ (drawing) và đồng bộ hóa giữa các client thông qua WebSocket.

## Mục Lục

1. [Tổng Quan Protocol](#tổng-quan-protocol)
2. [Cấu Trúc Message](#cấu-trúc-message)
3. [Drawing Action Types](#drawing-action-types)
4. [Triển Khai Client](#triển-khai-client)
5. [Xử Lý Room State](#xử-lý-room-state)
6. [Ví Dụ Code](#ví-dụ-code)

---

## Tổng Quan Protocol

### Message Format

Tất cả messages tuân theo format:
```
[TYPE: 1 byte][LENGTH: 2 bytes][PAYLOAD: variable]
```

- **TYPE**: Loại message (uint8)
- **LENGTH**: Độ dài payload (uint16, **big-endian/network byte order**)
- **PAYLOAD**: Dữ liệu thực tế

### Message Types Liên Quan

```javascript
const MSG_DRAW_DATA = 0x22;        // Client gửi dữ liệu vẽ đến server
const MSG_DRAW_BROADCAST = 0x23;   // Server broadcast dữ liệu vẽ đến các client khác
```

---

## Cấu Trúc Message

### MSG_DRAW_DATA / MSG_DRAW_BROADCAST

**Message Header:**
- Byte 0: `0x22` (DRAW_DATA) hoặc `0x23` (DRAW_BROADCAST)
- Byte 1-2: `0x000E` (14 bytes payload, big-endian)

**Payload Format (14 bytes):**
```
[action: 1 byte][x1: 2 bytes][y1: 2 bytes][x2: 2 bytes][y2: 2 bytes][color: 4 bytes][width: 1 byte]
```

**Chi tiết:**
- `action` (uint8): Loại hành động vẽ (0=MOVE, 1=LINE, 2=CLEAR, 3=ERASE)
- `x1, y1` (uint16, big-endian): Tọa độ điểm bắt đầu
- `x2, y2` (uint16, big-endian): Tọa độ điểm kết thúc
- `color` (uint32, big-endian): Màu RGBA (R: bits 31-24, G: bits 23-16, B: bits 15-8, A: bits 7-0)
- `width` (uint8): Độ rộng bút vẽ (1-20)

---

## Drawing Action Types

### DRAW_ACTION_MOVE (0)
Di chuyển bút (không vẽ). Hiện tại không được sử dụng.

### DRAW_ACTION_LINE (1)
Vẽ một đường thẳng từ (x1, y1) đến (x2, y2).

**Ví dụ:**
```javascript
const action = 1; // DRAW_ACTION_LINE
const x1 = 100, y1 = 100;
const x2 = 200, y2 = 200;
const color = 0xFF0000FF; // Màu đỏ (RGBA)
const width = 5;
```

### DRAW_ACTION_CLEAR (2)
Xóa toàn bộ canvas.

**Lưu ý:** Với CLEAR, các giá trị x1, y1, x2, y2, color, width không quan trọng (có thể set = 0).

### DRAW_ACTION_ERASE (3)
Xóa từng phần (eraser). Client sẽ dùng `globalCompositeOperation = 'destination-out'` để xóa.

**Lưu ý:** Với ERASE, `color` không quan trọng (có thể set = 0).

---

## Triển Khai Client

### 1. Khởi Tạo Canvas

```javascript
let canvas, ctx;
let isDrawing = false;
let lastX = 0, lastY = 0;
let currentColor = '#000000';
let brushSize = 5;
let isEraserMode = false;

// Khởi tạo
canvas = document.getElementById('drawCanvas');
ctx = canvas.getContext('2d');
ctx.lineCap = 'round';
ctx.lineJoin = 'round';
```

### 2. Xử Lý Mouse Events

```javascript
function startDrawing(e) {
    isDrawing = true;
    const rect = canvas.getBoundingClientRect();
    lastX = e.clientX - rect.left;
    lastY = e.clientY - rect.top;
}

function draw(e) {
    if (!isDrawing) return;

    const rect = canvas.getBoundingClientRect();
    const currentX = e.clientX - rect.left;
    const currentY = e.clientY - rect.top;

    // Vẽ trên canvas local
    if (isEraserMode) {
        ctx.globalCompositeOperation = 'destination-out';
    } else {
        ctx.globalCompositeOperation = 'source-over';
        ctx.strokeStyle = currentColor;
    }
    
    ctx.lineWidth = brushSize;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    ctx.lineTo(currentX, currentY);
    ctx.stroke();

    // Gửi draw data đến server
    sendDrawData(lastX, lastY, currentX, currentY);

    lastX = currentX;
    lastY = currentY;
}

function stopDrawing() {
    isDrawing = false;
}

// Đăng ký event listeners
canvas.addEventListener('mousedown', startDrawing);
canvas.addEventListener('mousemove', draw);
canvas.addEventListener('mouseup', stopDrawing);
canvas.addEventListener('mouseout', stopDrawing);
```

### 3. Gửi DRAW_DATA đến Server

```javascript
function sendDrawData(x1, y1, x2, y2) {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    const action = isEraserMode ? 3 : 1; // ERASE hoặc LINE
    const color = isEraserMode ? 0 : hexToRGBA(currentColor);

    // Tạo payload (14 bytes)
    const buffer = new ArrayBuffer(14);
    const view = new DataView(buffer);
    
    view.setUint8(0, action);
    view.setUint16(1, Math.round(x1), false); // Big-endian
    view.setUint16(3, Math.round(y1), false);
    view.setUint16(5, Math.round(x2), false);
    view.setUint16(7, Math.round(y2), false);
    view.setUint32(9, color, false); // Big-endian
    view.setUint8(13, brushSize);

    // Tạo message: [TYPE:1][LENGTH:2][PAYLOAD:14]
    const messageBuffer = new ArrayBuffer(17);
    const messageView = new DataView(messageBuffer);
    
    messageView.setUint8(0, 0x22); // MSG_DRAW_DATA
    messageView.setUint16(1, 14, false); // Payload length (big-endian)
    
    const payload = new Uint8Array(buffer);
    const message = new Uint8Array(messageBuffer);
    message.set(payload, 3);

    ws.send(message.buffer);
}

// Helper: Chuyển đổi hex color sang RGBA integer
function hexToRGBA(hex) {
    const r = parseInt(hex.slice(1, 3), 16);
    const g = parseInt(hex.slice(3, 5), 16);
    const b = parseInt(hex.slice(5, 7), 16);
    const a = 255;
    return (r << 24) | (g << 16) | (b << 8) | a;
}
```

### 4. Gửi CLEAR Action

```javascript
function sendClearAction() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    const action = 2; // DRAW_ACTION_CLEAR
    
    const buffer = new ArrayBuffer(14);
    const view = new DataView(buffer);
    view.setUint8(0, action);
    // Các bytes còn lại = 0

    const messageBuffer = new ArrayBuffer(17);
    const messageView = new DataView(messageBuffer);
    
    messageView.setUint8(0, 0x22); // MSG_DRAW_DATA
    messageView.setUint16(1, 14, false);
    
    const payload = new Uint8Array(buffer);
    const message = new Uint8Array(messageBuffer);
    message.set(payload, 3);

    ws.send(message.buffer);
}

function clearCanvas() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    sendClearAction();
}
```

### 5. Nhận và Xử Lý DRAW_BROADCAST

```javascript
function handleDrawBroadcast(payload) {
    if (payload.length < 14) {
        console.error('DRAW_BROADCAST payload quá ngắn');
        return;
    }

    const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    const action = view.getUint8(0);
    
    if (action === 2) { // CLEAR
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        return;
    }

    if (action === 1) { // LINE
        const x1 = view.getUint16(1, false); // Big-endian
        const y1 = view.getUint16(3, false);
        const x2 = view.getUint16(5, false);
        const y2 = view.getUint16(7, false);
        const colorInt = view.getUint32(9, false);
        const width = view.getUint8(13);

        // Chuyển đổi color từ integer sang CSS color
        const r = (colorInt >>> 24) & 0xFF;
        const g = (colorInt >>> 16) & 0xFF;
        const b = (colorInt >>> 8) & 0xFF;
        const color = `rgb(${r},${g},${b})`;

        // Vẽ trên canvas
        ctx.globalCompositeOperation = 'source-over';
        ctx.strokeStyle = color;
        ctx.lineWidth = width;
        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.stroke();
    }

    if (action === 3) { // ERASE
        const x1 = view.getUint16(1, false);
        const y1 = view.getUint16(3, false);
        const x2 = view.getUint16(5, false);
        const y2 = view.getUint16(7, false);
        const width = view.getUint8(13);

        // Xóa bằng destination-out
        ctx.globalCompositeOperation = 'destination-out';
        ctx.lineWidth = width;
        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.stroke();
    }
}

// Trong WebSocket onmessage handler
ws.onmessage = function(event) {
    // ... xử lý các message types khác ...
    
    if (type === 0x23) { // MSG_DRAW_BROADCAST
        handleDrawBroadcast(payload);
    }
};
```

### 6. Chức Năng Bút Xóa (Eraser)

```javascript
function toggleEraser() {
    isEraserMode = !isEraserMode;
    const eraserBtn = document.getElementById('eraserBtn');
    
    if (isEraserMode) {
        eraserBtn.textContent = 'Bút vẽ';
        eraserBtn.style.background = '#4caf50';
    } else {
        eraserBtn.textContent = 'Bút xóa';
        eraserBtn.style.background = '';
    }
}
```

---

## Xử Lý Room State

### Vấn Đề: Logic Tự Động Start Game

Hiện tại, server có logic tự động chuyển phòng sang trạng thái `ROOM_PLAYING` khi đạt `max_players`. Điều này có thể không phù hợp với flow của game thực tế.

### Cách Xóa Logic Tự Động Start Game

**File:** `src/server/protocol_room.c`

**Vị trí:** Trong hàm `protocol_handle_join_room()`, sau khi thêm player thành công (khoảng dòng 559-581).

**Code cần xóa:**

```c
// Kiểm tra nếu phòng đã đạt max players và đang WAITING, tự động start game
if (room->player_count >= room->max_players && room->state == ROOM_WAITING) {
    printf("Phòng '%s' (ID: %d) đã đạt max players (%d/%d), tự động bắt đầu game\n",
           room->room_name, room->room_id, room->player_count, room->max_players);
    
    if (room_start_game(room)) {
        printf("Game đã tự động bắt đầu trong phòng '%s' (ID: %d)\n",
               room->room_name, room->room_id);
        
        // Cập nhật trạng thái client sang IN_GAME
        client->state = CLIENT_STATE_IN_GAME;
        
        // Cập nhật trạng thái tất cả clients trong phòng
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active && 
                room_has_player(room, server->clients[i].user_id)) {
                server->clients[i].state = CLIENT_STATE_IN_GAME;
            }
        }
    } else {
        fprintf(stderr, "Lỗi: Không thể tự động bắt đầu game trong phòng %d\n", room->room_id);
    }
}
```

**Sau khi xóa:** Game chỉ bắt đầu khi client gửi `MSG_START_GAME` (0x16) một cách thủ công.

### Client-Side: Gửi START_GAME

```javascript
function sendStartGame() {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;
    
    // START_GAME không có payload
    const messageBuffer = new ArrayBuffer(3);
    const messageView = new DataView(messageBuffer);
    messageView.setUint8(0, 0x16); // MSG_START_GAME
    messageView.setUint16(1, 0, false); // Payload length = 0
    
    ws.send(messageBuffer);
}
```

**Lưu ý:** Chỉ room owner mới có quyền gửi `MSG_START_GAME`.

---

## Ví Dụ Code

### Ví Dụ 1: Gửi LINE Action

```javascript
// Vẽ đường từ (100, 100) đến (200, 200) với màu đỏ, độ rộng 5
const x1 = 100, y1 = 100, x2 = 200, y2 = 200;
const color = 0xFF0000FF; // Đỏ (RGBA)
const width = 5;

const buffer = new ArrayBuffer(14);
const view = new DataView(buffer);
view.setUint8(0, 1); // DRAW_ACTION_LINE
view.setUint16(1, x1, false);
view.setUint16(3, y1, false);
view.setUint16(5, x2, false);
view.setUint16(7, y2, false);
view.setUint32(9, color, false);
view.setUint8(13, width);

// Tạo message
const messageBuffer = new ArrayBuffer(17);
const messageView = new DataView(messageBuffer);
messageView.setUint8(0, 0x22); // MSG_DRAW_DATA
messageView.setUint16(1, 14, false);

const payload = new Uint8Array(buffer);
const message = new Uint8Array(messageBuffer);
message.set(payload, 3);

ws.send(message.buffer);
```

### Ví Dụ 2: Nhận và Vẽ LINE

```javascript
// payload là Uint8Array từ MSG_DRAW_BROADCAST
const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
const action = view.getUint8(0);

if (action === 1) { // LINE
    const x1 = view.getUint16(1, false);
    const y1 = view.getUint16(3, false);
    const x2 = view.getUint16(5, false);
    const y2 = view.getUint16(7, false);
    const colorInt = view.getUint32(9, false);
    const width = view.getUint8(13);

    const r = (colorInt >>> 24) & 0xFF;
    const g = (colorInt >>> 16) & 0xFF;
    const b = (colorInt >>> 8) & 0xFF;
    const color = `rgb(${r},${g},${b})`;

    ctx.globalCompositeOperation = 'source-over';
    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.stroke();
}
```

---

## Lưu Ý Quan Trọng

1. **Network Byte Order (Big-Endian):**
   - Tất cả `uint16` và `uint32` phải dùng big-endian khi gửi/nhận
   - JavaScript: `DataView.setUint16(offset, value, false)` và `DataView.getUint16(offset, false)`

2. **Canvas Coordinates:**
   - Tọa độ canvas phải được tính từ `getBoundingClientRect()` để xử lý đúng khi canvas có offset

3. **Global Composite Operation:**
   - Luôn reset về `'source-over'` sau khi dùng `'destination-out'` để tránh ảnh hưởng đến các lần vẽ tiếp theo

4. **WebSocket Binary Data:**
   - Đảm bảo gateway/server gửi binary data, không phải JSON
   - Kiểm tra `event.data instanceof ArrayBuffer` hoặc `event.data instanceof Blob`

5. **Room State:**
   - Chỉ có thể gửi `MSG_DRAW_DATA` khi phòng ở trạng thái `ROOM_PLAYING`
   - Kiểm tra `room.state === 1` (ROOM_PLAYING) trước khi cho phép vẽ

---

## Tài Liệu Tham Khảo

- `src/include/drawing.h` - Định nghĩa drawing action types
- `src/common/protocol.h` - Định nghĩa message types
- `DrawClient/index.html` - Ví dụ implementation đầy đủ

---

**Chúc team Frontend triển khai thành công!** 🎨

