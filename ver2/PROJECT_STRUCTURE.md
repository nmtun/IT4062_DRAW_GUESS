# 📚 Cấu trúc dự án Draw & Guess - Chi tiết từng file

## 📂 Tổng quan cấu trúc

```
draw-guess/
├── src/
│   ├── core/          # Lõi hệ thống
│   ├── network/       # Tầng mạng
│   ├── game/          # Logic game
│   ├── data/          # Quản lý dữ liệu
│   ├── handlers/      # Xử lý messages
│   ├── protocol/      # Protocol layer
│   └── utils/         # Utilities
├── include/           # Public headers
├── config/            # Configuration files
└── client/            # Web client
```

---

## 🔷 src/core/ - Lõi hệ thống

### **main.c**
**Mục đích:** Entry point của chương trình

**Chức năng:**
- Khởi động ứng dụng
- Xử lý tham số dòng lệnh (port, config file)
- Khởi tạo logger, database, game state
- Bắt tín hiệu tắt server (SIGINT, SIGTERM)
- Cleanup khi thoát

**Ví dụ:**
```c
int main(int argc, char* argv[]) {
    // Parse arguments
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    
    // Initialize modules
    logger_init("logs/server.log");
    config_load("config/server.conf");
    db_init("data/draw_guess.db");
    
    // Start server
    server_run(port);
    
    // Cleanup
    cleanup_all();
    return 0;
}
```

---

### **server.c/h**
**Mục đích:** Main server loop và quản lý kết nối

**Chức năng:**
- Tạo socket server
- Vòng lặp `select()` hoặc `epoll()` để xử lý nhiều kết nối
- Routing messages đến các handler tương ứng
- Quản lý danh sách connections
- Graceful shutdown

**API chính:**
```c
int server_init(int port);
void server_run(int port);
void server_accept_connection();
void server_handle_client_data(int client_fd);
void server_broadcast(const char* message, int exclude_fd);
void server_shutdown();
```

**Luồng hoạt động:**
```
1. Initialize socket
2. Loop:
   - select() đợi events
   - Accept new connections
   - Read from active connections
   - Dispatch to handlers
   - Send responses
3. Cleanup on shutdown
```

---

### **config.c/h**
**Mục đích:** Quản lý cấu hình từ file

**Chức năng:**
- Đọc file `config/server.conf`
- Parse các giá trị: port, max_clients, round_duration, db_path
- Cung cấp accessor functions
- Validate config values

**API chính:**
```c
ServerConfig* config_load(const char* path);
int config_get_port(ServerConfig* config);
int config_get_max_clients(ServerConfig* config);
int config_get_round_duration(ServerConfig* config);
const char* config_get_db_path(ServerConfig* config);
void config_free(ServerConfig* config);
```

**Ví dụ sử dụng:**
```c
ServerConfig* config = config_load("config/server.conf");
int port = config_get_port(config);          // 8080
int max_clients = config_get_max_clients(config);  // 50
```

---

### **logger.c/h**
**Mục đích:** Hệ thống logging

**Chức năng:**
- Ghi log ra file `logs/server.log`
- Các cấp độ: DEBUG, INFO, WARN, ERROR
- Format timestamp
- Tự động rotate log khi file quá lớn

**API chính:**
```c
void logger_init(const char* log_file);
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_warn(const char* format, ...);
void log_error(const char* format, ...);
void logger_close();
```

**Format log:**
```
[2025-01-03 10:30:45] [INFO] Server started on port 8080
[2025-01-03 10:30:50] [DEBUG] Player connected: fd=5
[2025-01-03 10:31:00] [ERROR] Database error: connection failed
```

---

## 🔷 src/network/ - Tầng mạng

### **socket.c/h**
**Mục đích:** Low-level socket operations

**Chức năng:**
- Wrapper cho các hàm socket cơ bản
- Xử lý lỗi network
- Non-blocking I/O
- Socket options (SO_REUSEADDR, TCP_NODELAY)

**API chính:**
```c
int socket_create();
int socket_bind(int fd, int port);
int socket_listen(int fd, int backlog);
int socket_accept(int fd);
int socket_send(int fd, const void* data, size_t len);
int socket_recv(int fd, void* buffer, size_t len);
void socket_close(int fd);
int socket_set_nonblocking(int fd);
```

