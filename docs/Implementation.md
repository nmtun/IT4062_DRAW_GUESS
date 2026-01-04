# DRAW & GUESS - MINI PROJECT
## Hướng dẫn Implementation cho Học phần Lập trình Mạng

---

## 📋 MỤC LỤC
1. [Tổng quan dự án](#1-tổng-quan-dự-án)
2. [Cấu trúc thư mục](#2-cấu-trúc-thư-mục)
3. [Thiết kế Protocol](#3-thiết-kế-protocol)
4. [Thiết kế Database](#4-thiết-kế-database)
5. [Hướng dẫn Implementation](#5-hướng-dẫn-implementation)
6. [Thứ tự triển khai](#6-thứ-tự-triển-khai)
7. [Testing](#7-testing)

---

## 1. TỔNG QUAN DỰ ÁN

### 1.1 Mô tả Game
**Draw & Guess** là game multiplayer real-time:
- **Số người chơi**: 2-8 người
- **Luật chơi**:
  - Mỗi round: 1 người được chọn làm **drawer**, nhận từ bí mật
  - Drawer vẽ tranh để gợi ý (không được viết chữ/số)
  - Người chơi khác đoán từ qua chat
  - Đoán đúng → +10 điểm (người đoán) và +5 điểm (drawer)
  - Mỗi round: 60 giây
  - Game kết thúc sau N rounds hoặc khi chỉ còn 1 người

### 1.2 Chức năng chính
- ✅ Đăng ký/Đăng nhập (MySQL)
- ✅ Tạo/Tham gia phòng chơi
- ✅ Vẽ real-time (canvas)
- ✅ Chat + đoán từ
- ✅ Tính điểm và xếp hạng
- ✅ Lưu lịch sử trận đấu

### 1.3 Công nghệ
- **Server**: C + TCP Sockets + MySQL
- **Client**: HTML5 + Canvas + WebSocket-like communication
- **Database**: MySQL 8.0+
- **I/O Model**: select() hoặc poll() để xử lý multiple clients

---

## 2. CẤU TRÚC THƯ MỤC

```
draw-guess/
├── server/
│   ├── main.c                  # Entry point của server
│   ├── server.c/h              # TCP server core
│   ├── auth.c/h                # Xác thực người dùng
│   ├── database.c/h            # Kết nối MySQL
│   ├── room.c/h                # Quản lý phòng chơi
│   ├── game.c/h                # Game logic
│   ├── protocol.c/h            # Xử lý protocol messages
│   ├── drawing.c/h             # Xử lý drawing data
│   └── utils.c/h               # Utility functions
│
├── client/
│   ├── index.html              # Trang chủ (login/register)
│   ├── lobby.html              # Lobby (danh sách phòng)
│   ├── game.html               # Game room
│   ├── js/
│   │   ├── network.js          # Kết nối với server
│   │   ├── canvas.js           # Vẽ canvas
│   │   ├── game.js             # Game UI logic
│   │   └── chat.js             # Chat interface
│   └── css/
│       └── style.css
│
├── common/
│   └── protocol.h              # Protocol definitions (shared)
│
├── database/
│   └── schema.sql              # Database schema
│
├── data/
│   └── words.txt               # Danh sách từ để đoán
│
├── Makefile
└── README.md
```

---

## 3. THIẾT KẾ PROTOCOL

### 3.1 Message Format
Tất cả messages đều có format:
```
[TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]
```

### 3.2 Message Types

#### **Authentication (0x01 - 0x0F)**
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x01 | LOGIN_REQUEST | C→S | Đăng nhập |
| 0x02 | LOGIN_RESPONSE | S→C | Kết quả đăng nhập |
| 0x03 | REGISTER_REQUEST | C→S | Đăng ký tài khoản |
| 0x04 | REGISTER_RESPONSE | S→C | Kết quả đăng ký |
| 0x05 | LOGOUT | C→S | Đăng xuất |

**Payload Examples:**
```c
// LOGIN_REQUEST: username(32) + password(32)
// LOGIN_RESPONSE: status(1) + user_id(4) + username(32)
// REGISTER_REQUEST: username(32) + password(32) + email(64)
// REGISTER_RESPONSE: status(1) + message(128)
```

#### **Room Management (0x10 - 0x1F)**
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x10 | ROOM_LIST_REQUEST | C→S | Yêu cầu danh sách phòng |
| 0x11 | ROOM_LIST_RESPONSE | S→C | Danh sách phòng |
| 0x12 | CREATE_ROOM | C→S | Tạo phòng mới |
| 0x13 | JOIN_ROOM | C→S | Vào phòng |
| 0x14 | LEAVE_ROOM | C→S | Rời phòng |
| 0x15 | ROOM_UPDATE | S→C | Cập nhật trạng thái phòng |
| 0x16 | START_GAME | C→S | Bắt đầu game |

**Payload Examples:**
```c
// ROOM_LIST_RESPONSE: room_count(1) + [room_id(4) + room_name(32) + players(1) + max_players(1)]...
// CREATE_ROOM: room_name(32) + max_players(1) + rounds(1)
// JOIN_ROOM: room_id(4)
// ROOM_UPDATE: room_id(4) + status(1) + player_count(1) + [player_info]...
```

#### **Game Play (0x20 - 0x2F)**
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x20 | GAME_START | S→C | Bắt đầu round mới |
| 0x21 | GAME_STATE | S→C | Trạng thái game |
| 0x22 | DRAW_DATA | C→S | Dữ liệu vẽ |
| 0x23 | DRAW_BROADCAST | S→C | Broadcast dữ liệu vẽ |
| 0x24 | GUESS_WORD | C→S | Đoán từ |
| 0x25 | CORRECT_GUESS | S→C | Đoán đúng |
| 0x26 | WRONG_GUESS | S→C | Đoán sai |
| 0x27 | ROUND_END | S→C | Kết thúc round |
| 0x28 | GAME_END | S→C | Kết thúc game |
| 0x29 | HINT | S→C | Gợi ý (vd: "_ _ _ t") |

**Payload Examples:**
```c
// GAME_START: drawer_id(4) + word_length(1) + time_limit(2)
// DRAW_DATA: action(1) + x1(2) + y1(2) + x2(2) + y2(2) + color(4) + width(1)
//   action: 0=move, 1=line, 2=clear
// GUESS_WORD: word(64)
// CORRECT_GUESS: player_id(4) + word(64) + points(2)
// ROUND_END: word(64) + [player_id(4) + score(2)]...
```

#### **Chat (0x30 - 0x3F)**
| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x30 | CHAT_MESSAGE | C→S | Gửi chat |
| 0x31 | CHAT_BROADCAST | S→C | Broadcast chat |

**Payload Examples:**
```c
// CHAT_MESSAGE: message(256)
// CHAT_BROADCAST: username(32) + message(256) + timestamp(8)
```

---

## 4. THIẾT KẾ DATABASE

### 4.1 Database Schema

```sql
-- Database: draw_guess_db

-- Bảng users
CREATE TABLE users (
    user_id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL,
    password_hash VARCHAR(64) NOT NULL,  -- SHA256
    email VARCHAR(64) UNIQUE NOT NULL,
    total_games INT DEFAULT 0,
    total_wins INT DEFAULT 0,
    total_score INT DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP NULL,
    INDEX idx_username (username)
);

-- Bảng game_history
CREATE TABLE game_history (
    game_id INT PRIMARY KEY AUTO_INCREMENT,
    room_name VARCHAR(32),
    winner_id INT,
    total_rounds INT,
    game_duration INT,  -- seconds
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (winner_id) REFERENCES users(user_id)
);

-- Bảng game_players (nhiều-nhiều)
CREATE TABLE game_players (
    game_id INT,
    user_id INT,
    final_score INT,
    rounds_won INT,
    words_guessed INT,
    PRIMARY KEY (game_id, user_id),
    FOREIGN KEY (game_id) REFERENCES game_history(game_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id)
);

-- Bảng words
CREATE TABLE words (
    word_id INT PRIMARY KEY AUTO_INCREMENT,
    word VARCHAR(64) NOT NULL,
    difficulty ENUM('easy', 'medium', 'hard') DEFAULT 'medium',
    category VARCHAR(32),
    times_used INT DEFAULT 0
);
```

### 4.2 Sample Data
```sql
-- Insert sample words
INSERT INTO words (word, difficulty, category) VALUES
('cat', 'easy', 'animal'),
('house', 'easy', 'object'),
('butterfly', 'medium', 'animal'),
('computer', 'medium', 'technology'),
('astronaut', 'hard', 'profession');
```

---

## 5. HƯỚNG DẪN IMPLEMENTATION

### 5.1 Server Components

#### **A. main.c**
```
Mục đích: Entry point, khởi tạo server

Các hàm chính:
- main()
  • Parse command line arguments (port, max_clients)
  • Khởi tạo database connection
  • Khởi tạo server socket
  • Load words từ database/file
  • Vào main event loop
  • Cleanup khi shutdown
```

#### **B. server.c/h**
```
Mục đích: TCP server core, quản lý connections

Struct:
- server_t
  • int listen_fd
  • int max_clients
  • client_t* clients[MAX_CLIENTS]
  • room_t* rooms[MAX_ROOMS]
  • fd_set master_set, read_set
  • int max_fd

- client_t
  • int sockfd
  • int user_id
  • char username[32]
  • int room_id
  • enum client_state (LOGGED_OUT, LOGGED_IN, IN_ROOM, IN_GAME)
  • time_t last_activity

Các hàm chính:
- server_init(port, max_clients)
  • Tạo socket, bind, listen
  • Khởi tạo fd_set
  • Return server_t*

- server_run(server_t*)
  • Main loop với select()/poll()
  • Accept connections mới
  • Đọc data từ clients
  • Xử lý disconnections

- server_accept_client(server_t*)
  • Accept connection
  • Tạo client_t mới
  • Thêm vào clients array và fd_set

- server_handle_client_data(server_t*, client_t*)
  • Đọc message từ socket
  • Parse message type
  • Gọi protocol handler tương ứng

- server_remove_client(server_t*, client_t*)
  • Remove khỏi fd_set
  • Close socket
  • Cleanup resources
  • Nếu đang trong game → notify room

- server_broadcast_to_room(room_id, message, exclude_client_id)
  • Gửi message đến tất cả clients trong room
  • Có thể exclude 1 client (vd: sender)

- server_cleanup(server_t*)
  • Close tất cả sockets
  • Free memory
  • Disconnect database
```

#### **C. database.c/h**
```
Mục đích: Quản lý MySQL connection và queries

Struct:
- db_connection_t
  • MYSQL* conn
  • char host[64]
  • char user[32]
  • char password[64]
  • char database[32]

Các hàm chính:
- db_connect(host, user, password, database)
  • Kết nối đến MySQL
  • Set charset utf8mb4
  • Return db_connection_t*

- db_disconnect(db_connection_t*)
  • Đóng connection
  • Free resources

- db_execute_query(db_connection_t*, query, params...)
  • Execute prepared statement
  • Return MYSQL_RES*

- db_register_user(db_connection_t*, username, password_hash, email)
  • INSERT INTO users
  • Return user_id hoặc -1 nếu lỗi

- db_authenticate_user(db_connection_t*, username, password_hash)
  • SELECT user_id FROM users WHERE...
  • UPDATE last_login
  • Return user_id hoặc -1 nếu thất bại

- db_get_user_stats(db_connection_t*, user_id, user_stats_t*)
  • SELECT total_games, total_wins, total_score
  • Populate struct

- db_update_user_stats(db_connection_t*, user_id, stats)
  • UPDATE users SET total_games += 1...

- db_save_game_result(db_connection_t*, game_result_t*)
  • INSERT INTO game_history
  • INSERT INTO game_players (multiple rows)

- db_get_random_word(db_connection_t*, difficulty)
  • SELECT word FROM words ORDER BY RAND() LIMIT 1
  • UPDATE times_used
  • Return word string

- db_get_leaderboard(db_connection_t*, limit)
  • SELECT TOP N users ORDER BY total_score DESC
  • Return array of user_stats_t
```

#### **D. auth.c/h**
```
Mục đích: Xử lý authentication

Các hàm chính:
- auth_hash_password(password, hash_output)
  • SHA256 hashing
  • Salt (optional)

- auth_verify_password(password, hash)
  • Hash input password
  • Compare với stored hash

- auth_handle_login(server_t*, client_t*, login_request)
  • Parse username + password từ payload
  • Hash password
  • Gọi db_authenticate_user()
  • Nếu thành công:
    - Cập nhật client->user_id, username
    - client->state = LOGGED_IN
    - Gửi LOGIN_RESPONSE (success)
  • Nếu thất bại:
    - Gửi LOGIN_RESPONSE (fail + reason)

- auth_handle_register(server_t*, client_t*, register_request)
  • Parse username + password + email
  • Validate input (length, characters)
  • Hash password
  • Gọi db_register_user()
  • Gửi REGISTER_RESPONSE

- auth_handle_logout(server_t*, client_t*)
  • Nếu đang trong room → leave room
  • Reset client state
  • client->state = LOGGED_OUT
```

#### **E. room.c/h**
```
Mục đích: Quản lý game rooms

Struct:
- room_t
  • int room_id
  • char room_name[32]
  • int owner_id
  • int players[MAX_PLAYERS_PER_ROOM]
  • int player_count
  • int max_players
  • enum room_state (WAITING, PLAYING, FINISHED)
  • game_state_t* game  // NULL nếu chưa chơi

Các hàm chính:
- room_create(room_name, owner_id, max_players, rounds)
  • Allocate room_t
  • Thêm owner vào players[]
  • room->state = WAITING
  • Return room_t*

- room_destroy(room_t*)
  • Free game_state nếu có
  • Free room_t

- room_add_player(room_t*, client_t*)
  • Kiểm tra room đã full chưa
  • Thêm player_id vào array
  • Broadcast ROOM_UPDATE đến tất cả players
  • Return success/fail

- room_remove_player(room_t*, user_id)
  • Remove khỏi players[]
  • Nếu là owner → chuyển owner hoặc close room
  • Nếu đang chơi → pause/end game
  • Broadcast ROOM_UPDATE

- room_get_list(server_t*, room_info_array)
  • Duyệt qua tất cả rooms
  • Populate array với room info
  • Return count

- room_start_game(room_t*, server_t*)
  • Kiểm tra có đủ 2 players không
  • Khởi tạo game_state_t
  • room->state = PLAYING
  • Gọi game_start_round()
```

#### **F. game.c/h**
```
Mục đích: Core game logic

Struct:
- game_state_t
  • room_t* room
  • int current_round
  • int total_rounds
  • int drawer_id
  • int drawer_index  // index trong players[]
  • char current_word[64]
  • int word_length
  • bool word_guessed
  • time_t round_start_time
  • int time_limit  // seconds
  • player_score_t scores[MAX_PLAYERS]
  • bool game_ended

- player_score_t
  • int user_id
  • int score
  • int words_guessed
  • int rounds_won

Các hàm chính:
- game_init(room_t*, rounds, time_limit)
  • Allocate game_state_t
  • Khởi tạo scores[] cho tất cả players
  • current_round = 0
  • Random drawer_index
  • Return game_state_t*

- game_start_round(game_state_t*, server_t*)
  • current_round++
  • drawer_id = players[drawer_index]
  • Lấy random word từ database
  • current_word = word
  • word_guessed = false
  • round_start_time = time(NULL)
  • Gửi GAME_START đến tất cả (drawer biết từ, người khác chỉ biết length)
  • Start timer (có thể dùng signal SIGALRM hoặc check trong main loop)

- game_check_timeout(game_state_t*, server_t*)
  • if (time(NULL) - round_start_time > time_limit)
    - Gọi game_end_round(false)

- game_handle_draw_data(game_state_t*, client_t*, draw_data)
  • Kiểm tra client->user_id == drawer_id
  • Nếu không phải drawer → ignore/error
  • Broadcast DRAW_BROADCAST đến tất cả (trừ drawer)

- game_handle_guess(game_state_t*, client_t*, guess_word, server_t*)
  • Kiểm tra client->user_id != drawer_id
  • So sánh guess_word với current_word (case-insensitive)
  • Nếu đúng:
    - word_guessed = true
    - Cộng điểm cho guesser (+10) và drawer (+5)
    - Gửi CORRECT_GUESS
    - Gọi game_end_round(true)
  • Nếu sai:
    - Gửi WRONG_GUESS (hoặc broadcast chat)

- game_end_round(game_state_t*, success, server_t*)
  • Gửi ROUND_END (công bố từ + điểm hiện tại)
  • Nếu current_round >= total_rounds:
    - Gọi game_end()
  • Ngược lại:
    - drawer_index = (drawer_index + 1) % player_count
    - Delay 3 giây
    - Gọi game_start_round()

- game_end(game_state_t*, server_t*)
  • Tính người thắng (điểm cao nhất)
  • Gửi GAME_END (final scores + winner)
  • Lưu kết quả vào database
  • room->state = FINISHED
  • Reset hoặc destroy game_state_t

- game_send_hint(game_state_t*, server_t*)
  • Sau 20 giây → gửi hint (vd: "_ _ t")
  • Sau 40 giây → gửi hint (vd: "c _ t")

- game_get_scores(game_state_t*, scores_array)
  • Copy scores[] to output array
  • Sort by score descending
```

#### **G. protocol.c/h**
```
Mục đích: Parse và handle protocol messages

Struct:
- message_t
  • uint8_t type
  • uint16_t length
  • uint8_t* payload

Các hàm chính:
- protocol_parse_message(buffer, buffer_len, message_t*)
  • Đọc type (1 byte)
  • Đọc length (2 bytes, network byte order)
  • Đọc payload (length bytes)
  • Validate format
  • Return success/fail

- protocol_create_message(type, payload, payload_len, buffer_out)
  • Tạo message theo format
  • Convert length sang network byte order
  • Copy vào buffer_out
  • Return total message length

- protocol_handle_message(server_t*, client_t*, message_t*)
  • Switch case theo message type
  • Gọi handler function tương ứng:
    - 0x01: auth_handle_login()
    - 0x03: auth_handle_register()
    - 0x12: room_handle_create()
    - 0x13: room_handle_join()
    - 0x22: game_handle_draw_data()
    - 0x24: game_handle_guess()
    - ...

- protocol_send_room_list(client_t*, rooms[], room_count)
  • Tạo ROOM_LIST_RESPONSE message
  • Serialize room info
  • Send qua socket

- protocol_send_game_start(room_t*, drawer_id, word_length)
  • Tạo GAME_START message
  • Gửi riêng cho drawer (có word) và players khác (chỉ length)

- protocol_send_chat(room_t*, username, message)
  • Tạo CHAT_BROADCAST message
  • Broadcast đến tất cả trong room

- protocol_broadcast_draw(room_t*, draw_data, exclude_client_id)
  • Tạo DRAW_BROADCAST message
  • Gửi đến tất cả (trừ drawer)
```

#### **H. drawing.c/h**
```
Mục đích: Xử lý drawing data

Struct:
- draw_action_t
  • enum action_type (MOVE, LINE, CLEAR)
  • int16_t x1, y1, x2, y2
  • uint32_t color  // RGBA
  • uint8_t width

Các hàm chính:
- drawing_parse_action(payload, draw_action_t*)
  • Parse payload thành struct
  • Validate coordinates (0 <= x,y <= canvas_size)
  • Validate color và width

- drawing_serialize_action(draw_action_t*, buffer_out)
  • Serialize struct sang bytes
  • Return buffer length

- drawing_validate_action(draw_action_t*)
  • Kiểm tra giá trị hợp lệ
  • Không cho phép text/numbers (optional: có thể phức tạp)
```

#### **I. utils.c/h**
```
Mục đích: Utility functions

Các hàm chính:
- utils_get_timestamp()
  • Return current time as uint64_t milliseconds

- utils_generate_room_id()
  • Generate unique room ID

- utils_safe_strcpy(dest, src, max_len)
  • String copy an toàn, null-terminated

- utils_load_words_from_file(filename, words_array, max_words)
  • Đọc file words.txt
  • Parse mỗi dòng
  • Return word count

- utils_get_random_word(words_array, count, difficulty)
  • Random word theo difficulty

- utils_string_to_lower(str)
  • Convert to lowercase (cho so sánh guess)

- utils_log(level, format, ...)
  • Log messages (INFO, WARNING, ERROR)
  • Có thể ghi ra file hoặc stdout

- utils_send_all(sockfd, buffer, length)
  • Loop send() until all bytes sent
  • Handle partial sends

- utils_recv_all(sockfd, buffer, length)
  • Loop recv() until all bytes received
  • Handle partial receives
```

### 5.2 Client Components (Web)

#### **A. index.html**
```
- Login form (username + password)
- Register form (username + email + password)
- Submit → gửi LOGIN_REQUEST/REGISTER_REQUEST
- Nhận response → redirect to lobby.html
```

#### **B. lobby.html**
```
- Hiển thị danh sách phòng (room_id, name, players)
- Button: Create Room, Join Room, Refresh
- WebSocket connection đến server
- Nhận ROOM_UPDATE realtime
```

#### **C. game.html**
```
- Canvas vẽ (800x600)
- Chat box
- Player list + scores
- Timer countdown
- Current word hint (cho non-drawers)
- Tools: color picker, brush size, clear canvas
```

#### **D. js/network.js**
```
Các hàm:
- connectToServer(ip, port)
  • WebSocket/TCP connection (qua proxy nếu cần)

- sendMessage(type, payload)
  • Tạo message theo protocol
  • Send qua socket

- onMessageReceived(callback)
  • Register callback cho incoming messages
  • Parse message type
  • Call appropriate handler

- sendLogin(username, password)
- sendRegister(username, email, password)
- sendCreateRoom(name, max_players, rounds)
- sendJoinRoom(room_id)
- sendDrawData(action, x1, y1, x2, y2, color, width)
- sendGuess(word)
- sendChat(message)
```

#### **E. js/canvas.js**
```
Các hàm:
- initCanvas(canvasElement)
  • Get 2D context
  • Setup mouse/touch events

- onMouseDown(e)
- onMouseMove(e)
- onMouseUp(e)
  • Capture drawing
  • Gửi DRAW_DATA đến server

- drawLine(x1, y1, x2, y2, color, width)
  • Vẽ line trên canvas

- clearCanvas()
  • Clear toàn bộ canvas

- setColor(color)
- setBrushSize(size)

- isDrawingAllowed()
  • Check xem player có phải drawer không
```

#### **F. js/game.js**
```
Các hàm:
- initGame()
  • Setup UI elements
  • Register event listeners

- onGameStart(drawer_id, word_length, time_limit)
  • Nếu là drawer:
    - Hiển thị word
    - Enable canvas
  • Nếu không:
    - Hiển thị hint ("_ _ _ _ _")
    - Disable canvas
  • Start countdown timer

- onDrawReceived(draw_data)
  • Gọi canvas.drawLine()

- onCorrectGuess(player_id, word, points)
  • Hiển thị notification
  • Update scores

- onRoundEnd(word, scores)
  • Hiển thị kết quả round
  • Update leaderboard

- onGameEnd(scores, winner)
  • Hiển thị final results
  • Option: Play Again hoặc Leave

- updateTimer(remaining_seconds)
  • Update UI countdown

- updateScores(scores)
  • Update player list với điểm mới
```

#### **G. js/chat.js**
```
Các hàm:
- initChat(chatBoxElement, inputElement)
- sendChatMessage(message)
  • Send CHAT_MESSAGE hoặc GUESS_WORD
- onChatReceived(username, message, timestamp)
  • Append message to chat box
  • Auto-scroll
- clearChat()
```

---

## 6. THỨ TỰ TRIỂN KHAI

### Phase 1: Foundation (Tuần 1-2)
```
1. Setup project structure
   - Tạo thư mục theo cấu trúc
   - Viết Makefile cơ bản

2. Database (database.c/h)
   - Tạo schema MySQL
   - Implement db_connect/disconnect
   - Test connection

3. Protocol definitions (common/protocol.h)
   - Define message types
   - Define structs

4. Basic server (server.c/h + main.c)
   - Socket creation, bind, listen
   - Accept connections
   - select() event loop
   - Handle disconnections

Test: Server có thể accept multiple clients, log connections
```

### Phase 2: Authentication (Tuần 2-3)
```
5. Authentication module (auth.c/h)
   - auth_hash_password()
   - auth_verify_password()
   - auth_handle_login()
   - auth_handle_register()

6. Database user functions (database.c/h)
   - db_register_user()
   - db_authenticate_user()
   - db_get_user_stats()

7. Protocol handlers (protocol.c/h)
   - protocol_parse_message()
   - protocol_create_message()
   - protocol_handle_message() - basic switch case
   - Handlers cho LOGIN, REGISTER messages

8. Basic web client (client/index.html + js/network.js)
   - Login/Register forms
   - TCP/WebSocket connection
   - Send LOGIN_REQUEST
   - Handle LOGIN_RESPONSE

Test: Đăng ký tài khoản mới, đăng nhập thành công, kiểm tra database
```

### Phase 3: Room Management (Tuần 3-4)
```
9. Room module (room.c/h)
   - room_create()
   - room_destroy()
   - room_add_player()
   - room_remove_player()
   - room_get_list()

10. Room protocol handlers (protocol.c/h)
    - protocol_send_room_list()
    - Handlers cho CREATE_ROOM, JOIN_ROOM, LEAVE_ROOM
    - ROOM_UPDATE broadcasting

11. Server room management (server.c/h)
    - Thêm rooms array vào server_t
    - server_broadcast_to_room()
    - Handle client disconnect → remove from room

12. Lobby UI (client/lobby.html + js)
    - Hiển thị room list
    - Create/Join room buttons
    - Real-time room updates

Test: Tạo phòng, nhiều clients join cùng phòng, rời phòng
```

### Phase 4: Drawing System (Tuần 4-5)
```
13. Drawing module (drawing.c/h)
    - drawing_parse_action()
    - drawing_serialize_action()
    - drawing_validate_action()

14. Canvas client (client/js/canvas.js)
    - initCanvas()
    - Mouse/touch event handlers
    - drawLine(), clearCanvas()
    - Send DRAW_DATA to server

15. Drawing protocol handlers (protocol.c/h)
    - Handle DRAW_DATA from drawer
    - protocol_broadcast_draw() to other players

16. Game UI (client/game.html)
    - Canvas setup
    - Drawing tools (color picker, brush size)
    - Player list panel
    - Chat panel

Test: 1 client vẽ, clients khác thấy real-time
```

### Phase 5: Game Logic (Tuần 5-6)
```
17. Words system (database.c/h + data/words.txt)
    - Load words vào database
    - db_get_random_word()
    - Categorize by difficulty

18. Game state module (game.c/h)
    - game_init()
    - game_start_round()
    - game_handle_guess()
    - game_check_timeout()
    - game_end_round()
    - game_end()

19. Game protocol handlers (protocol.c/h)
    - protocol_send_game_start()
    - Handle GUESS_WORD
    - Send CORRECT_GUESS, WRONG_GUESS
    - Send ROUND_END, GAME_END

20. Game UI logic (client/js/game.js)
    - onGameStart() - drawer vs guesser UI
    - Timer countdown
    - Handle guess input
    - Display round results
    - Display final scores

Test: Chơi full game từ đầu đến cuối, đoán đúng/sai, timeout
```

### Phase 6: Chat & Scoring (Tuần 6)
```
21. Chat system (protocol.c/h)
    - Handle CHAT_MESSAGE
    - Broadcast CHAT_BROADCAST
    - Filter guess words (không hiển thị trong chat)

22. Scoring system (game.c/h)
    - Tính điểm cho guesser (+10)
    - Tính điểm cho drawer (+5)
    - Update player_score_t
    - Sort leaderboard

23. Database game history (database.c/h)
    - db_save_game_result()
    - db_update_user_stats()
    - db_get_leaderboard()

24. Chat UI (client/js/chat.js)
    - Display messages
    - Send chat/guess
    - Auto-scroll

Test: Chat hoạt động, điểm được tính đúng, lưu vào database
```

### Phase 7: Polish & Features (Tuần 7)
```
25. Hints system (game.c/h)
    - game_send_hint() - reveal letters over time
    - Schedule hints (20s, 40s)

26. Timeouts & Reconnection
    - Client timeout detection (last_activity)
    - Auto-remove inactive clients
    - Graceful disconnect handling

27. Advanced features (optional)
    - Kick player (room owner)
    - Private rooms (password)
    - Custom word lists
    - Difficulty selection
    - Power-ups (skip word, extra time)

28. UI/UX improvements
    - Loading screens
    - Error messages
    - Sound effects
    - Animations

Test: All features working together
```

### Phase 8: Testing & Debugging (Tuần 8)
```
29. Integration testing
    - Multiple concurrent games
    - Edge cases (disconnect during game, etc.)
    - Stress test (nhiều clients)

30. Bug fixes & optimization
    - Memory leaks check (valgrind)
    - Thread safety (nếu dùng threads)
    - Network efficiency

31. Documentation
    - Code comments
    - API documentation
    - User manual
```

---

## 7. TESTING

### 7.1 Unit Tests

#### Database Tests
```bash
# Test database connection
./test_db_connect

# Test user registration
./test_db_register "testuser" "password123" "test@email.com"

# Test authentication
./test_db_auth "testuser" "password123"

# Test word retrieval
./test_db_random_word "medium"
```

#### Protocol Tests
```bash
# Test message parsing
./test_protocol_parse

# Test message creation
./test_protocol_create

# Test serialization/deserialization
./test_protocol_serialize
```

#### Game Logic Tests
```bash
# Test scoring
./test_game_scoring

# Test word matching (case-insensitive)
./test_game_guess "CAT" "cat"  # should return true

# Test round timer
./test_game_timer
```

### 7.2 Integration Tests

#### Test Scenario 1: Basic Game Flow
```
1. Start server
2. Client A: Register & Login
3. Client B: Register & Login
4. Client A: Create room "Test Room"
5. Client B: Join room
6. Client A: Start game
7. Server assigns Client A as drawer, word = "cat"
8. Client A: Draw
9. Client B: See drawing real-time
10. Client B: Guess "dog" → wrong
11. Client B: Guess "cat" → correct
12. Round ends, scores updated
13. Next round starts (Client B is drawer)
```

#### Test Scenario 2: Multiple Rooms
```
1. Start server
2. 6 clients connect
3. Create 2 rooms (3 players each)
4. Both rooms play simultaneously
5. Verify no cross-room data leaks
```

#### Test Scenario 3: Disconnect Handling
```
1. Start game with 3 players
2. Drawer disconnects mid-round
3. Verify: Round ends gracefully, new drawer assigned
4. Guesser disconnects
5. Verify: Game continues with remaining players
```

### 7.3 Load Testing
```bash
# Stress test với nhiều connections
./stress_test 100  # 100 concurrent clients

# Test memory usage
valgrind --leak-check=full ./server

# Test CPU usage
top -p $(pgrep server)
```

### 7.4 Security Tests
```
- SQL Injection: Test với username = "admin'--"
- Buffer overflow: Gửi messages quá dài
- Invalid message types: Gửi random bytes
- Brute force login: Multiple failed attempts
- XSS trong chat messages
```

---

## 8. BIÊN DỊCH VÀ CHẠY

### 8.1 Requirements
```
- GCC 7.0+
- MySQL 8.0+
- MySQL C Connector (libmysqlclient-dev)
- Make
```

### 8.2 Installation
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential
sudo apt-get install libmysqlclient-dev
sudo apt-get install mysql-server

# Clone project
git clone <your-repo>
cd draw-guess

# Setup database
mysql -u root -p < database/schema.sql
mysql -u root -p draw_guess_db < database/sample_data.sql

# Build server
make clean
make all

# Run server
./bin/server -p 8080 -c 50
```

### 8.3 Makefile Example
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -g -I./common -I/usr/include/mysql
LDFLAGS = -lmysqlclient -lm -lpthread

SERVER_SRC = server/main.c server/server.c server/auth.c \
             server/database.c server/room.c server/game.c \
             server/protocol.c server/drawing.c server/utils.c

SERVER_OBJ = $(SERVER_SRC:.c=.o)

all: server

server: $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o bin/server $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f server/*.o bin/server

test: test_db test_protocol

test_db:
	$(CC) $(CFLAGS) tests/test_database.c server/database.c -o bin/test_db $(LDFLAGS)
	./bin/test_db

test_protocol:
	$(CC) $(CFLAGS) tests/test_protocol.c server/protocol.c -o bin/test_protocol $(LDFLAGS)
	./bin/test_protocol
```

---

## 9. TROUBLESHOOTING

### Common Issues

#### 1. MySQL Connection Failed
```bash
# Check MySQL service
sudo systemctl status mysql

# Check credentials
mysql -u root -p

# Grant privileges
GRANT ALL PRIVILEGES ON draw_guess_db.* TO 'your_user'@'localhost';
FLUSH PRIVILEGES;
```

#### 2. Port Already in Use
```bash
# Find process using port
lsof -i :8080

# Kill process
kill -9 <PID>
```

#### 3. Segmentation Fault
```bash
# Debug với gdb
gdb ./bin/server
run -p 8080
bt  # backtrace when crash

# Check memory leaks
valgrind --leak-check=full ./bin/server -p 8080
```

#### 4. Client Cannot Connect
```
- Check firewall: sudo ufw allow 8080
- Check server IP: ifconfig
- Test connection: telnet <server-ip> 8080
```

#### 5. Drawing Lag
```
- Reduce draw data frequency (throttle mouse events)
- Compress drawing data
- Use smaller canvas resolution
- Optimize broadcast function
```

---

## 10. NÂNG CAO (OPTIONAL)

### 10.1 Features Nâng Cao
```
1. Replay system
   - Lưu tất cả draw actions
   - Playback sau khi game kết thúc

2. Achievements & Badges
   - "Fast Guesser": Đoán trong 5 giây
   - "Picasso": Vẽ được nhiều người đoán đúng

3. Custom word packs
   - Users upload word lists
   - Vote/rate word packs

4. Spectator mode
   - Người không chơi có thể xem
   - Không thấy chat, không đoán

5. Voice chat
   - WebRTC integration
   - Mute drawer (avoid cheating)

6. Mobile app
   - Touch drawing support
   - Native Android/iOS client

7. AI integration
   - AI bot player (uses image recognition)
   - Auto-moderate inappropriate drawings
```

### 10.2 Optimization
```
1. Use epoll instead of select (Linux)
   - Better performance với nhiều connections

2. Implement message queue
   - Async message processing
   - Prevent blocking

3. Drawing compression
   - Delta encoding (chỉ gửi changes)
   - Vector format thay vì raster

4. Database indexing
   - Index frequently queried columns
   - Query optimization

5. Caching
   - Cache room list
   - Cache user stats
   - Redis for session management

6. Load balancing
   - Multiple server instances
   - Distribute rooms across servers
```

### 10.3 Security Enhancements
```
1. Password requirements
   - Min length, complexity
   - Rate limiting login attempts

2. Input validation
   - Sanitize all user inputs
   - Prevent injection attacks

3. Encryption
   - TLS/SSL for network communication
   - Hash passwords với bcrypt

4. Anti-cheat
   - Prevent drawer from chatting answer
   - Detect suspicious patterns

5. CAPTCHA
   - Prevent bot accounts
```

---

## 11. ĐÁNH GIÁ & TIÊU CHÍ

### Tiêu chí đánh giá (10 điểm)
```
1. Server architecture (2 điểm)
   - Correct socket programming
   - Proper use of select/poll
   - Handle multiple clients

2. Protocol design (1.5 điểm)
   - Well-defined message format
   - Complete message types
   - Proper serialization

3. Database integration (1.5 điểm)
   - Correct schema design
   - Secure authentication
   - Proper data persistence

4. Game logic (2 điểm)
   - Turn-based flow works correctly
   - Scoring system accurate
   - Timer implementation

5. Real-time drawing (1.5 điểm)
   - Smooth drawing experience
   - Low latency broadcast
   - Drawing validation

6. Client UI/UX (1 điểm)
   - Intuitive interface
   - Responsive design
   - Clear game state

7. Code quality (0.5 điểm)
   - Clean, readable code
   - Proper comments
   - Error handling

Bonus (1 điểm):
   - Advanced features
   - Performance optimization
   - Creative additions
```

---

## 12. TÀI LIỆU THAM KHẢO

### Books & Resources
```
1. "Unix Network Programming" - W. Richard Stevens
   - Chapters 6 (I/O Multiplexing)
   - Chapter 8 (Elementary TCP Sockets)

2. MySQL Documentation
   - C API: https://dev.mysql.com/doc/c-api/8.0/en/

3. Canvas API
   - MDN: https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API

4. WebSocket Protocol
   - RFC 6455
```

### Example Code References
```
- TCP Echo Server với select(): 
  Tham khảo slide "5.IOMultiplexing_slide.pdf"

- Multi-threaded Server:
  Tham khảo slide "4.TCP_Sockets_slide.pdf"

- Protocol Design:
  Tham khảo slide "7.App_protocol_design_slide.pdf"
```

---

## 13. CHECKLIST HOÀN THÀNH

### Server Components
- [ ] Socket server với select()/poll()
- [ ] MySQL database connection
- [ ] User authentication (register/login)
- [ ] Room management (create/join/leave)
- [ ] Game logic (rounds, turns, scoring)
- [ ] Drawing data handling & broadcast
- [ ] Chat system
- [ ] Timeout & disconnect handling
- [ ] Game history persistence

### Client Components
- [ ] Login/Register UI
- [ ] Lobby với room list
- [ ] Game canvas với drawing tools
- [ ] Real-time drawing display
- [ ] Chat interface
- [ ] Guess input handling
- [ ] Score display & leaderboard
- [ ] Timer countdown
- [ ] Round/Game result screens

### Testing
- [ ] Unit tests cho core modules
- [ ] Integration test scenarios
- [ ] Load testing
- [ ] Security testing
- [ ] Bug fixes

### Documentation
- [ ] Code comments
- [ ] README.md
- [ ] API documentation
- [ ] User guide

---

## 14. LIÊN HỆ & HỖ TRỢ

```
Nếu gặp vấn đề trong quá trình implement:

1. Review lại slides bài giảng
2. Check example code trong slides
3. Debug với gdb và valgrind
4. Tham khảo documentation
5. Hỏi giảng viên/trợ giảng

Good luck! 🎨🎮
```

---

## PHỤ LỤC: SAMPLE CODE SNIPPETS

### A. Basic Server Loop với select()
```c
// Tham khảo: slide 5.IOMultiplexing_slide.pdf
// single-process-select-server.c

while (1) {
    read_fds = master_fds;
    
    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
        perror("select");
        break;
    }
    
    // Check listening socket
    if (FD_ISSET(listen_fd, &read_fds)) {
        // Accept new connection
    }
    
    // Check all client sockets
    for (int i = 0; i < max_clients; i++) {
        if (FD_ISSET(client_fds[i], &read_fds)) {
            // Handle client data
        }
    }
}
```

### B. MySQL Query Example
```c
// Tham khảo MySQL C API documentation

MYSQL *conn = mysql_init(NULL);
mysql_real_connect(conn, "localhost", "user", "pass", "draw_guess_db", 0, NULL, 0);

MYSQL_STMT *stmt = mysql_stmt_init(conn);
const char *query = "SELECT user_id FROM users WHERE username=? AND password_hash=?";
mysql_stmt_prepare(stmt, query, strlen(query));

// Bind parameters và execute
// ...
```

### C. Canvas Drawing (JavaScript)
```javascript
// client/js/canvas.js

canvas.addEventListener('mousedown', (e) => {
    isDrawing = true;
    lastX = e.offsetX;
    lastY = e.offsetY;
});

canvas.addEventListener('mousemove', (e) => {
    if (!isDrawing) return;
    
    const x = e.offsetX;
    const y = e.offsetY;
    
    // Draw locally
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    ctx.lineTo(x, y);
    ctx.stroke();
    
    // Send to server
    sendDrawData('line', lastX, lastY, x, y, currentColor, brushSize);
    
    lastX = x;
    lastY = y;
});
```

---

**END OF DOCUMENT**

_Version 1.0 - Created for Network Programming Course_
_Last updated: November 2025_