# 🚀 Hướng dẫn Implementation - Draw & Guess Game

## 📋 Nguyên tắc Implementation

1. **Bottom-up approach**: Implement dependencies trước
2. **Test từng module**: Viết test ngay khi hoàn thành
3. **Incremental development**: Mỗi phase chạy được độc lập
4. **Logging everywhere**: Log để debug dễ dàng

---

## 🎯 Phase 0: Setup Project (30 phút)

### Bước 1: Tạo cấu trúc thư mục
```bash
mkdir -p draw-guess/{src/{core,network,game,data,handlers,protocol,utils},include,config,logs,data,tests,client}
cd draw-guess
```

### Bước 2: Tạo Makefile
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -I./include -g
LDFLAGS = -lsqlite3 -lm -lpthread -lcjson

SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*/*.c) $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET = $(BIN_DIR)/draw_guess_server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

test:
	$(CC) $(CFLAGS) tests/*.c $(SRC_DIR)/*/*.c -o $(BIN_DIR)/test $(LDFLAGS)
	./$(BIN_DIR)/test

.PHONY: all clean test
```

### Bước 3: Tạo `.gitignore`
```
*.o
*.log
*.db
logs/
build/
bin/
data/*.db
.vscode/
.idea/
```

---

## 🎯 Phase 1: Utils & Foundation (2-3 giờ)

**Mục tiêu**: Xây dựng các utilities cơ bản

### 1.1 `src/utils/string_utils.c/h`

**Header file (`include/string_utils.h`):**
```c
#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

// Basic operations
char* str_trim(const char* str);
char* str_lower(const char* str);
char* str_upper(const char* str);
char* str_duplicate(const char* str);

// Comparison
int str_compare_ignore_case(const char* s1, const char* s2);
int str_starts_with(const char* str, const char* prefix);
int str_ends_with(const char* str, const char* suffix);

// Advanced
char** str_split(const char* str, char delim, int* count);
char* str_join(char** parts, int count, const char* sep);
void str_array_free(char** arr, int count);

// Validation
int str_is_empty(const char* str);
int str_length_between(const char* str, int min, int max);
int str_contains_only(const char* str, const char* charset);

#endif
```

**Thứ tự implement:**
1. `str_trim()` - Trim whitespace (15 phút)
2. `str_lower()` - Convert to lowercase (10 phút)
3. `str_duplicate()` - Safe strdup wrapper (5 phút)
4. `str_compare_ignore_case()` - Case-insensitive compare (10 phút)
5. `str_split()` - Split string by delimiter (20 phút)
6. `str_is_empty()` - Check if empty/null (5 phút)
7. Các hàm còn lại (20 phút)

**Test case:**
```c
// tests/test_string_utils.c
#include <assert.h>
#include <string.h>
#include "string_utils.h"

void test_trim() {
    char* result = str_trim("  hello  ");
    assert(strcmp(result, "hello") == 0);
    free(result);
}

void test_split() {
    int count;
    char** parts = str_split("a,b,c", ',', &count);
    assert(count == 3);
    assert(strcmp(parts[0], "a") == 0);
    str_array_free(parts, count);
}
```

---

### 1.2 `src/utils/time_utils.c/h`

**Header (`include/time_utils.h`):**
```c
#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <time.h>

// Basic time
time_t time_now(void);
char* time_format(time_t t);           // "2025-01-03 10:30:45"
char* time_format_iso(time_t t);       // ISO 8601

// Time calculations
int time_diff_seconds(time_t start, time_t end);
int time_expired(time_t start, int timeout_seconds);
time_t time_add_seconds(time_t t, int seconds);

// Utilities
void time_sleep_ms(int milliseconds);
long time_get_milliseconds(void);

#endif
```

**Thứ tự implement:**
1. `time_now()` - Get current time (5 phút)
2. `time_format()` - Format timestamp (15 phút)
3. `time_diff_seconds()` - Calculate difference (10 phút)
4. `time_expired()` - Check timeout (10 phút)
5. `time_sleep_ms()` - Sleep in milliseconds (10 phút)

---

### 1.3 `src/utils/hash_utils.c/h`

**Header (`include/hash_utils.h`):**
```c
#ifndef HASH_UTILS_H
#define HASH_UTILS_H

// ID generation
char* hash_generate_id(void);              // UUID-like
char* hash_random_string(int length);      // Random alphanumeric

// Hashing (Optional)
char* hash_sha256(const char* str);
char* hash_md5(const char* str);

#endif
```

**Thứ tự implement:**
1. `hash_random_string()` - Random alphanumeric (20 phút)
2. `hash_generate_id()` - UUID-like ID (15 phút)

---

### 1.4 `src/core/logger.c/h`

**Header (`include/logger.h`):**
```c
#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

// Initialization
int logger_init(const char* log_file, LogLevel level);
void logger_close(void);

// Logging functions
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_warn(const char* format, ...);
void log_error(const char* format, ...);

// Configuration
void logger_set_level(LogLevel level);

#endif
```

**Thứ tự implement:**
1. Internal state và helper functions (15 phút)
   - Global FILE* pointer
   - `logger_level_string()`
   - `logger_write()` với va_list
2. `logger_init()` và `logger_close()` (15 phút)
3. 4 logging functions: debug, info, warn, error (20 phút)
4. Thread-safety với mutex (optional, 20 phút)

**Test:**
```c
logger_init("logs/test.log", LOG_DEBUG);
log_info("Server started on port %d", 8080);
log_error("Connection failed: %s", strerror(errno));
logger_close();
```

---

### 1.5 `src/core/config.c/h`

**Header (`include/config.h`):**
```c
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    // Server
    int port;
    int max_clients;
    char bind_address[64];
    
    // Game
    int round_duration;
    int min_players;
    int points_correct;
    int points_drawer;
    
    // Paths
    char db_path[256];
    char words_file[256];
    char log_file[256];
} ServerConfig;

// Loading
ServerConfig* config_load(const char* path);
ServerConfig* config_create_default(void);
void config_free(ServerConfig* config);

// Getters
int config_get_port(const ServerConfig* cfg);
int config_get_max_clients(const ServerConfig* cfg);
const char* config_get_db_path(const ServerConfig* cfg);

#endif
```

**Thứ tự implement:**
1. `config_create_default()` - Default values (10 phút)
2. `config_parse_line()` - Parse key=value (20 phút)
3. `config_load()` - Load from file (30 phút)
4. Getter functions (10 phút)

**Tạo file `config/server.conf`:**
```ini
# Server settings
port=8080
max_clients=50
bind_address=0.0.0.0

# Game settings
round_duration=60
min_players=2
points_correct=10
points_drawer=5

# Paths
db_path=data/draw_guess.db
words_file=config/words.txt
log_file=logs/server.log
```

---

## ✅ Checkpoint Phase 1

**Test đơn giản (`tests/test_phase1.c`):**
```c
#include <stdio.h>
#include "logger.h"
#include "config.h"
#include "string_utils.h"
#include "time_utils.h"

int main() {
    printf("=== Phase 1 Test ===\n");
    
    // Test logger
    logger_init("logs/test.log", LOG_DEBUG);
    log_info("Phase 1 test started");
    
    // Test config
    ServerConfig* cfg = config_load("config/server.conf");
    if (cfg) {
        log_info("Port: %d", config_get_port(cfg));
        config_free(cfg);
    }
    
    // Test string utils
    char* trimmed = str_trim("  hello  ");
    log_info("Trimmed: '%s'", trimmed);
    free(trimmed);
    
    // Test time utils
    char* time_str = time_format(time_now());
    log_info("Current time: %s", time_str);
    free(time_str);
    
    logger_close();
    printf("✓ Phase 1 completed!\n");
    return 0;
}
```

**Compile và chạy:**
```bash
make test
./bin/test
cat logs/test.log
```

---

## 🎯 Phase 2: JSON & Protocol (2-3 giờ)

**Mục tiêu**: Parse và create JSON messages

### 2.1 Cài đặt cJSON

```bash
# Ubuntu/Debian
sudo apt-get install libcjson-dev

# macOS
brew install cjson

# Hoặc compile from source
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
mkdir build && cd build
cmake ..
make
sudo make install
```

---

### 2.2 `src/utils/json_utils.c/h`

**Header (`include/json_utils.h`):**
```c
#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <cjson/cJSON.h>

// Parsing
cJSON* json_parse(const char* str);
void json_free(cJSON* json);

// Getters
const char* json_get_string(cJSON* json, const char* key);
int json_get_int(cJSON* json, const char* key);
double json_get_double(cJSON* json, const char* key);
int json_get_bool(cJSON* json, const char* key);
int json_has_key(cJSON* json, const char* key);

// Object creation
cJSON* json_create_object(void);
void json_add_string(cJSON* json, const char* key, const char* value);
void json_add_int(cJSON* json, const char* key, int value);
void json_add_double(cJSON* json, const char* key, double value);
void json_add_bool(cJSON* json, const char* key, int value);

// Array operations
cJSON* json_create_array(void);
void json_add_item_to_array(cJSON* array, cJSON* item);

// Serialization
char* json_to_string(cJSON* json);
char* json_to_string_unformatted(cJSON* json);

#endif
```

**Thứ tự implement:**
1. Parsing và getters (20 phút)
2. Object creation (20 phút)
3. Array operations (15 phút)
4. Serialization (10 phút)

---

### 2.3 `src/protocol/message.c/h`

**Header (`include/message.h`):**
```c
#ifndef MESSAGE_H
#define MESSAGE_H

#include <cjson/cJSON.h>

typedef enum {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_JOIN,
    MSG_TYPE_LEAVE,
    MSG_TYPE_DRAW,
    MSG_TYPE_GUESS,
    MSG_TYPE_CHAT,
    MSG_TYPE_ROUND_START,
    MSG_TYPE_ROUND_END,
    MSG_TYPE_PLAYERS,
    MSG_TYPE_CORRECT,
    MSG_TYPE_WRONG,
    MSG_TYPE_ERROR,
    MSG_TYPE_PING,
    MSG_TYPE_PONG
} MessageType;

typedef struct {
    MessageType type;
    int sender_fd;
    cJSON* data;
} Message;

// Parsing
Message* message_parse(const char* json_str, int sender_fd);
MessageType message_get_type_from_string(const char* type_str);
int message_validate(const Message* msg);
void message_free(Message* msg);

// Creation - Game messages
char* message_create_join(const char* name);
char* message_create_leave(void);
char* message_create_draw(double x, double y, const char* color, int size, int drawing);
char* message_create_guess(const char* guess);
char* message_create_chat(const char* message);

// Creation - Server messages
char* message_create_players(cJSON* players_array);
char* message_create_round_start(const char* drawer_name, const char* hint, int duration);
char* message_create_round_end(const char* word, cJSON* scores_array);
char* message_create_correct(const char* name, const char* word, int score);
char* message_create_wrong(const char* name, const char* guess);
char* message_create_error(const char* error_msg);

// Utilities
const char* message_type_to_string(MessageType type);
int message_is_broadcast(MessageType type);

#endif
```

**Thứ tự implement:**
1. MessageType enum và conversion (10 phút)
2. `message_parse()` - Parse JSON to Message struct (30 phút)
3. Client message creators (join, draw, guess, chat) (30 phút)
4. Server message creators (players, round_start, correct, etc) (40 phút)
5. Validation và utilities (20 phút)

**Test:**
```c
// Parse incoming message
Message* msg = message_parse("{\"type\":\"join\",\"name\":\"Alice\"}", 5);
assert(msg->type == MSG_TYPE_JOIN);
const char* name = json_get_string(msg->data, "name");
printf("Player: %s\n", name);
message_free(msg);

// Create outgoing message
char* json = message_create_join("Bob");
printf("JSON: %s\n", json);
free(json);
```

---

### 2.4 `src/protocol/validation.c/h`

**Header (`include/validation.h`):**
```c
#ifndef VALIDATION_H
#define VALIDATION_H

// Name validation
int validate_name(const char* name);
int validate_name_length(const char* name);
int validate_name_charset(const char* name);

// Message validation
int validate_chat_message(const char* msg);
int validate_guess(const char* guess);

// Draw data validation
int validate_coordinates(double x, double y);
int validate_color(const char* color);
int validate_brush_size(int size);

// Sanitization
char* sanitize_name(const char* name);
char* sanitize_message(const char* msg);

#endif
```

**Thứ tự implement:**
1. `validate_name()` - Name validation rules (20 phút)
   - Length: 2-20 characters
   - Charset: alphanumeric, spaces, Vietnamese chars
   - No leading/trailing spaces
2. `validate_chat_message()` (10 phút)
3. `validate_coordinates()`, `validate_color()` (15 phút)
4. Sanitization functions (20 phút)

**Validation rules:**
```c
// Name: 2-20 chars, alphanumeric + spaces + Vietnamese
// Message: 1-256 chars, no empty/whitespace only
// Color: #RRGGBB format (regex: ^#[0-9A-Fa-f]{6}$)
// Coordinates: 0.0 <= x,y <= 1.0
// Brush size: 1 <= size <= 20
```

---

## ✅ Checkpoint Phase 2

**Test (`tests/test_phase2.c`):**
```c
#include <stdio.h>
#include "logger.h"
#include "message.h"
#include "validation.h"

int main() {
    logger_init("logs/test.log", LOG_DEBUG);
    
    printf("=== Phase 2 Test ===\n");
    
    // Test message parsing
    const char* json_in = "{\"type\":\"join\",\"name\":\"Alice\"}";
    Message* msg = message_parse(json_in, 5);
    
    if (msg) {
        log_info("Parsed message type: %s", message_type_to_string(msg->type));
        const char* name = json_get_string(msg->data, "name");
        log_info("Player name: %s", name);
        
        // Validate
        if (validate_name(name)) {
            log_info("✓ Name is valid");
        } else {
            log_error("✗ Name is invalid");
        }
        
        message_free(msg);
    }
    
    // Test message creation
    char* json_out = message_create_round_start("Bob", "_ _ _", 60);
    log_info("Created message: %s", json_out);
    free(json_out);
    
    // Test validation
    assert(validate_coordinates(0.5, 0.5) == 1);
    assert(validate_coordinates(1.5, 0.5) == 0);
    assert(validate_color("#FF5733") == 1);
    assert(validate_color("red") == 0);
    
    logger_close();
    printf("✓ Phase 2 completed!\n");
    return 0;
}
```

---

## 🎯 Phase 3: Network Layer (3-4 giờ)

**Mục tiêu**: Socket handling và WebSocket protocol

### 3.1 `src/network/socket.c/h`

**Header (`include/socket.h`):**
```c
#ifndef SOCKET_H
#define SOCKET_H

#include <sys/types.h>

// Socket creation
int socket_create(void);
int socket_bind(int fd, const char* address, int port);
int socket_listen(int fd, int backlog);
int socket_accept(int server_fd, char* client_ip, int* client_port);

// I/O operations
ssize_t socket_send(int fd, const void* data, size_t len);
ssize_t socket_recv(int fd, void* buffer, size_t len);
void socket_close(int fd);

// Socket options
int socket_set_nonblocking(int fd);
int socket_set_reuseaddr(int fd);
int socket_set_nodelay(int fd);
int socket_set_keepalive(int fd);

// Error handling
const char* socket_get_error(void);
int socket_is_would_block(void);

#endif
```

**Thứ tự implement:**
1. `socket_create()` - Create TCP socket (10 phút)
2. `socket_bind()` và `socket_listen()` (15 phút)
3. `socket_accept()` - Accept with client info (15 phút)
4. `socket_send()` và `socket_recv()` (15 phút)
5. Socket options (set_nonblocking, set_reuseaddr, etc) (25 phút)
6. Error handling utilities (15 phút)

---

### 3.2 `src/network/websocket.c/h`

**⚠️ Phần phức tạp nhất của project!**

**Header (`include/websocket.h`):**
```c
#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <stdint.h>

// Handshake
int ws_handshake(int client_fd);
char* ws_generate_accept_key(const char* client_key);

// Frame operations
typedef struct {
    uint8_t fin;
    uint8_t opcode;
    uint8_t masked;
    uint64_t payload_len;
    uint8_t mask[4];
    char* payload;
} WSFrame;

int ws_decode_frame(const uint8_t* data, size_t len, WSFrame* frame);
int ws_encode_frame(const char* data, size_t len, uint8_t opcode, uint8_t** output);
void ws_frame_free(WSFrame* frame);

// Send operations
int ws_send_text(int fd, const char* text);
int ws_send_binary(int fd, const uint8_t* data, size_t len);
int ws_send_ping(int fd);
int ws_send_pong(int fd);
int ws_send_close(int fd, uint16_t code);

// Opcodes
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT         0x1
#define WS_OPCODE_BINARY       0x2
#define WS_OPCODE_CLOSE        0x8
#define WS_OPCODE_PING         0x9
#define WS_OPCODE_PONG         0xA

#endif
```

**Thứ tự implement (Khó nhất!):**

1. **WebSocket Handshake** (45 phút)
   - Read HTTP request
   - Parse `Sec-WebSocket-Key`
   - Generate accept key (SHA-1 + Base64)
   - Send HTTP response

2. **Frame Decoding** (60 phút)
   - Parse frame header
   - Extract payload length (7-bit, 16-bit, 64-bit)
   - Unmask payload
   - Handle fragmentation

3. **Frame Encoding** (45 phút)
   - Build frame header
   - Set FIN, opcode, payload length
   - No masking for server->client

4. **High-level send functions** (30 phút)
   - ws_send_text(), ws_send_ping(), etc

**WebSocket Frame Format:**
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

---

### 3.3 `src/network/connection.c/h`

**Header (`include/connection.h`):**
```c
#ifndef CONNECTION_H
#define CONNECTION_H

#include <time.h>
#include <stdbool.h>

typedef struct Connection {
    int fd;
    char ip_address[64];
    int port;
    time_t connected_at;
    time_t last_activity;
    bool is_websocket;
    bool handshake_done;
    int player_id;  // -1 if not joined
    
    struct Connection* next;
} Connection;

// Connection management
Connection* connection_create(int fd, const char* ip, int port);
void connection_add(Connection* conn);
void connection_remove(int fd);
Connection* connection_find(int fd);
Connection* connection_get_all(int* count);

// Activity tracking
void connection_update_activity(int fd);
void connection_check_timeouts(int timeout_seconds);
bool connection_is_alive(int fd);

// Cleanup
void connection_free_all(void);

#endif
```

**Thứ tự implement:**
1. Connection linked list management (30 phút)
2. CRUD operations (create, add, remove, find) (30 phút)
3. Activity tracking và timeout checking (20 phút)

---

## ✅ Checkpoint Phase 3

**Test simple server (`tests/test_phase3.c`):**
```c
#include <stdio.h>
#include "logger.h"
#include "socket.h"
#include "websocket.h"
#include "connection.h"

int main() {
    logger_init("logs/test.log", LOG_DEBUG);
    
    printf("=== Phase 3 Test ===\n");
    
    // Create server socket
    int server_fd = socket_create();
    socket_set_reuseaddr(server_fd);
    socket_bind(server_fd, "0.0.0.0", 8080);
    socket_listen(server_fd, 10);
    
    log_info("Test server listening on port 8080");
    log_info("Open client/index.html in browser to test");
    log_info("Press Ctrl+C to stop");
    
    // Accept one connection
    char client_ip[64];
    int client_port;
    int client_fd = socket_accept(server_fd, client_ip, &client_port);
    
    if (client_fd > 0) {
        log_info("Client connected: %s:%d", client_ip, client_port);
        
        // WebSocket handshake
        if (ws_handshake(client_fd) == 0) {
            log_info("✓ WebSocket handshake successful");
            
            // Create connection
            Connection* conn = connection_create(client_fd, client_ip, client_port);
            conn->is_websocket = true;
            conn->handshake_done = true;
            connection_add(conn);
            
            // Send test message
            ws_send_text(client_fd, "Hello from server!");
            
            // Read one message
            uint8_t buffer[4096];
            ssize_t n = socket_recv(client_fd, buffer, sizeof(buffer));
            if (n > 0) {
                WSFrame frame;
                if (ws_decode_frame(buffer, n, &frame) == 0) {
                    log_info("Received: %s", frame.payload);
                    ws_frame_free(&frame);
                }
            }
        }
        
        socket_close(client_fd);
    }
    
    socket_close(server_fd);
    connection_free_all();
    logger_close();
    
    printf("✓ Phase 3 completed!\n");
    return 0;
}
```

**Tạo simple web client (`client/test.html`):**
```html
<!DOCTYPE html>
<html>
<head>
    <title>WebSocket Test</title>
</head>
<body>
    <h1>WebSocket Test</h1>
    <button id="connect">Connect</button>
    <button id="send">Send Message</button>
    <div id="log"></div>
    
    <script>
        let ws = null;
        
        document.getElementById('connect').onclick = () => {
            ws = new WebSocket('ws://localhost:8080');
            
            ws.onopen = () => {
                log('Connected!');
            };
            
            ws.onmessage = (e) => {
                log('Received: ' + e.data);
            };
            
            ws.onerror = (e) => {
                log('Error: ' + e);
            };
        };
        
        document.getElementById('send').onclick = () => {
            if (ws) {
                ws.send('Hello from client!');
            }
        };
        
        function log(msg) {
            document.getElementById('log').innerHTML += msg + '<br>';
        }
    </script>
</body>
</html>
```

---

## 🎯 Phase 4: Game Logic (3-4 giờ)

### 4.1 `src/game/player.c/h`

**Header (`include/player.h`):**
```c
#ifndef PLAYER_H
#define PLAYER_H

#include <stdbool.h>
#include <time.h>

typedef enum {
    PLAYER_STATE_DISCONNECTED,
    PLAYER_STATE_CONNECTED,
    PLAYER_STATE_WAITING,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_DRAWING,
    PLAYER_STATE_GUESSING
} PlayerState;

typedef struct Player {
    int id;
    int fd;
    char name[64];
    int score;
    PlayerState state;
    bool is_drawer;
    bool has_guessed;
    int streak;
    time_t joined_at;
    
    struct Player* next;
} Player;

// CRUD operations
Player* player_create(int fd, const char* name);
void player_add(Player* player);
void player_remove(int player_id);
Player* player_find_by_id(int player_id);
Player* player_find_by_fd(int fd);
Player* player_find_by_name(const char* name);
Player* player_get_all(int* count);

// State management
void player_set_state(Player* player, PlayerState state);
void player_set_drawer(Player* player, bool is_drawer);
void player_mark_guessed(Player* player);
void player_reset_guess(Player* player);

// Score management
void player_add_score(Player* player, int points);
void player_reset_score(Player* player);
void player_increment_streak(Player* player);
void player_reset_streak(Player* player);

// Queries
int player_count(void);
Player* player_get_drawer(void);
bool player_is_drawer(const Player* player);
bool player_has_guessed_correct(const Player* player);
bool player_all_guessed(void);

// Cleanup
void player_free_all(void);

#endif
```

**Thứ tự implement:**
1. Player linked list management (20 phút)
2. CRUD operations (create, add, remove, find) (40 phút)
3. State management functions (20 phút)
4. Score và streak management (20 phút)
5. Query functions (drawer, all_guessed) (20 phút)

---

### 4.2 `src/game/round.c/h`

**Header (`include/round.h`):**
```c
#ifndef ROUND_H
#define ROUND_H

#include <time.h>
#include "player.h"

typedef struct {
    int round_number;
    char current_word[128];
    char hint[256];
    Player* drawer;
    time_t start_time;
    int duration;  // seconds
    bool is_active;
    int correct_guesses;
} Round;

// Round lifecycle
void round_init(Round* round);
int round_start(Round* round, int duration);
void round_end(Round* round);
bool round_is_active(const Round* round);
bool round_is_expired(const Round* round);

// Drawer selection
Player* round_select_next_drawer(void);
void round_set_drawer(Round* round, Player* drawer);

// Word management
const char* round_pick_word(void);
char* round_generate_hint(const char* word);
void round_reveal_hint(Round* round, int chars_to_reveal);

// Guess handling
bool round_check_guess(const Round* round, const char* guess);
void round_increment_correct_guesses(Round* round);

// Time tracking
int round_get_elapsed_seconds(const Round* round);
int round_get_remaining_seconds(const Round* round);

#endif
```

**Thứ tự implement:**
1. Round initialization và lifecycle (30 phút)
2. Drawer selection với rotation (25 phút)
3. Word picking (random from list) (15 phút)
4. Hint generation ("con mèo" → "_ _ _  _ _ _") (30 phút)
5. Guess checking (case-insensitive compare) (15 phút)
6. Time tracking functions (15 phút)

---

### 4.3 `src/game/scoring.c/h`

**Header (`include/scoring.h`):**
```c
#ifndef SCORING_H
#define SCORING_H

#include "player.h"
#include "round.h"

// Score calculation
int scoring_calculate_guess(int elapsed_seconds, int base_points);
int scoring_calculate_drawer(int correct_guesses, int points_per_guess);
int scoring_apply_streak_bonus(int base_points, int streak);

// Award points
void scoring_award_guess_points(Player* player, int elapsed_seconds);
void scoring_award_drawer_points(Player* drawer, int correct_guesses);

// Leaderboard
typedef struct {
    char name[64];
    int score;
    int rank;
} LeaderboardEntry;

LeaderboardEntry* scoring_get_leaderboard(int* count);
void scoring_leaderboard_free(LeaderboardEntry* entries);

#endif
```

**Scoring Logic:**
```
Time-based scoring (60s round):
- 0-10s:  100% of base points
- 11-20s: 80%
- 21-30s: 60%
- 31-40s: 40%
- 41-50s: 20%
- 51-60s: 10%

Streak bonus:
- 2 correct: +20%
- 3 correct: +50%
- 5+ correct: +100%

Drawer points:
- +5 points per correct guess
```

**Thứ tự implement:**
1. Time-based score calculation (20 phút)
2. Streak bonus calculation (15 phút)
3. Award points functions (20 phút)
4. Leaderboard generation (25 phút)

---

### 4.4 `src/game/game_state.c/h`

**Header (`include/game_state.h`):**
```c
#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "player.h"
#include "round.h"
#include <time.h>

typedef enum {
    GAME_STATE_IDLE,
    GAME_STATE_WAITING,
    GAME_STATE_ROUND_STARTING,
    GAME_STATE_ROUND_ACTIVE,
    GAME_STATE_ROUND_ENDING
} GameStateEnum;

typedef struct {
    GameStateEnum state;
    Round current_round;
    int total_rounds;
    int min_players;
    time_t last_state_change;
} GameState;

// Initialization
void game_state_init(GameState* gs, int min_players);
void game_state_reset(GameState* gs);

// State transitions
void game_state_set(GameState* gs, GameStateEnum new_state);
GameStateEnum game_state_get(const GameState* gs);
const char* game_state_to_string(GameStateEnum state);

// State checks
bool game_state_can_start(const GameState* gs);
bool game_state_is_active(const GameState* gs);
bool game_state_is_waiting(const GameState* gs);

// Game flow
void game_state_update(GameState* gs);
void game_state_start_round(GameState* gs);
void game_state_end_round(GameState* gs);

#endif
```

**State Machine:**
```
IDLE
  ↓ (min players joined)
WAITING (5s countdown)
  ↓
ROUND_STARTING (choose drawer, pick word)
  ↓
ROUND_ACTIVE (playing)
  ↓ (timeout or all guessed)
ROUND_ENDING (show results, 3s)
  ↓
WAITING (next round)
```

**Thứ tự implement:**
1. State management (init, set, get) (20 phút)
2. State checks (can_start, is_active) (15 phút)
3. Game flow (start_round, end_round) (30 phút)
4. Update loop logic (30 phút)

---

## ✅ Checkpoint Phase 4

**Test (`tests/test_phase4.c`):**
```c
#include <stdio.h>
#include "logger.h"
#include "player.h"
#include "round.h"
#include "game_state.h"
#include "scoring.h"

int main() {
    logger_init("logs/test.log", LOG_DEBUG);
    
    printf("=== Phase 4 Test ===\n");
    
    // Test player management
    Player* p1 = player_create(1, "Alice");
    Player* p2 = player_create(2, "Bob");
    player_add(p1);
    player_add(p2);
    
    log_info("Created 2 players");
    assert(player_count() == 2);
    
    // Test game state
    GameState gs;
    game_state_init(&gs, 2);
    
    if (game_state_can_start(&gs)) {
        log_info("Can start game with %d players", player_count());
        game_state_start_round(&gs);
        
        log_info("Round started. Word: %s", gs.current_round.current_word);
        log_info("Hint: %s", gs.current_round.hint);
        log_info("Drawer: %s", gs.current_round.drawer->name);
        
        // Test guessing
        Player* guesser = (gs.current_round.drawer == p1) ? p2 : p1;
        if (round_check_guess(&gs.current_round, gs.current_round.current_word)) {
            log_info("✓ Correct guess!");
            scoring_award_guess_points(guesser, 5);
            log_info("Score: %d", guesser->score);
        }
        
        game_state_end_round(&gs);
    }
    
    // Test leaderboard
    int count;
    LeaderboardEntry* leaderboard = scoring_get_leaderboard(&count);
    log_info("Leaderboard:");
    for (int i = 0; i < count; i++) {
        log_info("#%d: %s - %d points", i+1, leaderboard[i].name, leaderboard[i].score);
    }
    scoring_leaderboard_free(leaderboard);
    
    player_free_all();
    logger_close();
    
    printf("✓ Phase 4 completed!\n");
    return 0;
}
```

---

## 🎯 Phase 5: Data Management (2-3 giờ)

### 5.1 `src/data/words.c/h`

**Header (`include/words.h`):**
```c
#ifndef WORDS_H
#define WORDS_H

typedef enum {
    WORD_DIFFICULTY_EASY = 1,
    WORD_DIFFICULTY_MEDIUM = 2,
    WORD_DIFFICULTY_HARD = 3
} WordDifficulty;

typedef struct {
    char word[128];
    WordDifficulty difficulty;
    bool used;
} WordEntry;

// Initialization
int words_load(const char* filename);
void words_free(void);

// Selection
const char* words_get_random(void);
const char* words_get_by_difficulty(WordDifficulty difficulty);
const char* words_get_unused(void);

// Management
void words_mark_used(const char* word);
void words_reset_used(void);
int words_get_count(void);
int words_get_unused_count(void);

#endif
```

**Format file `config/words.txt`:**
```
# Dễ (3-6 ký tự)
easy:con mèo,con chó,cái bàn,cái ghế,quả táo,cái cây
easy:mặt trời,con cá,con vịt,cái cốc,cái bút

# Trung bình (7-12 ký tự)
medium:máy tính,điện thoại,bức tranh,đồng hồ,cầu vồng
medium:xe đạp,tủ lạnh,cửa sổ,bàn phím,con rùa

# Khó (13+ ký tự)
hard:kiến trúc,triết học,văn hóa,công nghệ,vũ trụ
hard:thư viện,bảo tàng,sân vận động,siêu thị
```

**Thứ tự implement:**
1. Parse words file (30 phút)
2. Store words trong array/list (20 phút)
3. Random selection (15 phút)
4. Track used words (20 phút)
5. Difficulty filtering (15 phút)

---

### 5.2 `src/data/database.c/h`

**Header (`include/database.h`):**
```c
#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>
#include "player.h"

// Initialization
int db_init(const char* db_path);
void db_close(void);
sqlite3* db_get_handle(void);

// Player operations
int db_save_player(const Player* player);
int db_load_player(const char* name, Player* player);
int db_update_player_score(const char* name, int score);
int db_player_exists(const char* name);

// Statistics
typedef struct {
    char name[64];
    int total_score;
    int games_played;
    int words_guessed;
    int words_drawn;
    double avg_guess_time;
} PlayerStats;

PlayerStats* db_get_player_stats(const char* name);
PlayerStats* db_get_leaderboard(int limit, int* count);
void db_stats_free(PlayerStats* stats);

// Game history
typedef struct {
    int id;
    char word[128];
    char drawer_name[64];
    char winner_name[64];
    int round_duration;
    time_t created_at;
} GameSession;

int db_save_game_session(const GameSession* session);
GameSession* db_get_recent_sessions(int limit, int* count);
void db_sessions_free(GameSession* sessions, int count);

// Word history
int db_mark_word_used(const char* word, const char* drawer_name);
int db_get_word_usage_count(const char* word);

#endif
```

**Database Schema:**
```sql
CREATE TABLE IF NOT EXISTS players (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    total_score INTEGER DEFAULT 0,
    games_played INTEGER DEFAULT 0,
    words_guessed INTEGER DEFAULT 0,
    words_drawn INTEGER DEFAULT 0,
    total_guess_time REAL DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_played TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS game_sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    word TEXT NOT NULL,
    drawer_name TEXT NOT NULL,
    winner_name TEXT,
    round_duration INTEGER,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS word_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    word TEXT NOT NULL,
    drawer_name TEXT NOT NULL,
    used_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_players_score ON players(total_score DESC);
CREATE INDEX IF NOT EXISTS idx_sessions_time ON game_sessions(created_at DESC);
```

**Thứ tự implement:**
1. `db_init()` - Create tables (30 phút)
2. Player CRUD operations (45 phút)
3. Statistics queries (40 phút)
4. Game session logging (30 phút)
5. Word history tracking (20 phút)

---

### 5.3 `src/data/cache.c/h` (Optional)

**Header (`include/cache.h`):**
```c
#ifndef CACHE_H
#define CACHE_H

#include <time.h>

typedef struct CacheEntry {
    char key[256];
    void* value;
    time_t expires_at;
    struct CacheEntry* next;
} CacheEntry;

// Cache management
void cache_init(void);
void cache_free(void);

// Operations
void cache_set(const char* key, void* value, int ttl_seconds);
void* cache_get(const char* key);
void cache_invalidate(const char* key);
void cache_clear(void);

// Maintenance
void cache_cleanup_expired(void);
int cache_get_size(void);

#endif
```

**Sử dụng:**
```c
// Cache leaderboard for 60 seconds
PlayerStats* stats = db_get_leaderboard(10, &count);
cache_set("leaderboard", stats, 60);

// Next request within 60s
PlayerStats* cached = cache_get("leaderboard");
if (cached) {
    return cached;  // No DB query
}
```

---

## ✅ Checkpoint Phase 5

**Test (`tests/test_phase5.c`):**
```c
#include <stdio.h>
#include "logger.h"
#include "words.h"
#include "database.h"

int main() {
    logger_init("logs/test.log", LOG_DEBUG);
    
    printf("=== Phase 5 Test ===\n");
    
    // Test words
    if (words_load("config/words.txt") == 0) {
        log_info("Loaded %d words", words_get_count());
        
        const char* word = words_get_random();
        log_info("Random word: %s", word);
        
        words_mark_used(word);
        log_info("Unused words: %d", words_get_unused_count());
    }
    
    // Test database
    if (db_init("data/test.db") == 0) {
        log_info("✓ Database initialized");
        
        // Save player
        Player p = {
            .id = 1,
            .fd = 5,
            .score = 100,
            .state = PLAYER_STATE_PLAYING
        };
        strncpy(p.name, "TestPlayer", sizeof(p.name));
        
        db_save_player(&p);
        log_info("✓ Saved player");
        
        // Get leaderboard
        int count;
        PlayerStats* leaderboard = db_get_leaderboard(10, &count);
        log_info("Leaderboard has %d entries", count);
        db_stats_free(leaderboard);
        
        db_close();
    }
    
    words_free();
    logger_close();
    
    printf("✓ Phase 5 completed!\n");
    return 0;
}
```

---

## 🎯 Phase 6: Message Handlers (3-4 giờ)

### 6.1 `src/handlers/handler_manager.c/h`

**Header (`include/handler_manager.h`):**
```c
#ifndef HANDLER_MANAGER_H
#define HANDLER_MANAGER_H

#include "message.h"
#include "game_state.h"

typedef int (*HandlerFunc)(GameState* gs, Message* msg);

// Initialization
void handler_init(void);

// Registration
void handler_register(MessageType type, HandlerFunc func);

// Dispatch
int handler_dispatch(GameState* gs, Message* msg);

// Error handling
typedef void (*ErrorHandlerFunc)(int fd, const char* error);
void handler_set_error_handler(ErrorHandlerFunc func);

#endif
```

**Thứ tự implement:**
1. Handler registry (hash map hoặc array) (20 phút)
2. Registration function (10 phút)
3. Dispatch function (20 phút)
4. Error handler mechanism (15 phút)

---

### 6.2 `src/handlers/join_handler.c/h`

**Header (`include/join_handler.h`):**
```c
#ifndef JOIN_HANDLER_H
#define JOIN_HANDLER_H

#include "game_state.h"
#include "message.h"

int join_handler(GameState* gs, Message* msg);

#endif
```

**Logic flow:**
```
1. Parse name from message
2. Validate name (length, charset)
3. Check if name already exists
4. Check game capacity
5. Create and add player
6. Send player list to new player
7. Broadcast player_joined to all
8. Check if can start game
9. If yes, start countdown/game
```

**Thứ tự implement:**
1. Parse và validate input (20 phút)
2. Player creation và add (15 phút)
3. Send responses (player list) (25 phút)
4. Broadcast join event (20 phút)
5. Auto-start game logic (20 phút)

---

### 6.3 `src/handlers/draw_handler.c/h`

**Header (`include/draw_handler.h`):**
```c
#ifndef DRAW_HANDLER_H
#define DRAW_HANDLER_H

#include "game_state.h"
#include "message.h"

int draw_handler(GameState* gs, Message* msg);

#endif
```

**Logic flow:**
```
1. Find player by sender_fd
2. Check if player is the drawer
3. Validate coordinates (0-1 range)
4. Validate color format
5. Validate brush size
6. Broadcast draw data to all except drawer
```

**Input format:**
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

**Thứ tự implement:**
1. Permission check (is drawer) (15 phút)
2. Validate draw data (20 phút)
3. Broadcast to others (20 phút)

---

### 6.4 `src/handlers/guess_handler.c/h`

**Header (`include/guess_handler.h`):**
```c
#ifndef GUESS_HANDLER_H
#define GUESS_HANDLER_H

#include "game_state.h"
#include "message.h"

int guess_handler(GameState* gs, Message* msg);

#endif
```

**Logic flow:**
```
1. Find player by sender_fd
2. Check player is not drawer
3. Check player hasn't guessed yet
4. Normalize guess (trim, lowercase)
5. Compare with answer (case-insensitive)
6. If correct:
   - Calculate points (time-based)
   - Award points
   - Mark player as guessed
   - Broadcast correct message
   - Check if all guessed
   - If yes, end round
7. If wrong:
   - Broadcast as chat message
```

**Thứ tự implement:**
1. Validation (not drawer, hasn't guessed) (20 phút)
2. Normalize và compare guess (15 phút)
3. Correct guess handling (35 phút)
4. Wrong guess handling (15 phút)
5. End round check (15 phút)

---

### 6.5 `src/handlers/chat_handler.c/h`

**Header (`include/chat_handler.h`):**
```c
#ifndef CHAT_HANDLER_H
#define CHAT_HANDLER_H

#include "game_state.h"
#include "message.h"

int chat_handler(GameState* gs, Message* msg);

#endif
```

**Logic flow:**
```
1. Find player by sender_fd
2. Validate message (length, not empty)
3. Sanitize message (optional: filter profanity)
4. Broadcast chat to all players
```

**Thứ tự implement:**
1. Validate message (15 phút)
2. Create chat broadcast (15 phút)
3. Send to all (10 phút)

---

### 6.6 `src/handlers/leave_handler.c/h`

**Header (`include/leave_handler.h`):**
```c
#ifndef LEAVE_HANDLER_H
#define LEAVE_HANDLER_H

#include "game_state.h"
#include "message.h"

int leave_handler(GameState* gs, Message* msg);

#endif
```

**Logic flow:**
```
1. Find player by sender_fd
2. Check if player is drawer
3. If drawer leaves, end round
4. Remove player
5. Broadcast player_left
6. Check if too few players
7. If yes, end game or wait
```

---

## ✅ Checkpoint Phase 6

**Test handlers integration:**
```c
// tests/test_phase6.c
#include "handler_manager.h"
#include "join_handler.h"
#include "guess_handler.h"

int main() {
    logger_init("logs/test.log", LOG_DEBUG);
    
    printf("=== Phase 6 Test ===\n");
    
    // Initialize
    handler_init();
    words_load("config/words.txt");
    db_init("data/test.db");
    
    // Register handlers
    handler_register(MSG_TYPE_JOIN, join_handler);
    handler_register(MSG_TYPE_GUESS, guess_handler);
    handler_register(MSG_TYPE_CHAT, chat_handler);
    
    // Create game state
    GameState gs;
    game_state_init(&gs, 2);
    
    // Simulate join
    Message* msg1 = message_parse("{\"type\":\"join\",\"name\":\"Alice\"}", 5);
    handler_dispatch(&gs, msg1);
    message_free(msg1);
    
    Message* msg2 = message_parse("{\"type\":\"join\",\"name\":\"Bob\"}", 6);
    handler_dispatch(&gs, msg2);
    message_free(msg2);
    
    log_info("Player count: %d", player_count());
    
    // Start round
    if (game_state_can_start(&gs)) {
        game_state_start_round(&gs);
        log_info("Round started");
    }
    
    // Simulate guess
    Message* msg3 = message_parse("{\"type\":\"guess\",\"guess\":\"test\"}", 6);
    handler_dispatch(&gs, msg3);
    message_free(msg3);
    
    player_free_all();
    db_close();
    words_free();
    logger_close();
    
    printf("✓ Phase 6 completed!\n");
    return 0;
}
```

---

## 🎯 Phase 7: Main Server Loop (3-4 giờ)

### 7.1 `src/core/server.c/h`

**Header (`include/server.h`):**
```c
#ifndef SERVER_H
#define SERVER_H

#include "game_state.h"

// Server lifecycle
int server_init(int port);
void server_run(void);
void server_shutdown(void);

// Connection handling
void server_accept_connection(void);
void server_handle_client_data(int client_fd);
void server_disconnect_client(int client_fd);

// Broadcasting
void server_broadcast(const char* message);
void server_broadcast_except(const char* message, int exclude_fd);
void server_send_to(int fd, const char* message);

// State
GameState* server_get_game_state(void);

#endif
```

**Main server loop architecture:**
```c
void server_run() {
    while (server_running) {
        // 1. select() or epoll_wait()
        
        // 2. Accept new connections
        
        // 3. Read from active connections
        //    - WebSocket handshake if needed
        //    - Decode WebSocket frames
        //    - Parse messages
        //    - Dispatch to handlers
        
        // 4. Update game state
        //    - Check round timeout
        //    - Auto-start rounds
        //    - Clean up timeouts
        
        // 5. Send responses/broadcasts
    }
}
```

**Thứ tự implement:**

1. **Server initialization** (30 phút)
   - Create socket
   - Bind và listen
   - Initialize game state
   - Setup signal handlers (SIGINT, SIGTERM)

2. **Main loop với select()** (60 phút)
   - fd_set management
   - select() timeout
   - Handle multiple file descriptors

3. **Accept new connections** (30 phút)
   - Accept socket
   - Create Connection
   - Add to connection list

4. **Handle client data** (60 phút)
   - Read from socket
   - WebSocket handshake
   - Decode frames
   - Parse messages
   - Dispatch to handlers

5. **Broadcasting** (30 phút)
   - Iterate connections
   - Encode and send

6. **Game state updates** (30 phút)
   - Periodic checks
   - Timeout handling
   - Auto-transitions

7. **Cleanup và shutdown** (20 phút)
   - Close all connections
   - Free resources
   - Save state to DB

---

### 7.2 `src/core/main.c`

```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "logger.h"
#include "config.h"
#include "database.h"
#include "words.h"
#include "server.h"
#include "handler_manager.h"

// Global server config
static ServerConfig* config = NULL;
static volatile sig_atomic_t server_running = 1;

void signal_handler(int signum) {
    (void)signum;
    server_running = 0;
    log_info("Shutdown signal received");
}

void register_all_handlers() {
    handler_register(MSG_TYPE_JOIN, join_handler);
    handler_register(MSG_TYPE_LEAVE, leave_handler);
    handler_register(MSG_TYPE_DRAW, draw_handler);
    handler_register(MSG_TYPE_GUESS, guess_handler);
    handler_register(MSG_TYPE_CHAT, chat_handler);
}

void cleanup_all() {
    log_info("Cleaning up...");
    server_shutdown();
    player_free_all();
    words_free();
    db_close();
    config_free(config);
    logger_close();
}

int main(int argc, char* argv[]) {
    // Parse arguments
    const char* config_file = (argc > 1) ? argv[1] : "config/server.conf";
    
    // Load config
    config = config_load(config_file);
    if (!config) {
        fprintf(stderr, "Failed to load config\n");
        return 1;
    }
    
    // Initialize logger
    if (logger_init(config_get_log_file(config), LOG_INFO) != 0) {
        fprintf(stderr, "Failed to initialize logger\n");
        config_free(config);
        return 1;
    }
    
    log_info("=== Draw & Guess Server Starting ===");
    log_info("Version: 1.0.0");
    log_info("Port: %d", config_get_port(config));
    
    // Initialize modules
    if (db_init(config_get_db_path(config)) != 0) {
        log_error("Failed to initialize database");
        cleanup_all();
        return 1;
    }
    
    if (words_load(config_get_words_file(config)) != 0) {
        log_error("Failed to load words");
        cleanup_all();
        return 1;
    }
    log_info("Loaded %d words", words_get_count());
    
    // Register handlers
    handler_init();
    register_all_handlers();
    log_info("Registered message handlers");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize and run server
    if (server_init(config_get_port(config)) != 0) {
        log_error("Failed to initialize server");
        cleanup_all();
        return 1;
    }
    
    log_info("Server running on port %d", config_get_port(config));
    log_info("Press Ctrl+C to stop");
    
    server_run();
    
    // Cleanup
    cleanup_all();
    log_info("Server stopped");
    
    return 0;
}
```

---

## ✅ Checkpoint Phase 7

**Compile toàn bộ project:**
```bash
make clean
make
```

**Chạy server:**
```bash
./bin/draw_guess_server config/server.conf
```

**Test với web client:**
1. Mở `client/index.html` trong browser
2. Connect to ws://localhost:8080
3. Join với tên
4. Test draw, guess, chat

---

## 🎯 Phase 8: Web Client (4-6 giờ)

### 8.1 Cấu trúc client

```
client/
├── index.html
├── css/
│   └── style.css
├── js/
│   ├── main.js
│   ├── websocket.js
│   ├── canvas.js
│   ├── ui.js
│   └── game.js
└── assets/
    ├── sounds/
    └── images/
```

### 8.2 `client/index.html`

**Cấu trúc cơ bản:**
```html
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Draw & Guess</title>
    <link rel="stylesheet" href="css/style.css">
</head>
<body>
    <!-- Join Screen -->
    <div id="join-screen">
        <h1>Draw & Guess</h1>
        <input type="text" id="player-name" placeholder="Nhập tên của bạn" maxlength="20">
        <button id="join-btn">Tham gia</button>
    </div>
    
    <!-- Game Screen -->
    <div id="game-screen" style="display: none;">
        <!-- Header -->
        <div id="header">
            <div id="round-info">Round <span id="round-number">1</span></div>
            <div id="timer">60</div>
            <div id="word-hint">_ _ _ _ _</div>
        </div>
        
        <!-- Main content -->
        <div id="main-content">
            <!-- Left: Players -->
            <div id="players-panel">
                <h3>Người chơi</h3>
                <div id="players-list"></div>
            </div>
            
            <!-- Center: Canvas -->
            <div id="canvas-container">
                <canvas id="draw-canvas" width="800" height="600"></canvas>
                
                <!-- Drawing tools -->
                <div id="tools">
                    <input type="color" id="color-picker" value="#000000">
                    <input type="range" id="brush-size" min="1" max="20" value="3">
                    <button id="clear-btn">Xóa</button>
                </div>
            </div>
            
            <!-- Right: Chat -->
            <div id="chat-panel">
                <h3>Chat</h3>
                <div id="chat-messages"></div>
                <div id="chat-input-container">
                    <input type="text" id="chat-input" placeholder="Nhập đoán hoặc chat...">
                    <button id="send-btn">Gửi</button>
                </div>
            </div>
        </div>
    </div>
    
    <script src="js/websocket.js"></script>
    <script src="js/canvas.js"></script>
    <script src="js/ui.js"></script>
    <script src="js/game.js"></script>
    <script src="js/main.js"></script>
</body>
</html>
```

### 8.3 `client/js/websocket.js`

```javascript
class WebSocketClient {
    constructor(url) {
        this.url = url;
        this.ws = null;
        this.handlers = {};
    }
    
    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            
            this.ws.onopen = () => {
                console.log('Connected');
                resolve();
            };
            
            this.ws.onmessage = (event) => {
                const msg = JSON.parse(event.data);
                this.handleMessage(msg);
            };
            
            this.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                reject(error);
            };
            
            this.ws.onclose = () => {
                console.log('Disconnected');
                this.reconnect();
            };
        });
    }
    
    send(type, data) {
        const msg = { type, ...data };
        this.ws.send(JSON.stringify(msg));
    }
    
    on(type, handler) {
        this.handlers[type] = handler;
    }
    
    handleMessage(msg) {
        const handler = this.handlers[msg.type];
        if (handler) {
            handler(msg);
        }
    }
    
    reconnect() {
        setTimeout(() => {
            console.log('Reconnecting...');
            this.connect();
        }, 3000);
    }
}
```

### 8.4 `client/js/canvas.js`

```javascript
class DrawingCanvas {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.drawing = false;
        this.currentColor = '#000000';
        this.currentSize = 3;
        this.canDraw = false;
        
        this.setupEventListeners();
    }
    
    setupEventListeners() {
        this.canvas.addEventListener('mousedown', (e) => this.startDrawing(e));
        this.canvas.addEventListener('mousemove', (e) => this.draw(e));
        this.canvas.addEventListener('mouseup', () => this.stopDrawing());
        this.canvas.addEventListener('mouseout', () => this.stopDrawing());
    }
    
    startDrawing(e) {
        if (!this.canDraw) return;
        
        this.drawing = true;
        const pos = this.getMousePos(e);
        
        // Send to server
        window.ws.send('draw', {
            x: pos.x / this.canvas.width,
            y: pos.y / this.canvas.