**Tại sao tách ra:**
- Abstraction layer cho socket API
- Dễ mock khi test
- Cross-platform compatibility

---

### **websocket.c/h**
**Mục đích:** WebSocket protocol implementation

**Chức năng:**
- WebSocket handshake (HTTP upgrade)
- Encode/decode frames theo RFC 6455
- Handle ping/pong keepalive
- Handle close frames
- Fragment handling

**API chính:**
```c
int ws_handshake(int client_fd, const char* request);
int ws_encode_frame(const char* data, unsigned char* output);
int ws_decode_frame(const unsigned char* data, int len, char* output);
void ws_send_ping(int client_fd);
void ws_send_pong(int client_fd);
void ws_close(int client_fd, int code);
```

**WebSocket Frame Structure:**
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
```

**Opcodes:**
- 0x1: Text frame
- 0x2: Binary frame
- 0x8: Connection close
- 0x9: Ping
- 0xA: Pong

---

### **connection.c/h**
**Mục đích:** Quản lý kết nối

**Chức năng:**
- Track active connections
- Connection metadata (IP, port, connected_at)
- Timeout handling
- Connection lifecycle

**API chính:**
```c
Connection* connection_create(int fd, const char* ip, int port);
void connection_add(Connection* conn);
void connection_remove(int fd);
Connection* connection_find(int fd);
void connection_update_activity(int fd);
void connection_check_timeouts();
int connection_is_alive(int fd);
```

**Connection struct:**
```c
typedef struct {
    int fd;
    char ip_address[64];
    int port;
    time_t connected_at;
    time_t last_activity;
    int is_websocket;
    int handshake_done;
} Connection;
```

---

## 🔷 src/game/ - Logic game

### **game_state.c/h**
**Mục đích:** Quản lý trạng thái game tổng thể

**Chức năng:**
- Khởi tạo và cập nhật GameState
- State transitions
- Game loop timing
- Coordination giữa các modules

**API chính:**
```c
void game_state_init(GameState* state);
void game_state_update(GameState* state);
void game_state_set(GameState* state, GameStateEnum new_state);
int game_state_can_start(GameState* state);
void game_state_reset(GameState* state);
```

**State Machine:**
```
IDLE → WAITING → ROUND_STARTING → ROUND_ACTIVE → ROUND_ENDING → WAITING
                                        ↓
                                   (timeout)
                                        ↓
                                  ROUND_ENDING
```

---

### **round.c/h**
**Mục đích:** Quản lý từng round game

**Chức năng:**
- Bắt đầu/kết thúc round
- Chọn drawer (rotation)
- Pick word ngẫu nhiên
- Generate hint
- Check timeout
- Calculate scores

**API chính:**
```c
void round_start(GameState* state);
void round_end(GameState* state);
int round_select_drawer(GameState* state);
const char* round_pick_word();
char* round_get_hint(const char* word);
int round_check_timeout(GameState* state);
void round_award_points(GameState* state, int player_id);
```

**Round lifecycle:**
```
1. round_start()
   - Select drawer
   - Pick word
   - Generate hint
   - Notify players
   
2. (Players play)
   
3. round_end()
   - Calculate scores
   - Save statistics
   - Notify results
```

**Hint generation:**
```
"con mèo" → "_ _ _   _ _ _"
"máy tính" → "_ _ _   _ _ _ _"
```

---

### **player.c/h**
**Mục đích:** Quản lý player

**Chức năng:**
- CRUD operations cho players
- State management
- Validation
- Statistics tracking

**API chính:**
```c
Player* player_create(int fd, const char* name);
void player_join(GameState* state, Player* player);
void player_leave(GameState* state, int player_id);
void player_set_state(Player* player, PlayerState state);
void player_add_score(Player* player, int points);
int player_is_drawer(const Player* player);
int player_has_guessed(const Player* player);
void player_reset_guess(Player* player);
```

**Player States:**
```c
DISCONNECTED → CONNECTED → WAITING → PLAYING
                                         ↓
                              DRAWING / GUESSING
