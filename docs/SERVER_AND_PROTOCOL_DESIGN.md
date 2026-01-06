# Tài Liệu Thiết Kế và Hoạt Động Server & Protocol

## Mục Lục
1. [Tổng Quan Kiến Trúc](#tổng-quan-kiến-trúc)
2. [Cấu Trúc Dữ Liệu Server](#cấu-trúc-dữ-liệu-server)
3. [Khởi Tạo và Quản Lý Kết Nối](#khởi-tạo-và-quản-lý-kết-nối)
4. [Event Loop với select()](#event-loop-với-select)
5. [Protocol Design](#protocol-design)
6. [Message Parsing và Serialization](#message-parsing-và-serialization)
7. [Protocol Handlers và Routing](#protocol-handlers-và-routing)
8. [Broadcast Mechanism](#broadcast-mechanism)
9. [Connection Management](#connection-management)
10. [Error Handling và Recovery](#error-handling-và-recovery)

---

## Tổng Quan Kiến Trúc

### Kiến Trúc Tổng Thể

```
┌─────────────┐
│   Client    │ (WebSocket)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Gateway   │ (Node.js - WebSocket ↔ TCP Bridge)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ TCP Server  │ (C - Binary Protocol)
└──────┬──────┘
       │
       ├──► Protocol Handlers
       │    ├── protocol_auth.c
       │    ├── protocol_room.c
       │    ├── protocol_game.c
       │    ├── protocol_chat.c
       │    └── protocol_drawing.c
       │
       ├──► Game Logic
       │    ├── game.c
       │    └── room.c
       │
       └──► Database
            └── database.c
```

### Luồng Xử Lý Message

```
1. Client gửi message (WebSocket JSON)
   ↓
2. Gateway chuyển đổi JSON → Binary Protocol
   ↓
3. TCP Server nhận binary data
   ↓
4. protocol_parse_message() - Parse binary → message_t
   ↓
5. protocol_handle_message() - Router đến handler phù hợp
   ↓
6. Protocol Handler xử lý logic
   ↓
7. protocol_send_message() - Serialize → Binary
   ↓
8. Gateway chuyển đổi Binary → JSON
   ↓
9. Client nhận response (WebSocket JSON)
```

---

## Cấu Trúc Dữ Liệu Server

### 1. Client State Machine

**File: `include/server.h`**

```c
typedef enum {
    CLIENT_STATE_LOGGED_OUT = 0,  // Chưa đăng nhập
    CLIENT_STATE_LOGGED_IN = 1,   // Đã đăng nhập, chưa vào phòng
    CLIENT_STATE_IN_ROOM = 2,     // Đang trong phòng, chưa chơi
    CLIENT_STATE_IN_GAME = 3      // Đang chơi game
} client_state_t;
```

**State Transitions:**
```
LOGGED_OUT → LOGIN → LOGGED_IN
LOGGED_IN → JOIN_ROOM → IN_ROOM
IN_ROOM → START_GAME → IN_GAME
IN_GAME → GAME_END → IN_ROOM
IN_ROOM/IN_GAME → LEAVE_ROOM → LOGGED_IN
```

### 2. Client Structure

**File: `include/server.h`**

```c
typedef struct {
    int fd;                        // Socket file descriptor
    int active;                    // 1 = active, 0 = inactive
    int user_id;                   // -1 nếu chưa đăng nhập
    char username[32];             // Username (null-terminated)
    char avatar[32];                // Avatar filename
    client_state_t state;          // Trạng thái hiện tại
} client_t;
```

**Quản lý:**
- Mảng cố định: `client_t clients[MAX_CLIENTS]` (MAX_CLIENTS = 100)
- Index-based lookup: `server->clients[client_index]`
- Active flag để tái sử dụng slot

### 3. Server Structure

**File: `include/server.h`**

```c
typedef struct server {
    int socket_fd;                 // Server socket file descriptor
    int port;                      // Port lắng nghe
    struct sockaddr_in address;    // Server address
    client_t clients[MAX_CLIENTS]; // Mảng clients
    int client_count;              // Số clients hiện tại
    room_t* rooms[MAX_ROOMS];      // Mảng con trỏ đến rooms
    int room_count;                // Số rooms hiện tại
    fd_set read_fds;              // File descriptor set cho select()
    int max_fd;                    // Max file descriptor (cho select())
} server_t;
```

**Thiết kế:**
- **Static arrays:** Không cần dynamic allocation
- **Pointer array cho rooms:** Dễ dàng thêm/xóa rooms
- **fd_set:** Dùng cho `select()` để monitor multiple sockets

---

## Khởi Tạo và Quản Lý Kết Nối

### 1. Server Initialization

**File: `server.c` - `server_init()`**

```c
int server_init(server_t *server, int port)
```

**Các bước:**

1. **Khởi tạo cấu trúc:**
   ```c
   memset(server, 0, sizeof(server_t));
   server->port = port;
   server->client_count = 0;
   server->max_fd = 0;
   ```

2. **Tạo socket:**
   ```c
   server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   ```
   - `AF_INET`: IPv4
   - `SOCK_STREAM`: TCP

3. **Set SO_REUSEADDR:**
   ```c
   int opt = 1;
   setsockopt(server->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
   ```
   - Cho phép tái sử dụng địa chỉ ngay sau khi server dừng
   - Tránh lỗi "Address already in use"

4. **Cấu hình địa chỉ:**
   ```c
   server->address.sin_family = AF_INET;
   server->address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost
   server->address.sin_port = htons(port);
   ```

5. **Bind socket:**
   ```c
   bind(server->socket_fd, (struct sockaddr *)&server->address, sizeof(server->address));
   ```

### 2. Listen và Accept

**File: `server.c` - `server_listen()`**

```c
int server_listen(server_t *server)
{
    // Set hàng đợi kết nối tối đa là 5
    if (listen(server->socket_fd, 5) < 0) {
        perror("listen() failed");
        return -1;
    }
    return 0;
}
```

**File: `server.c` - `server_accept_client()`**

```c
int server_accept_client(server_t *server)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(server->socket_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept() failed");
        return -1;
    }
    
    return server_add_client(server, client_fd);
}
```

### 3. Thêm Client vào Danh Sách

**File: `server.c` - `server_add_client()`**

**Các bước:**

1. **Thiết lập TCP Keepalive:**
   ```c
   int keepalive = 1;
   setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
   ```
   - Giữ kết nối sống khi không có traffic
   - Quan trọng cho VPS/proxy/load balancer

2. **TCP Keepalive Parameters (Linux):**
   ```c
   #ifdef TCP_KEEPIDLE
   setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
   // 60 giây idle trước khi bắt đầu probes
   #endif
   
   #ifdef TCP_KEEPINTVL
   setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
   // 10 giây giữa các probes
   #endif
   
   #ifdef TCP_KEEPCNT
   setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
   // 3 probes trước khi đóng
   #endif
   ```

3. **Tìm slot trống:**
   ```c
   for (int i = 0; i < MAX_CLIENTS; i++) {
       if (!server->clients[i].active) {
           server->clients[i].fd = client_fd;
           server->clients[i].active = 1;
           server->clients[i].user_id = -1;
           server->clients[i].state = CLIENT_STATE_LOGGED_OUT;
           server->client_count++;
           
           // Cập nhật max_fd cho select()
           if (client_fd > server->max_fd) {
               server->max_fd = client_fd;
           }
           
           return i;  // Trả về client_index
       }
   }
   ```

4. **Nếu đầy:**
   ```c
   close(client_fd);  // Đóng kết nối
   return -1;
   ```

### 4. Xóa Client

**File: `server.c` - `server_remove_client()`**

```c
void server_remove_client(server_t *server, int client_index)
{
    int fd = server->clients[client_index].fd;
    
    // Shutdown write để đảm bảo dữ liệu được gửi trước khi đóng
    shutdown(fd, SHUT_WR);
    
    // Đóng socket
    close(fd);
    
    // Reset client state
    server->clients[client_index].active = 0;
    server->clients[client_index].fd = -1;
    server->clients[client_index].user_id = -1;
    server->clients[client_index].state = CLIENT_STATE_LOGGED_OUT;
    server->client_count--;
}
```

**Lưu ý:**
- `shutdown(fd, SHUT_WR)` đảm bảo dữ liệu được flush trước khi đóng
- Không xóa khỏi mảng, chỉ đánh dấu `active = 0` để tái sử dụng slot

---

## Event Loop với select()

### 1. Event Loop Architecture

**File: `server.c` - `server_event_loop()`**

```c
void server_event_loop(server_t *server)
{
    while (1) {
        // 1. Khởi tạo fd_set
        FD_ZERO(&server->read_fds);
        FD_SET(server->socket_fd, &server->read_fds);
        server->max_fd = server->socket_fd;
        
        // 2. Thêm tất cả client sockets
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].fd, &server->read_fds);
                if (server->clients[i].fd > server->max_fd) {
                    server->max_fd = server->clients[i].fd;
                }
            }
        }
        
        // 3. select() với timeout 1 giây
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int activity = select(server->max_fd + 1, &server->read_fds, NULL, NULL, &tv);
        
        // 4. Xử lý events
        if (FD_ISSET(server->socket_fd, &server->read_fds)) {
            server_accept_client(server);  // Kết nối mới
        }
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (server->clients[i].active && FD_ISSET(server->clients[i].fd, &server->read_fds)) {
                server_handle_client_data(server, i);  // Dữ liệu từ client
            }
        }
        
        // 5. Game tick (mỗi giây)
        // Kiểm tra timeout, broadcast timer update, ping database
    }
}
```

### 2. select() Mechanism

**Cách hoạt động:**

1. **FD_SET:** Thêm file descriptors vào set
   ```c
   FD_SET(fd, &read_fds);  // Thêm fd vào set
   ```

2. **select():** Chờ events trên các fds
   ```c
   select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
   ```
   - `max_fd + 1`: Số lượng fds cần kiểm tra
   - `&read_fds`: Set các fds cần đọc
   - `timeout`: Thời gian chờ tối đa (1 giây)

3. **FD_ISSET:** Kiểm tra fd có sẵn sàng không
   ```c
   if (FD_ISSET(fd, &read_fds)) {
       // fd sẵn sàng đọc
   }
   ```

**Ưu điểm:**
- **Non-blocking với timeout:** Không block vô hạn
- **Single-threaded:** Đơn giản, không cần synchronization
- **Efficient:** Chỉ check các fds cần thiết

**Nhược điểm:**
- **Limited scalability:** `select()` có giới hạn `FD_SETSIZE` (thường 1024)
- **O(n) complexity:** Phải check tất cả fds sau mỗi `select()`

### 3. Game Tick trong Event Loop

**File: `server.c` - `server_event_loop()`**

```c
// Tick: kiểm tra timeout cho tất cả phòng đang chơi
time_t now = time(NULL);
for (int r = 0; r < MAX_ROOMS; r++) {
    room_t* room = server->rooms[r];
    if (!room || room->state != ROOM_PLAYING || !room->game) continue;

    // Giữ lại word trước khi game_end_round reset state
    char word_before[64];
    strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);

    if (word_before[0] == '\0') continue;

    if (game_check_timeout(room->game)) {
        // Broadcast round_end + next round/game end
        protocol_handle_round_timeout(server, room, word_before);
    }
}

// Gửi timer update mỗi 1 giây
if (now - last_timer_update >= 1) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        room_t* room = server->rooms[r];
        if (room && room->state == ROOM_PLAYING && room->game) {
            protocol_broadcast_timer_update(server, room);
        }
    }
    last_timer_update = now;
}

// Ping database mỗi 5 phút
if (now - last_db_ping > 300) {
    if (db && db->conn) {
        mysql_ping(db->conn);
        last_db_ping = now;
    }
}
```

**Các tác vụ định kỳ:**
- **Game timeout check:** Mỗi giây
- **Timer update broadcast:** Mỗi giây
- **Database ping:** Mỗi 5 phút (giữ connection sống)

---

## Protocol Design

### 1. Message Format

**File: `common/protocol.h`**

```
[TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]
```

**Binary Layout:**
```
Byte 0:     Message Type (uint8_t)
Byte 1-2:   Payload Length (uint16_t, network byte order - big-endian)
Byte 3+:    Payload Data (variable length)
```

**Ví dụ:**
```
LOGIN_REQUEST (0x01) với username="user", password="pass"
[0x01][0x0040][username(32) + password(32) + avatar(32)]
```

### 2. Message Types

**File: `common/protocol.h`**

**Authentication (0x01 - 0x0F):**
- `MSG_LOGIN_REQUEST` (0x01)
- `MSG_LOGIN_RESPONSE` (0x02)
- `MSG_REGISTER_REQUEST` (0x03)
- `MSG_REGISTER_RESPONSE` (0x04)
- `MSG_LOGOUT` (0x05)
- `MSG_CHANGE_PASSWORD_REQUEST` (0x06)
- `MSG_CHANGE_PASSWORD_RESPONSE` (0x07)

**Room Management (0x10 - 0x1F):**
- `MSG_ROOM_LIST_REQUEST` (0x10)
- `MSG_ROOM_LIST_RESPONSE` (0x11)
- `MSG_CREATE_ROOM` (0x12)
- `MSG_JOIN_ROOM` (0x13)
- `MSG_LEAVE_ROOM` (0x14)
- `MSG_ROOM_UPDATE` (0x15)
- `MSG_START_GAME` (0x16)
- `MSG_ROOM_PLAYERS_UPDATE` (0x17)

**Game Play (0x20 - 0x2F):**
- `MSG_GAME_START` (0x20)
- `MSG_DRAW_DATA` (0x22)
- `MSG_DRAW_BROADCAST` (0x23)
- `MSG_GUESS_WORD` (0x24)
- `MSG_CORRECT_GUESS` (0x25)
- `MSG_ROUND_END` (0x27)
- `MSG_GAME_END` (0x28)
- `MSG_TIMER_UPDATE` (0x2A)

**Chat (0x30 - 0x3F):**
- `MSG_CHAT_MESSAGE` (0x30)
- `MSG_CHAT_BROADCAST` (0x31)

**System (0x50 - 0x5F):**
- `MSG_SERVER_SHUTDOWN` (0x50)
- `MSG_ACCOUNT_LOGGED_IN_ELSEWHERE` (0x51)

### 3. Payload Structures

**File: `common/protocol.h`**

**LOGIN_REQUEST:**
```c
typedef struct {
    char username[MAX_USERNAME_LEN];      // 32 bytes
    char password[MAX_PASSWORD_LEN];      // 32 bytes
    char avatar[32];                      // 32 bytes
} login_request_t;  // Total: 96 bytes
```

**LOGIN_RESPONSE:**
```c
#pragma pack(1)
typedef struct {
    uint8_t status;         // STATUS_SUCCESS hoặc STATUS_ERROR
    int32_t user_id;        // -1 nếu thất bại
    char username[MAX_USERNAME_LEN];  // 32 bytes
} login_response_t;  // Total: 37 bytes
#pragma pack()
```

**GAME_START:**
```c
// Payload: drawer_id(4) + word_length(1) + time_limit(2) + round_start_ms(8) 
//          + current_round(4) + player_count(1) + total_rounds(1) 
//          + word(64) + category(64) = 149 bytes
```

**Network Byte Order:**
- Tất cả integers (int32_t, uint16_t, uint64_t) được convert sang network byte order (big-endian)
- Sử dụng `htonl()`, `htons()`, `ntohl()`, `ntohs()`

---

## Message Parsing và Serialization

### 1. Parse Message (Binary → message_t)

**File: `protocol_core.c` - `protocol_parse_message()`**

```c
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out)
{
    if (!buffer || !msg_out || buffer_len < 3) {
        return -1;
    }

    // Đọc type (1 byte)
    msg_out->type = buffer[0];

    // Đọc length (2 bytes, network byte order)
    uint16_t length_network;
    memcpy(&length_network, buffer + 1, 2);
    msg_out->length = ntohs(length_network);  // Convert từ network byte order

    // Kiểm tra độ dài hợp lệ
    if (msg_out->length > buffer_len - 3) {
        fprintf(stderr, "Lỗi: Payload length (%d) vượt quá buffer còn lại (%zu)\n", 
                msg_out->length, buffer_len - 3);
        return -1;
    }

    // Cấp phát bộ nhớ cho payload
    if (msg_out->length > 0) {
        msg_out->payload = (uint8_t*)malloc(msg_out->length);
        if (!msg_out->payload) {
            fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho payload\n");
            return -1;
        }
        memcpy(msg_out->payload, buffer + 3, msg_out->length);
    } else {
        msg_out->payload = NULL;
    }

    return 0;
}
```

**Các bước:**
1. Kiểm tra buffer đủ 3 bytes (type + length)
2. Đọc type (byte 0)
3. Đọc length (byte 1-2), convert từ network byte order
4. Validate length
5. Cấp phát và copy payload
6. **Lưu ý:** Caller phải `free(msg_out->payload)` sau khi dùng

### 2. Create Message (message_t → Binary)

**File: `protocol_core.c` - `protocol_create_message()`**

```c
int protocol_create_message(uint8_t type, const uint8_t* payload, uint16_t payload_len, uint8_t* buffer_out)
{
    if (!buffer_out) {
        return -1;
    }

    // Type (1 byte)
    buffer_out[0] = type;

    // Length (2 bytes, network byte order)
    uint16_t length_network = htons(payload_len);  // Convert sang network byte order
    memcpy(buffer_out + 1, &length_network, 2);

    // Payload
    if (payload && payload_len > 0) {
        memcpy(buffer_out + 3, payload, payload_len);
    }

    return 3 + payload_len;  // Tổng độ dài message
}
```

**Các bước:**
1. Ghi type (byte 0)
2. Convert length sang network byte order và ghi (byte 1-2)
3. Copy payload (byte 3+)
4. Trả về tổng độ dài

### 3. Send Message

**File: `protocol_core.c` - `protocol_send_message()`**

```c
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len)
{
    uint8_t buffer[BUFFER_SIZE];
    
    if (3 + payload_len > BUFFER_SIZE) {
        fprintf(stderr, "Lỗi: Message quá lớn (%d bytes)\n", 3 + payload_len);
        return -1;
    }

    // Tạo message
    int msg_len = protocol_create_message(type, payload, payload_len, buffer);
    if (msg_len < 0) {
        return -1;
    }

    // Gửi qua socket
    ssize_t sent = send(client_fd, buffer, msg_len, 0);
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }

    // Kiểm tra đã gửi đủ
    if (sent != msg_len) {
        fprintf(stderr, "Cảnh báo: Chỉ gửi được %zd/%d bytes\n", sent, msg_len);
        return -1;
    }

    return 0;
}
```

**Các bước:**
1. Validate payload size
2. Tạo message binary
3. Gửi qua socket
4. Verify đã gửi đủ bytes

### 4. Handle Client Data

**File: `server.c` - `server_handle_client_data()`**

```c
void server_handle_client_data(server_t *server, int client_index) {
    uint8_t buffer[BUFFER_SIZE];
    int client_fd = server->clients[client_index].fd;
    
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        // Lỗi hoặc kết nối đóng
        server_handle_disconnect(server, client_index);
        return;
    }
    
    // Parse message
    message_t msg;
    if (protocol_parse_message(buffer, bytes_read, &msg) == 0) {
        // Xử lý message
        protocol_handle_message(server, client_index, &msg);
        
        // Giải phóng payload
        if (msg.payload) {
            free(msg.payload);
        }
    } else {
        fprintf(stderr, "Lỗi parse message từ client %d\n", client_index);
    }
}
```

**Lưu ý:**
- `recv()` có thể nhận partial message (TCP stream)
- Cần buffer để accumulate data (Gateway xử lý việc này)
- Phải `free()` payload sau khi dùng

---

## Protocol Handlers và Routing

### 1. Message Router

**File: `protocol.c` - `protocol_handle_message()`**

```c
int protocol_handle_message(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg) {
        return -1;
    }

    switch (msg->type) {
        case MSG_LOGIN_REQUEST:
            return protocol_handle_login(server, client_index, msg);
            
        case MSG_REGISTER_REQUEST:
            return protocol_handle_register(server, client_index, msg);
            
        case MSG_LOGOUT:
            return protocol_handle_logout(server, client_index, msg);

        case MSG_CHANGE_PASSWORD_REQUEST:
            return protocol_handle_change_password(server, client_index, msg);

        case MSG_ROOM_LIST_REQUEST:
            return protocol_handle_room_list_request(server, client_index, msg);

        case MSG_CREATE_ROOM:
            return protocol_handle_create_room(server, client_index, msg);

        case MSG_JOIN_ROOM:
            return protocol_handle_join_room(server, client_index, msg);

        case MSG_LEAVE_ROOM:
            return protocol_handle_leave_room(server, client_index, msg);

        case MSG_DRAW_DATA:
            return protocol_handle_draw_data(server, client_index, msg);

        case MSG_START_GAME:
            return protocol_handle_start_game(server, client_index, msg);

        case MSG_GUESS_WORD:
            return protocol_handle_guess_word(server, client_index, msg);

        case MSG_CHAT_MESSAGE:
            return protocol_handle_chat_message(server, client_index, msg);

        case MSG_GET_GAME_HISTORY:
            return protocol_handle_get_game_history(server, client_index, msg);
            
        default:
            fprintf(stderr, "Unknown message type: 0x%02X từ client %d\n", 
                    msg->type, client_index);
            return -1;
    }
}
```

**Thiết kế:**
- **Centralized routing:** Tất cả messages đi qua một router
- **Modular handlers:** Mỗi handler trong file riêng
- **Forward declarations:** Handlers được declare ở đầu `protocol.c`

### 2. Handler Modules

**Cấu trúc:**
```
protocol.c              - Router và core functions
protocol_core.c         - Parse/serialize/send
protocol_auth.c         - Login, register, logout, change password
protocol_room.c         - Room management
protocol_game.c         - Game logic
protocol_chat.c         - Chat messages
protocol_drawing.c      - Drawing data
protocol_history.c      - Game history
```

**Ví dụ Handler:**

**File: `protocol_auth.c` - `protocol_handle_login()`**

```c
int protocol_handle_login(server_t* server, int client_index, const message_t* msg)
{
    client_t* client = &server->clients[client_index];
    
    // Parse payload
    login_request_t* req = (login_request_t*)msg->payload;
    
    // Validate
    if (msg->length < sizeof(login_request_t)) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "Invalid payload");
        return -1;
    }
    
    // Authenticate
    int user_id = auth_authenticate(db, req->username, req->password);
    
    if (user_id > 0) {
        // Success
        client->user_id = user_id;
        strncpy(client->username, req->username, sizeof(client->username) - 1);
        strncpy(client->avatar, req->avatar, sizeof(client->avatar) - 1);
        client->state = CLIENT_STATE_LOGGED_IN;
        
        protocol_send_login_response(client->fd, STATUS_SUCCESS, user_id, req->username);
    } else {
        // Failed
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "Authentication failed");
    }
    
    return 0;
}
```

**Pattern chung:**
1. Validate client state
2. Parse payload
3. Validate payload size
4. Xử lý logic
5. Update client state
6. Send response

### 3. Payload Parsing

**Ví dụ: Parse LOGIN_REQUEST**

```c
login_request_t* req = (login_request_t*)msg->payload;

// Validate
if (msg->length < sizeof(login_request_t)) {
    return -1;
}

// Extract fields
char username[32];
strncpy(username, req->username, sizeof(username) - 1);
username[sizeof(username) - 1] = '\0';  // Đảm bảo null-terminated
```

**Lưu ý:**
- Payload là binary data, cần cast về struct
- Luôn validate size trước khi cast
- Đảm bảo null-terminated cho strings

---

## Broadcast Mechanism

### 1. Broadcast to Room

**File: `server.c` - `server_broadcast_to_room()`**

```c
int server_broadcast_to_room(server_t* server, int room_id, uint8_t msg_type, 
                             const uint8_t* payload, uint16_t payload_len, 
                             int exclude_user_id)
{
    // Tìm room
    room_t* room = NULL;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (server->rooms[i] != NULL && server->rooms[i]->room_id == room_id) {
            room = server->rooms[i];
            break;
        }
    }

    if (!room) {
        return -1;
    }

    // Gửi message đến tất cả clients trong phòng
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];
        
        // Bỏ qua client không active
        if (!client->active) {
            continue;
        }

        // Bỏ qua client bị loại trừ
        if (exclude_user_id > 0 && client->user_id == exclude_user_id) {
            continue;
        }

        // Kiểm tra client có trong phòng không
        if (room_has_player(room, client->user_id)) {
            if (protocol_send_message(client->fd, msg_type, payload, payload_len) == 0) {
                sent_count++;
            }
        }
    }

    return sent_count;
}
```

**Cách hoạt động:**
1. Tìm room theo `room_id`
2. Duyệt tất cả clients
3. Kiểm tra client active và trong phòng
4. Gửi message đến mỗi client
5. Trả về số lượng đã gửi

**Use cases:**
- `GAME_START`: Broadcast đến tất cả trong phòng
- `CORRECT_GUESS`: Broadcast đến tất cả (trừ người đoán nếu cần)
- `ROUND_END`: Broadcast đến tất cả
- `DRAW_BROADCAST`: Broadcast đến tất cả (trừ người vẽ)

### 2. Broadcast Room List

**File: `protocol_room.c` - `protocol_broadcast_room_list()`**

```c
int protocol_broadcast_room_list(server_t* server)
{
    // Lấy danh sách phòng
    room_info_t room_list[50];
    int room_count = room_get_list(server, room_list, 50);

    // Tạo payload
    uint8_t payload[BUFFER_SIZE];
    // ... serialize room_list ...

    // Gửi đến tất cả clients đã đăng nhập
    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];

        if (client->active &&
            client->state >= CLIENT_STATE_LOGGED_IN &&
            client->user_id > 0) {
            if (protocol_send_message(client->fd, MSG_ROOM_LIST_RESPONSE,
                                      payload, payload_size) == 0) {
                sent_count++;
            }
        }
    }

    return sent_count;
}
```

**Khi nào broadcast:**
- Khi có phòng mới được tạo
- Khi có phòng bị xóa
- Khi phòng thay đổi state (WAITING → PLAYING → FINISHED)

### 3. Broadcast Room Players Update

**File: `protocol_room.c` - `protocol_broadcast_room_players_update()`**

```c
int protocol_broadcast_room_players_update(server_t* server, room_t* room,
                                           uint8_t action, int changed_user_id,
                                           const char* changed_username,
                                           int exclude_client_index)
{
    // Tạo payload với danh sách đầy đủ người chơi
    room_players_update_t* update = (room_players_update_t*)payload;
    
    update->room_id = htonl((uint32_t)room->room_id);
    update->action = action;  // 0 = JOIN, 1 = LEAVE
    update->changed_user_id = htonl((uint32_t)changed_user_id);
    update->player_count = htons((uint16_t)room->player_count);
    
    // Serialize danh sách players
    player_info_protocol_t* players = (player_info_protocol_t*)(payload + sizeof(room_players_update_t));
    for (int i = 0; i < room->player_count; i++) {
        players[i].user_id = htonl((uint32_t)room->players[i]);
        players[i].is_owner = (room->players[i] == room->owner_id) ? 1 : 0;
        players[i].is_active = room->active_players[i] == 1 ? 1 : 
                              (room->active_players[i] == -1 ? 255 : 0);
        // ... username, avatar ...
    }

    // Broadcast đến tất cả clients trong phòng
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];
        if (!client->active || i == exclude_client_index) {
            continue;
        }
        if (room_has_player(room, client->user_id)) {
            protocol_send_message(client->fd, MSG_ROOM_PLAYERS_UPDATE,
                                  payload, payload_size);
        }
    }
}
```

**Khi nào broadcast:**
- Khi có người join room
- Khi có người leave room
- Khi owner thay đổi
- Khi player state thay đổi (active/inactive)

---

## Connection Management

### 1. Disconnect Handling

**File: `server.c` - `server_handle_disconnect()`**

```c
void server_handle_disconnect(server_t *server, int client_index)
{
    client_t* client = &server->clients[client_index];
    
    // Nếu client đang trong phòng
    if (client->user_id > 0 && 
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {
        
        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            // Lưu thông tin trước khi xóa
            int was_playing = (room->state == ROOM_PLAYING && room->game != NULL);
            int was_drawer = (was_playing && room->game->drawer_id == client->user_id);
            char word_before[64];
            if (was_playing) {
                strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
            }
            
            // Xóa player khỏi phòng
            room_remove_player(room, client->user_id);
            
            // Kiểm tra số người active
            int active_count = 0;
            for (int i = 0; i < room->player_count; i++) {
                if (room->active_players[i] == 1) {
                    active_count++;
                }
            }
            
            // Nếu đang chơi và active < 2 → end game
            if (was_playing && room->game && active_count < 2) {
                room->game->game_ended = true;
                protocol_broadcast_game_end(server, room);
                room_end_game(room);
            }
            
            // Nếu drawer rời → end round và chuyển round tiếp theo
            if (was_drawer && room->game && word_before[0] != '\0') {
                game_end_round(room->game, false, -1);
                protocol_handle_round_timeout(server, room, word_before);
            }
            
            // Broadcast ROOM_PLAYERS_UPDATE
            protocol_broadcast_room_players_update(server, room, 1, 
                                                  client->user_id, client->username, -1);
        }
    }
    
    // Xóa client khỏi danh sách
    server_remove_client(server, client_index);
}
```

**Các bước:**
1. Kiểm tra client có trong phòng không
2. Lưu thông tin game trước khi xóa
3. Xóa player khỏi phòng
4. Xử lý các trường hợp đặc biệt:
   - Active < 2 → end game
   - Drawer rời → end round
5. Broadcast updates
6. Xóa client

### 2. Find Room by User

**File: `server.c` - `server_find_room_by_user()`**

```c
room_t* server_find_room_by_user(server_t* server, int user_id)
{
    if (!server || user_id <= 0) {
        return NULL;
    }

    // Duyệt qua tất cả rooms
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (server->rooms[i] != NULL) {
            if (room_has_player(server->rooms[i], user_id)) {
                return server->rooms[i];
            }
        }
    }

    return NULL;
}
```

**Complexity:** O(MAX_ROOMS * MAX_PLAYERS_PER_ROOM)

### 3. Server Shutdown

**File: `server.c` - `server_broadcast_shutdown()`**

```c
int server_broadcast_shutdown(server_t* server)
{
    const uint8_t* payload = NULL;
    uint16_t payload_len = 0;

    int sent_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_t* client = &server->clients[i];
        
        if (client->active) {
            if (protocol_send_message(client->fd, MSG_SERVER_SHUTDOWN, payload, payload_len) == 0) {
                sent_count++;
            }
        }
    }

    return sent_count;
}
```

**File: `server.c` - `server_cleanup()`**

```c
void server_cleanup(server_t *server) {
    // Đóng tất cả client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server->clients[i].active) {
            close(server->clients[i].fd);
        }
    }
    
    // Đóng server socket
    if (server->socket_fd >= 0) {
        close(server->socket_fd);
    }
    
    printf("Server đã đóng\n");
}
```

---

## Error Handling và Recovery

### 1. Parse Errors

**File: `protocol_core.c`**

```c
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out)
{
    // Validate input
    if (!buffer || !msg_out || buffer_len < 3) {
        return -1;
    }

    // Validate length
    if (msg_out->length > buffer_len - 3) {
        fprintf(stderr, "Lỗi: Payload length (%d) vượt quá buffer còn lại (%zu)\n", 
                msg_out->length, buffer_len - 3);
        return -1;
    }

    // Handle malloc failure
    if (msg_out->length > 0) {
        msg_out->payload = (uint8_t*)malloc(msg_out->length);
        if (!msg_out->payload) {
            fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho payload\n");
            return -1;
        }
    }
}
```

### 2. Send Errors

**File: `protocol_core.c`**

```c
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len)
{
    // Validate size
    if (3 + payload_len > BUFFER_SIZE) {
        fprintf(stderr, "Lỗi: Message quá lớn (%d bytes)\n", 3 + payload_len);
        return -1;
    }

    // Handle send failure
    ssize_t sent = send(client_fd, buffer, msg_len, 0);
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }

    // Handle partial send
    if (sent != msg_len) {
        fprintf(stderr, "Cảnh báo: Chỉ gửi được %zd/%d bytes\n", sent, msg_len);
        return -1;
    }
}
```

**Lưu ý:**
- Partial send có thể xảy ra với TCP
- Cần retry hoặc buffer để gửi phần còn lại
- Hiện tại chỉ log warning

### 3. Connection Errors

**File: `server.c` - `server_handle_client_data()`**

```c
void server_handle_client_data(server_t *server, int client_index) {
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        // Lỗi hoặc kết nối đóng
        server_handle_disconnect(server, client_index);
        return;
    }
    
    // Parse errors được handle trong protocol_parse_message
    if (protocol_parse_message(buffer, bytes_read, &msg) != 0) {
        fprintf(stderr, "Lỗi parse message từ client %d\n", client_index);
        // Không disconnect, chỉ log error
    }
}
```

**Recovery:**
- Parse errors: Log và tiếp tục (không disconnect)
- Recv errors: Disconnect và cleanup
- Send errors: Log và tiếp tục (client sẽ timeout)

### 4. Database Errors

**Pattern:**
```c
if (db && room->db_room_id > 0) {
    // Best-effort: Nếu DB fail, game vẫn chạy
    int result = db_save_guess(db, ...);
    if (result < 0) {
        printf("Warning: Failed to save guess to database\n");
        // Không ảnh hưởng game logic
    }
}
```

**Thiết kế:**
- Database operations là "best-effort"
- Game logic không phụ thuộc vào database
- Errors chỉ log, không crash server

---

## Tóm Tắt Thiết Kế

### Ưu Điểm

1. **Đơn giản:**
   - Single-threaded với `select()`
   - Không cần synchronization
   - Dễ debug và maintain

2. **Hiệu quả:**
   - Binary protocol (nhỏ gọn)
   - Static arrays (không có allocation overhead)
   - Efficient broadcast (O(n) với n = số clients)

3. **Modular:**
   - Protocol handlers tách biệt
   - Dễ thêm message types mới
   - Clear separation of concerns

### Hạn Chế

1. **Scalability:**
   - `select()` giới hạn bởi `FD_SETSIZE` (1024)
   - Single-threaded → không tận dụng multi-core
   - O(n) complexity cho mỗi operation

2. **Error Recovery:**
   - Partial send không được retry
   - Database errors chỉ log
   - Không có reconnection logic

3. **Memory Management:**
   - Payload được malloc/free mỗi message
   - Có thể optimize với pool allocation

### Cải Tiến Có Thể

1. **Epoll/kqueue:** Thay `select()` để scale tốt hơn
2. **Thread pool:** Xử lý I/O và game logic song song
3. **Message buffer pool:** Giảm malloc/free overhead
4. **Partial send retry:** Đảm bảo gửi đủ bytes
5. **Connection pooling:** Tái sử dụng connections

---

## Kết Luận

Tài liệu này mô tả chi tiết thiết kế và hoạt động của server.c và protocol system. Server sử dụng kiến trúc single-threaded với `select()` để quản lý multiple connections, binary protocol để giao tiếp hiệu quả, và modular handlers để xử lý các loại messages khác nhau. Thiết kế đơn giản, dễ maintain, và phù hợp cho ứng dụng real-time game với số lượng người chơi vừa phải.