```

---

### **scoring.c/h**
**Mục đích:** Hệ thống tính điểm

**Chức năng:**
- Tính điểm dựa trên thời gian đoán
- Bonus cho drawer
- Streak bonuses
- Leaderboard calculation

**API chính:**
```c
int scoring_calculate_guess(int elapsed_seconds);
int scoring_calculate_drawer(int correct_guesses);
int scoring_apply_streak_bonus(int base_points, int streak);
void scoring_update_leaderboard(GameState* state);
```

**Scoring logic:**
```c
// Time-based scoring
0-10s:  15 points
11-20s: 12 points
21-30s: 10 points
31-40s: 8 points
41-50s: 6 points
51-60s: 5 points

// Drawer points
+5 points for each correct guess

// Streak bonus
2 correct in a row: +2 bonus
3 correct in a row: +5 bonus
5 correct in a row: +10 bonus
```

---

## 🔷 src/data/ - Quản lý dữ liệu

### **words.c/h**
**Mục đích:** Quản lý từ điển

**Chức năng:**
- Load words từ file
- Random selection
- Difficulty levels
- Track used words (tránh lặp)

**API chính:**
```c
int words_load(const char* filename);
const char* words_get_random();
const char* words_get_by_difficulty(int difficulty);
void words_mark_used(const char* word);
void words_reset_used();
int words_get_count();
```

**Format file config/words.txt:**
```
easy:con mèo,con chó,cái bàn,cái ghế
medium:máy tính,điện thoại,bức tranh,đồng hồ
hard:kiến trúc,triết học,văn hóa,công nghệ
```

---

### **database.c/h**
**Mục đích:** Database operations (SQLite)

**Chức năng:**
- Initialize database & tables
- Save/load players
- Game history
- Statistics queries
- Leaderboard

**API chính:**
```c
int db_init(const char* db_path);
void db_close();

// Player operations
int db_save_player(const Player* player);
int db_load_player(const char* name, Player* player);
int db_update_player_score(const char* name, int score);

// Statistics
PlayerStats* db_get_player_stats(const char* name);
PlayerStats* db_get_leaderboard(int limit);

// Game history
int db_save_game_session(const GameSession* session);
GameSession* db_get_recent_sessions(int limit);
```

**Database Schema:**
```sql
CREATE TABLE players (
    id INTEGER PRIMARY KEY,
    name TEXT UNIQUE NOT NULL,
    total_score INTEGER DEFAULT 0,
    games_played INTEGER DEFAULT 0,
    words_guessed INTEGER DEFAULT 0,
    words_drawn INTEGER DEFAULT 0,
    created_at TIMESTAMP,
    last_played TIMESTAMP
);

CREATE TABLE game_sessions (
    id INTEGER PRIMARY KEY,
    word TEXT NOT NULL,
    drawer_name TEXT NOT NULL,
    winner_name TEXT,
    round_duration INTEGER,
    created_at TIMESTAMP
);

CREATE TABLE word_history (
    id INTEGER PRIMARY KEY,
    word TEXT NOT NULL,
    drawer_name TEXT NOT NULL,
    used_at TIMESTAMP
);
```

---

### **cache.c/h**
**Mục đích:** In-memory cache (giảm DB queries)

**Chức năng:**
- Cache player data
- Cache leaderboard
- TTL (Time To Live)
- Cache invalidation

**API chính:**
```c
void cache_init();
void cache_set(const char* key, void* value, int ttl_seconds);
void* cache_get(const char* key);
void cache_invalidate(const char* key);
void cache_clear();
void cache_cleanup_expired();
```

**Ví dụ:**
```c
// Cache leaderboard for 60 seconds
PlayerStats* stats = db_get_leaderboard(10);
cache_set("leaderboard", stats, 60);

// Next request within 60s
PlayerStats* cached = cache_get("leaderboard");
if (cached) {
    return cached; // No DB query
}
```

---

## 🔷 src/handlers/ - Message handlers

### **handler_manager.c/h**
**Mục đích:** Router cho messages

**Chức năng:**
- Register handlers
- Dispatch messages
- Error handling
- Middleware support

**API chính:**
```c
void handler_init();
void handler_register(MessageType type, HandlerFunc func);
int handler_dispatch(GameState* state, Message* msg);
void handler_set_error_handler(ErrorHandlerFunc func);
```

**Pattern:**
```c
// Registration
handler_register(MSG_TYPE_JOIN, join_handler);
handler_register(MSG_TYPE_DRAW, draw_handler);
handler_register(MSG_TYPE_GUESS, guess_handler);

// Auto-dispatch
Message* msg = message_parse(raw_data);
handler_dispatch(game_state, msg);
// → calls appropriate handler
```

---

### **join_handler.c/h**
**Mục đích:** Xử lý JOIN message

**Chức năng:**
- Validate player name
- Check game capacity
- Add player to game
- Send player list
- Broadcast join event
- Auto-start game if ready

**Input:**
```json
{
    "type": "join",
    "name": "Alice"
}
```

**Output:**
```json
{
    "type": "players",
    "players": [
        {"name": "Alice", "score": 0, "isDrawer": false},
        {"name": "Bob", "score": 10, "isDrawer": true}
    ]
}
```

**Broadcast:**
```json
{
    "type": "player_joined",
    "name": "Alice"
}
```

---

### **draw_handler.c/h**
**Mục đích:** Xử lý DRAW message

**Chức năng:**
- Validate drawer permission
- Parse draw coordinates
- Broadcast to other players
- (Optional) Save to buffer

**Input:**
```json
{
    "type": "draw",
    "x": 0.523,
    "y": 0.342,
    "color": "#000000",
    "size": 3,
    "drawing": true
}
```

**Logic:**
```c
1. Check if sender is drawer
2. Validate coordinates (0-1 range)
3. Validate color format
4. Broadcast to all except drawer
```

---

### **guess_handler.c/h**
**Mục đích:** Xử lý GUESS message

**Chức năng:**
- Validate guesser (not drawer)
- Compare with answer
- Award points if correct
- Broadcast result
- End round or mark guessed

**Input:**
```json
{
    "type": "guess",
    "guess": "con mèo"
}
```

**Logic:**
```c
1. Check sender is not drawer
2. Normalize guess (trim, lowercase)
3. Compare with answer (case-insensitive)
4. If correct:
   - Award points (time-based)
   - Broadcast success
   - End round or mark player
5. If wrong:
   - Broadcast as chat
```

**Output (correct):**
```json
{
    "type": "correct",
    "name": "Alice",
    "word": "con mèo",
    "score": 15
}
```

---

### **chat_handler.c/h**
**Mục đích:** Xử lý CHAT message

**Chức năng:**
- Validate message length
- Filter profanity (optional)
- Broadcast to all

**Input:**
```json
{
    "type": "chat",
    "message": "Hello everyone!"
}
```

**Output:**
```json
{
    "type": "chat",
    "name": "Alice",
    "message": "Hello everyone!"
}
```

---

## 🔷 src/protocol/ - Protocol layer

### **message.c/h**
**Mục đích:** Message parsing và serialization

**Chức năng:**
- Parse JSON → Message struct
- Create JSON từ struct
- Message validation
- Type detection

**API chính:**
```c
Message* message_parse(const char* json);
char* message_create(MessageType type, const char* payload);
int message_validate(const Message* msg);
MessageType message_get_type(const char* json);
void message_free(Message* msg);
```

---

### **frame.c/h**
**Mục đích:** WebSocket frame encoding/decoding

**Chức năng:**
- Encode text/binary frames
- Decode incoming frames
- Handle masking
- Fragment handling

**API chính:**
```c
int frame_encode(const char* data, unsigned char* output);
int frame_decode(const unsigned char* data, int len, char* output);
int frame_is_control(unsigned char opcode);
int frame_is_complete(const unsigned char* data, int len);
```

---

### **validation.c/h**
**Mục đích:** Input validation

**Chức năng:**
- Validate names
- Validate messages
- Validate coordinates
- Sanitize inputs

**API chính:**
```c
int validate_name(const char* name);
int validate_message(const char* message);
int validate_color(const char* color);
int validate_coordinates(float x, float y);
char* sanitize_input(const char* input);
```

**Validation rules:**
```c
// Name: 2-20 chars, alphanumeric + spaces
// Message: 1-256 chars
// Color: #RRGGBB format
// Coordinates: 0.0 - 1.0
```

---

## 🔷 src/utils/ - Utilities

### **string_utils.c/h**
```c
char* str_trim(const char* str);
char* str_lower(const char* str);
int str_compare_ignore_case(const char* s1, const char* s2);
char** str_split(const char* str, char delim, int* count);
```

### **json_utils.c/h**
```c
void* json_parse(const char* str);
char* json_get_string(void* obj, const char* key);
int json_get_int(void* obj, const char* key);
char* json_create_object();
void json_add_string(void* obj, const char* key, const char* value);
```

### **time_utils.c/h**
```c
time_t time_now();
char* time_format(time_t t);
int time_diff(time_t start, time_t end);
int time_expired(time_t start, int timeout);
```

### **hash_utils.c/h**
```c
char* hash_generate_id();
char* hash_sha256(const char* str);
char* hash_random_string(int length);
```

---

## 🔷 config/ - Configuration

### **server.conf**
```ini
[server]
port = 8080
max_clients = 50
bind_address = 0.0.0.0

[game]
round_duration = 60
min_players = 2
points_correct = 10
points_drawer = 5
hint_reveal_time = 15

[database]
enabled = true
path = data/draw_guess.db
connection_timeout = 5000

[logging]
enabled = true
level = INFO
file = logs/server.log
max_size = 10485760  # 10MB
```

### **words.txt**
```
# Easy words (3-6 characters)
easy:con mèo,con chó,cái bàn,cái ghế,quả táo

# Medium words (7-12 characters)
medium:máy tính,điện thoại,bức tranh,đồng hồ,cầu vồng

# Hard words (13+ characters)
hard:kiến trúc,triết học,văn hóa,công nghệ,vũ trụ
```

---

## 🎯 Message Flow Example

### Ví dụ: Player đoán đúng

```
1. Client gửi:
   {"type":"guess","guess":"con mèo"}
   
2. network/websocket.c
   ws_decode_frame() → raw JSON
   
3. protocol/message.c
   message_parse() → Message struct
   
4. handlers/handler_manager.c
   handler_dispatch() → guess_handler
   
5. handlers/guess_handler.c
   - Validate not drawer
   - Compare with answer
   - Call game/scoring.c
   
6. game/scoring.c
   scoring_calculate_guess() → 15 points
   
7. game/player.c
   player_add_score() → update score
   
8. data/database.c
   db_update_player_score() → save to DB
   
9. protocol/message.c
   message_create() → JSON response
   
10. network/websocket.c
    ws_encode_frame() + broadcast
    
11. All clients receive:
    {"type":"correct","name":"Alice","score":15}
```

---

## 📊 Module Dependencies

```
main.c
  ├─→ core/config.c
  ├─→ core/logger.c
  ├─→ core/server.c
  │     ├─→ network/socket.c
  │     ├─→ network/websocket.c
  │     ├─→ network/connection.c
  │     ├─→ handlers/handler_manager.c
  │     │     ├─→ handlers/join_handler.c
  │     │     ├─→ handlers/draw_handler.c
  │     │     ├─→ handlers/guess_handler.c
  │     │     └─→ handlers/chat_handler.c
  │     │           ├─→ game/game_state.c
  │     │           ├─→ game/round.c
  │     │           ├─→ game/player.c
  │     │           └─→ game/scoring.c
  │     │                 ├─→ data/words.c
  │     │                 ├─→ data/database.c
  │     │                 └─→ data/cache.c
  │     └─→ protocol/message.c
  │           ├─→ protocol/frame.c
  │           └─→ protocol/validation.c
  └─→ utils/* (used by all modules)
```

---

## ✅ Best Practices

### 1. **Separation of Concerns**
Mỗi file có một nhiệm vụ rõ ràng, không làm nhiều việc

### 2. **Dependency Injection**
Functions nhận dependencies qua parameters, không dùng global variables

### 3. **Error Handling**
Mọi function đều return error codes, check và log errors

### 4. **Memory Management**
Rõ ràng về ownership: ai allocate thì ai free

### 5. **Testing**
Mỗi module có thể test độc lập

---

## 🚀 Next Steps

1. Implement từng module theo thứ tự dependencies
2. Write unit tests cho mỗi module
3. Integration testing
4. Performance optimization
5. Documentation

---

Cấu trúc này giúp:
- ✅ **Maintainable** - Dễ maintain và debug
- ✅ **Scalable** - Dễ thêm features mới
- ✅ **Testable** - Test được từng phần
- ✅ **Reusable** - Dùng lại modules cho project khác
- ✅ **Professional** - Chuẩn industry practices