# Tài Liệu Chi Tiết Logic Xử Lý Game

## Mục Lục
1. [Tổng Quan Flow Game](#tổng-quan-flow-game)
2. [Khởi Tạo Game](#khởi-tạo-game)
3. [Bắt Đầu Game](#bắt-đầu-game)
4. [Xử Lý Round](#xử-lý-round)
5. [Xử Lý Đoán Từ](#xử-lý-đoán-từ)
6. [Kết Thúc Round](#kết-thúc-round)
7. [Kết Thúc Game](#kết-thúc-game)
8. [Xử Lý Người Chơi Rời Phòng](#xử-lý-người-chơi-rời-phòng)
9. [Lưu Kết Quả Vào Database](#lưu-kết-quả-vào-database)
10. [Hiển Thị Leaderboard](#hiển-thị-leaderboard)

---

## Tổng Quan Flow Game

### Flow Chính
```
1. Chủ phòng ấn "Bắt đầu" 
   ↓
2. Server khởi tạo game_state_t
   ↓
3. Bắt đầu round đầu tiên
   ↓
4. Vòng lặp: Round → Đoán từ → Timeout/Đoán đúng hết → Round tiếp theo
   ↓
5. Hết tất cả rounds → Kết thúc game
   ↓
6. Lưu kết quả vào database
   ↓
7. Broadcast GAME_END với leaderboard
   ↓
8. Frontend hiển thị LeaderboardModal
```

---

## Khởi Tạo Game

### 1. Chủ Phòng Ấn "Bắt Đầu"

**Frontend (`GameRoom.jsx`):**
```javascript
const handleStartGame = () => {
  const services = getServices();
  services.startGame();
};
```

**Gateway (`index.js`):**
- Chuyển đổi message `start_game` thành binary protocol `MSG_START_GAME` (0x16)
- Gửi đến TCP server

**Server (`protocol_game.c`):**
```c
int protocol_handle_start_game(server_t* server, int client_index, const message_t* msg)
```

**Điều kiện kiểm tra:**
- Client phải active và đã đăng nhập
- Client phải ở trạng thái `CLIENT_STATE_IN_ROOM`
- Client phải là owner của phòng
- Phòng phải ở trạng thái `ROOM_WAITING`

### 2. Khởi Tạo Game State

**File: `room.c` - `room_start_game()`**

```c
bool room_start_game(room_t *room)
```

**Các bước:**
1. Kiểm tra phòng ở trạng thái `ROOM_WAITING`
2. Kiểm tra có ít nhất 2 người chơi
3. Đánh dấu tất cả người chơi hiện tại là `active_players[i] = 1`
4. Chuyển trạng thái phòng sang `ROOM_PLAYING`
5. Gọi `game_init()` để khởi tạo game state

**File: `game.c` - `game_init()`**

```c
game_state_t* game_init(room_t* room, int total_rounds, int time_limit_seconds)
```

**Các bước khởi tạo:**

1. **Tính tổng số từ cần:**
   - `total_words_needed = room->player_count * total_rounds`
   - Ví dụ: 3 người chơi, 2 rounds → cần 6 từ

2. **Pre-select từ từ database:**
   - Lấy tất cả từ theo `difficulty` (easy/medium/hard)
   - Shuffle 5 lần để đảm bảo ngẫu nhiên
   - Lấy `total_words_needed` từ đầu tiên
   - Lưu vào `game->word_stack[]`

3. **Khởi tạo game state:**
   ```c
   game->room = room;
   game->current_round = 0;
   game->total_rounds = total_words_needed;  // Tổng số round thực tế
   game->time_limit = 30;  // 30 giây mỗi round
   game->drawer_id = -1;
   game->drawer_index = 0;  // Bắt đầu từ owner
   game->current_word[0] = '\0';
   game->word_guessed = false;
   game->game_ended = false;
   ```

4. **Chọn drawer ban đầu:**
   - Tìm index của owner trong `room->players[]`
   - Nếu không tìm thấy, dùng index 0
   - `game->drawer_index = owner_idx >= 0 ? owner_idx : 0`

5. **Khởi tạo scores:**
   - Gọi `ensure_scores_initialized(game)`
   - Tạo score entry cho mỗi người chơi với `score = 0`, `words_guessed = 0`, `rounds_won = 0`

---

## Bắt Đầu Game

### 1. Bắt Đầu Round Đầu Tiên

**File: `protocol_game.c` - `protocol_handle_start_game()`**

Sau khi `room_start_game()` thành công:

```c
// Cập nhật trạng thái tất cả clients trong phòng
for (int i = 0; i < room->player_count; i++) {
    int uid = room->players[i];
    int cidx = find_client_index_by_user(server, uid);
    if (cidx >= 0) {
        server->clients[cidx].state = CLIENT_STATE_IN_GAME;
    }
}

// Bắt đầu round đầu tiên
if (!game_start_round(room->game)) return -1;

// Broadcast GAME_START
broadcast_game_start(server, room);
```

### 2. Bắt Đầu Round

**File: `game.c` - `game_start_round()`**

```c
bool game_start_round(game_state_t* game)
```

**Các bước:**

1. **Kiểm tra game đã kết thúc:**
   ```c
   if (game->game_ended) return false;
   ```

2. **Kiểm tra hết rounds:**
   ```c
   if (game->current_round >= game->total_rounds) {
       game_end(game);
       return false;
   }
   ```

3. **Tăng round counter:**
   ```c
   game->current_round++;
   ```

4. **Chọn drawer:**
   - **Round đầu tiên:** Giữ nguyên `drawer_index` (đã là owner)
   - **Các round sau:** Gọi `pick_next_drawer_index()` để chuyển sang người tiếp theo
   ```c
   if (game->current_round > 1) {
       game->drawer_index = pick_next_drawer_index(game);
   }
   ```
   - `pick_next_drawer_index()`: `(game->drawer_index + 1) % player_count`

5. **Kiểm tra drawer hợp lệ:**
   - Kiểm tra `drawer_index` trong phạm vi
   - Kiểm tra `drawer_id` hợp lệ
   - **Quan trọng:** Kiểm tra drawer có active không
   ```c
   if (game->room->active_players[game->drawer_index] != 1) {
       // Drawer không active → end round ngay
       game_end_round(game, false, -1);
       return false;
   }
   ```

6. **Gán từ mới:**
   ```c
   assign_new_word(game);
   ```
   - Pop từ `word_stack` (FIFO - lấy từ đầu)
   - Gán vào `game->current_word` và `game->current_category`
   - Tăng `word_stack_top++`

7. **Reset round state:**
   ```c
   game->word_guessed = false;
   game->round_start_time = time(NULL);
   game->guessed_count = 0;  // Reset danh sách đã đoán đúng
   ```

### 3. Broadcast GAME_START

**File: `protocol_game.c` - `broadcast_game_start()`**

**Payload structure:**
```
drawer_id(4) + word_length(1) + time_limit(2) + round_start_ms(8) 
+ current_round(4) + player_count(1) + total_rounds(1) 
+ word(64) + category(64) = 149 bytes
```

**Logic gửi:**
- **Drawer:** Nhận `word` đầy đủ
- **Người khác:** Nhận `word` rỗng (chỉ thấy `word_length`)
- **Tất cả:** Nhận `category` (chủ đề)

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleGameStart = (data) => {
  setGameState('playing');
  setCurrentDrawerId(data.drawer_id);
  setIsDrawing(data.drawer_id === myUserId);
  setWord(isDrawing ? data.word : '');  // Chỉ drawer thấy từ
  setCategory(data.category);  // Tất cả thấy category
  setTimeLimit(data.time_limit);
  setRoundStartMs(data.round_start_ms);
  // ...
};
```

### 4. Tạo Database Round

**File: `protocol_game.c` - `protocol_handle_start_game()`**

```c
if (db && room->db_room_id > 0) {
    int drawer_pid = room_player_db_id(room, room->game->drawer_id);
    if (drawer_pid > 0) {
        room->game->db_round_id = db_create_game_round(
            db, 
            room->db_room_id, 
            room->game->current_round, 
            room->game->drawer_index, 
            drawer_pid, 
            room->game->current_word
        );
    }
    // Đánh dấu room đang chơi
    db_execute_query(db, "UPDATE rooms SET status='in_progress' WHERE id = ?", ...);
}
```

---

## Xử Lý Round

### 1. Server Tick - Kiểm Tra Timeout

**File: `server.c` - `server_event_loop()`**

Mỗi giây, server kiểm tra timeout cho tất cả phòng đang chơi:

```c
// Tick: kiem tra timeout cho tat ca phong dang choi
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
```

**File: `game.c` - `game_check_timeout()`**

```c
bool game_check_timeout(game_state_t* game)
{
    if (!game || game->game_ended) return false;
    if (game->round_start_time == 0) return false;

    time_t now = time(NULL);
    if ((int)(now - game->round_start_time) > game->time_limit) {
        // Timeout → end round thất bại
        game_end_round(game, false, -1);
        return true;
    }
    return false;
}
```

### 2. Broadcast Timer Update

**File: `server.c` - `server_event_loop()`**

Mỗi 1 giây, server gửi timer update đến tất cả phòng đang chơi:

```c
if (now - last_timer_update >= 1) {
    for (int r = 0; r < MAX_ROOMS; r++) {
        room_t* room = server->rooms[r];
        if (room && room->state == ROOM_PLAYING && room->game) {
            protocol_broadcast_timer_update(server, room);
        }
    }
    last_timer_update = now;
}
```

**File: `protocol_game.c` - `protocol_broadcast_timer_update()`**

```c
int protocol_broadcast_timer_update(server_t* server, room_t* room)
{
    game_state_t* game = room->game;
    time_t now = time(NULL);
    int elapsed = (int)(now - game->round_start_time);
    int time_left = game->time_limit - elapsed;
    if (time_left < 0) time_left = 0;
    
    uint8_t payload[2];
    write_u16_be(payload, (uint16_t)time_left);
    
    return server_broadcast_to_room(server, room->room_id, MSG_TIMER_UPDATE, payload, 2, -1);
}
```

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleTimerUpdate = (data) => {
  setServerTimeLeft(data.time_left);  // Thời gian từ server (authoritative)
};

// Hybrid timer: Server authority + Client smoothing
useEffect(() => {
  const tick = () => {
    if (serverTimeLeft !== null) {
      // Ưu tiên server time, làm mượt với client
      const smoothed = Math.max(prev - 0.25, serverTimeLeft);
      setDisplayTimeLeft(smoothed);
    }
  };
  timerRef.current = setInterval(tick, 250);
}, [serverTimeLeft]);
```

---

## Xử Lý Đoán Từ

### 1. Người Chơi Gửi Đoán

**Frontend (`GameRoom.jsx`):**
```javascript
const handleSendMessage = (text) => {
  const services = getServices();
  services.sendChatMessage(text);  // Server tự động xử lý như guess nếu đang chơi
};
```

**Gateway (`index.js`):**
- Chuyển đổi `chat_message` thành `MSG_CHAT_MESSAGE` (0x30)
- Server xử lý: Nếu đang chơi và không phải drawer → tự động xử lý như guess

**Server (`protocol_chat.c` hoặc `protocol_game.c`):**
- Parse message
- Nếu `client->state == CLIENT_STATE_IN_GAME` và không phải drawer → gọi `protocol_process_guess()`

### 2. Xử Lý Đoán

**File: `protocol_game.c` - `protocol_process_guess()`**

```c
int protocol_process_guess(server_t* server, int client_index, room_t* room, const char* guess)
```

**Các bước:**

1. **Giữ lại word trước khi xử lý:**
   ```c
   char current_word[MAX_WORD_LEN];
   strncpy(current_word, room->game->current_word, MAX_WORD_LEN - 1);
   ```

2. **Gọi `game_handle_guess()`:**
   ```c
   const bool correct = game_handle_guess(room->game, client->user_id, guess);
   ```

3. **Lưu guess vào database:**
   ```c
   if (db && room->game->db_round_id > 0) {
       int pid = room_player_db_id(room, client->user_id);
       db_save_guess(db, room->game->db_round_id, pid, guess, correct ? 1 : 0);
   }
   ```

4. **Nếu đoán sai:** Return -1 (không broadcast, chỉ hiển thị chat message)

5. **Nếu đoán đúng:**
   - Tính điểm theo thứ tự
   - Broadcast `MSG_CORRECT_GUESS`
   - Lưu score details vào database
   - Kiểm tra tất cả đã đoán đúng chưa

### 3. Xử Lý Đoán Đúng

**File: `game.c` - `game_handle_guess()`**

```c
bool game_handle_guess(game_state_t* game, int guesser_user_id, const char* guess_word)
```

**Các bước:**

1. **Kiểm tra điều kiện:**
   - Game hợp lệ và chưa kết thúc
   - Guess word không rỗng
   - Drawer không được đoán
   - Word chưa được đoán đúng (hoặc đã đoán nhưng chưa đủ người)

2. **So sánh từ (case-insensitive):**
   ```c
   if (!words_equal_case_insensitive(guess_word, game->current_word)) {
       return false;  // Đoán sai
   }
   ```

3. **Kiểm tra đã đoán đúng chưa:**
   ```c
   bool already_guessed = false;
   for (int i = 0; i < game->guessed_count; i++) {
       if (game->guessed_user_ids[i] == guesser_user_id) {
           already_guessed = true;
           break;
       }
   }
   if (already_guessed) {
       return true;  // Đã đoán đúng trước đó, không cộng điểm nữa
   }
   ```

4. **Tính điểm theo thứ tự:**
   ```c
   int guesser_order = game->guessed_count + 1;  // 1, 2, 3, 4+
   int guesser_points, drawer_points;
   
   if (guesser_order == 1) {
       guesser_points = 10;
       drawer_points = 5;
   } else if (guesser_order == 2) {
       guesser_points = 7;
       drawer_points = 4;
   } else if (guesser_order == 3) {
       guesser_points = 5;
       drawer_points = 3;
   } else {
       guesser_points = 3;
       drawer_points = 2;
   }
   ```

5. **Cộng điểm:**
   ```c
   // Cộng điểm cho người đoán đúng
   int gidx = find_score_index(game, guesser_user_id);
   game->scores[gidx].score += guesser_points;
   game->scores[gidx].words_guessed += 1;
   if (guesser_order == 1) {
       game->scores[gidx].rounds_won += 1;  // Chỉ người đầu tiên
   }
   
   // Cộng điểm cho người vẽ (mỗi lần có người đoán đúng)
   int didx = find_score_index(game, game->drawer_id);
   game->scores[didx].score += drawer_points;
   ```

6. **Thêm vào danh sách đã đoán đúng:**
   ```c
   game->guessed_user_ids[game->guessed_count] = guesser_user_id;
   game->guessed_count++;
   game->word_guessed = true;
   ```

7. **KHÔNG kết thúc round ngay:** Để những người khác vẫn có thể đoán cho đến hết giờ

### 4. Broadcast CORRECT_GUESS

**File: `protocol_game.c` - `protocol_process_guess()`**

```c
// Payload: player_id(4) + word(64) + guesser_points(2) + drawer_points(2) + username(32) = 104 bytes
uint8_t cp[104];
write_i32_be(cp + 0, (int32_t)client->user_id);
write_fixed_string(cp + 4, 64, current_word);
write_u16_be(cp + 68, (uint16_t)guesser_points);
write_u16_be(cp + 70, (uint16_t)drawer_points);
write_fixed_string(cp + 72, 32, client->username);

server_broadcast_to_room(server, room->room_id, MSG_CORRECT_GUESS, cp, 104, -1);
```

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleCorrectGuess = (data) => {
  // Cập nhật điểm cho người đoán đúng và người vẽ
  setPlayers((prev) => {
    const updated = prev.map(p => {
      if (p.id === data.player_id) {
        return { ...p, score: (p.score || 0) + data.guesser_points };
      } else if (p.isDrawing) {
        return { ...p, score: (p.score || 0) + data.drawer_points };
      }
      return p;
    });
    updated.sort((a, b) => (b.score || 0) - (a.score || 0));
    return updated;
  });
  
  // Hiển thị thông báo
  setChatMessages((prev) => [
    ...prev,
    { type: 'system', text: `${data.username} đã đoán đúng! (+${data.guesser_points} điểm)` }
  ]);
};
```

### 5. Kiểm Tra Tất Cả Đã Đoán Đúng

**File: `protocol_game.c` - `protocol_process_guess()`**

Sau khi broadcast CORRECT_GUESS:

```c
// Đếm số người chơi active (trừ drawer)
int total_active_guessers = 0;
int drawer_index = -1;

// Tìm drawer index
for (int i = 0; i < room->player_count; i++) {
    if (room->players[i] == room->game->drawer_id) {
        drawer_index = i;
        break;
    }
}

// Đếm số người chơi active (không tính drawer)
for (int i = 0; i < room->player_count; i++) {
    if (room->active_players[i] == 1 && i != drawer_index) {
        total_active_guessers++;
    }
}

int guessed_count = room->game->guessed_count;

// Nếu tất cả đã đoán đúng → kết thúc round sớm
if (total_active_guessers > 0 && guessed_count >= total_active_guessers) {
    game_end_round(room->game, true, -1);
    protocol_handle_round_timeout(server, room, word_before_clear);
    return 0;
}
```

---

## Kết Thúc Round

### 1. Kết Thúc Round

**File: `game.c` - `game_end_round()`**

```c
void game_end_round(game_state_t* game, bool success, int winner_user_id)
{
    printf("[GAME] Room %d end round %d: %s, word='%s'\n",
           game->room->room_id, game->current_round, 
           success ? "SUCCESS" : "TIMEOUT", game->current_word);

    // Reset word/timer cho round hiện tại
    game->current_word[0] = '\0';
    game->current_category[0] = '\0';
    game->word_length = 0;
    game->round_start_time = 0;

    // Kiểm tra hết game?
    if (game->current_round >= game->total_rounds) {
        game_end(game);
        return;
    }

    // Round tiếp theo sẽ do caller gọi (protocol_handle_round_timeout)
}
```

### 2. Broadcast ROUND_END

**File: `protocol_game.c` - `broadcast_round_end()`**

```c
int broadcast_round_end(server_t* server, room_t* room, const char* word)
{
    game_state_t* game = room->game;
    
    // Payload: word(64) + score_count(2) + pairs(user_id(4), score(4))...
    const uint16_t score_count = (uint16_t)game->score_count;
    const size_t payload_size = 64 + 2 + (size_t)score_count * 8;
    
    uint8_t payload[BUFFER_SIZE];
    write_fixed_string(payload + 0, 64, word ? word : "");
    write_u16_be(payload + 64, score_count);
    
    uint8_t* p = payload + 66;
    for (int i = 0; i < game->score_count; i++) {
        write_i32_be(p, (int32_t)game->scores[i].user_id);
        p += 4;
        write_i32_be(p, (int32_t)game->scores[i].score);
        p += 4;
    }
    
    return server_broadcast_to_room(server, room->room_id, MSG_ROUND_END, payload, payload_size, -1);
}
```

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleRoundEnd = (data) => {
  // Cập nhật scores
  if (Array.isArray(data.scores)) {
    applyScores(data.scores);
  }
  
  // Hiển thị từ đúng
  setChatMessages((prev) => [
    ...prev,
    { type: 'system', text: `Hết round! Từ đúng là: "${data.word}"` }
  ]);
  
  // Reset UI
  setIsDrawing(false);
  setCurrentDrawerId(null);
  setWord('');
  setCategory('');
};
```

### 3. Bắt Đầu Round Tiếp Theo

**File: `protocol_game.c` - `protocol_handle_round_timeout()`**

```c
int protocol_handle_round_timeout(server_t* server, room_t* room, const char* word_before_clear)
{
    // Broadcast round_end nếu word vẫn còn
    if (word_before_clear && word_before_clear[0] != '\0') {
        broadcast_round_end(server, room, word_before_clear);
    }

    // Kiểm tra game đã kết thúc?
    if (room->game->game_ended) {
        protocol_broadcast_game_end(server, room);
        room_end_game(room);
        return 0;
    }

    // Thử bắt đầu round mới
    // Nếu drawer không active, game_start_round sẽ end round ngay và trả về false
    // Trong trường hợp đó, cần tiếp tục thử bắt đầu round tiếp theo
    int max_attempts = room->player_count + 1;
    int attempts = 0;
    
    while (attempts < max_attempts && !room->game->game_ended) {
        if (game_start_round(room->game)) {
            // Round bắt đầu thành công với drawer active
            broadcast_game_start(server, room);
            return 0;
        }
        
        // Round không bắt đầu được (có thể do drawer không active)
        if (room->game->game_ended) {
            protocol_broadcast_game_end(server, room);
            room_end_game(room);
            return 0;
        }
        
        // Thử chuyển drawer và bắt đầu lại
        attempts++;
    }
    
    // Nếu không thể bắt đầu round nào → kết thúc game
    if (room->game && !room->game->game_ended) {
        game_end(room->game);
        protocol_broadcast_game_end(server, room);
        room_end_game(room);
    }
    
    return 0;
}
```

**Logic xử lý drawer không active:**
- Nếu drawer không active, `game_start_round()` sẽ gọi `game_end_round()` ngay và trả về `false`
- Round vẫn được đếm (đảm bảo số lượng round đúng)
- Server tiếp tục thử bắt đầu round tiếp theo với drawer mới
- Nếu tất cả drawer đều không active → kết thúc game

---

## Kết Thúc Game

### 1. Kết Thúc Game

**File: `game.c` - `game_end()`**

```c
void game_end(game_state_t* game)
{
    if (!game || game->game_ended) return;
    
    game->game_ended = true;
    int winner = compute_winner_user_id(game);
    
    printf("[GAME] Room %d game ended. Winner user_id=%d\n",
           game->room ? game->room->room_id : -1, winner);
}
```

**File: `game.c` - `compute_winner_user_id()`**

```c
static int compute_winner_user_id(game_state_t* game)
{
    if (!game || game->score_count <= 0) return -1;
    
    int best_uid = game->scores[0].user_id;
    int best_score = game->scores[0].score;
    
    for (int i = 1; i < game->score_count; i++) {
        if (game->scores[i].score > best_score) {
            best_score = game->scores[i].score;
            best_uid = game->scores[i].user_id;
        }
    }
    
    return best_uid;
}
```

### 2. Broadcast GAME_END

**File: `protocol_game.c` - `protocol_broadcast_game_end()`**

```c
int protocol_broadcast_game_end(server_t* server, room_t* room)
{
    game_state_t* game = room->game;
    
    // Tính winner
    int winner_id = -1;
    int best = -2147483647;
    for (int i = 0; i < game->score_count; i++) {
        if (game->scores[i].score > best) {
            best = game->scores[i].score;
            winner_id = game->scores[i].user_id;
        }
    }
    
    // Lưu lịch sử chơi cho tất cả người chơi
    if (db && game->score_count > 0) {
        // Sắp xếp scores theo điểm giảm dần (để tính rank)
        player_score_t sorted_scores[MAX_PLAYERS_PER_ROOM];
        for (int i = 0; i < game->score_count; i++) {
            sorted_scores[i].user_id = game->scores[i].user_id;
            sorted_scores[i].score = game->scores[i].score;
        }
        
        // Bubble sort giảm dần
        for (int i = 0; i < game->score_count - 1; i++) {
            for (int j = 0; j < game->score_count - i - 1; j++) {
                if (sorted_scores[j].score < sorted_scores[j + 1].score) {
                    player_score_t temp = sorted_scores[j];
                    sorted_scores[j] = sorted_scores[j + 1];
                    sorted_scores[j + 1] = temp;
                }
            }
        }
        
        // Lưu lịch sử với rank
        for (int i = 0; i < game->score_count; i++) {
            int rank = i + 1;  // rank từ 1, 2, 3, ...
            db_save_game_history(db, sorted_scores[i].user_id, sorted_scores[i].score, rank);
        }
    }
    
    // Payload: winner_id(4) + score_count(2) + pairs(user_id(4), score(4))...
    const uint16_t score_count = (uint16_t)game->score_count;
    const size_t payload_size = 4 + 2 + (size_t)score_count * 8;
    
    uint8_t payload[BUFFER_SIZE];
    write_i32_be(payload + 0, (int32_t)winner_id);
    write_u16_be(payload + 4, score_count);
    
    uint8_t* p = payload + 6;
    for (int i = 0; i < game->score_count; i++) {
        write_i32_be(p, (int32_t)game->scores[i].user_id);
        p += 4;
        write_i32_be(p, (int32_t)game->scores[i].score);
        p += 4;
    }
    
    return server_broadcast_to_room(server, room->room_id, MSG_GAME_END, payload, payload_size, -1);
}
```

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleGameEnd = (data) => {
  // Cập nhật scores và lưu leaderboard data riêng
  if (Array.isArray(data.scores)) {
    setPlayers(prevPlayers => {
      const map = new Map(data.scores.map(s => [s.user_id, s.score]));
      const leaderboardData = prevPlayers.map(p => ({
        ...p,
        score: map.get(p.id) !== undefined ? map.get(p.id) : (p.score ?? 0),
        isDrawing: false
      }));
      
      // Sắp xếp theo điểm giảm dần
      leaderboardData.sort((a, b) => (b.score || 0) - (a.score || 0));
      
      // Lưu vào leaderboardPlayers để hiển thị trong modal
      setLeaderboardPlayers(leaderboardData);
      return leaderboardData;
    });
  }
  
  setGameState('finished');
  setShowLeaderboard(true);  // Hiển thị modal leaderboard
};
```

### 3. End Game và Cleanup

**File: `room.c` - `room_end_game()`**

```c
void room_end_game(room_t *room)
{
    printf("Game trong phong '%s' (ID: %d) da ket thuc\n",
           room->room_name, room->room_id);

    // Free game state
    if (room->game) {
        game_destroy(room->game);
        room->game = NULL;
    }

    // Chuyển trạng thái phòng sang FINISHED
    room->state = ROOM_FINISHED;
}
```

---

## Xử Lý Người Chơi Rời Phòng

### 1. Người Chơi Rời Phòng (Explicit Leave)

**Frontend (`GameRoom.jsx`):**
```javascript
const handleLeaveRoom = () => {
  const services = getServices();
  services.leaveRoom(parseInt(roomId, 10));
};
```

**Server (`protocol_room.c` - `protocol_handle_leave_room()`):**

**Các bước xử lý:**

1. **Lưu thông tin trước khi xóa:**
   ```c
   int was_playing = (room->state == ROOM_PLAYING && room->game != NULL);
   int was_drawer = (was_playing && room->game->drawer_id == client->user_id);
   char word_before[64];
   if (was_playing) {
       strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
   }
   ```

2. **Xóa người chơi khỏi phòng:**
   ```c
   room_remove_player(room, client->user_id);
   ```

3. **Kiểm tra số người chơi active:**
   ```c
   int active_count = 0;
   for (int i = 0; i < room->player_count; i++) {
       if (room->active_players[i] == 1) {
           active_count++;
       }
   }
   ```

4. **Nếu đang chơi và active < 2 → kết thúc game:**
   ```c
   bool should_end_game = (was_playing_before && room->game != NULL && active_count < 2);
   
   if (should_end_game && room->game) {
       room->game->game_ended = true;
       protocol_broadcast_game_end(server, room);
       room_end_game(room);
   }
   ```

5. **Nếu drawer rời phòng → end round và chuyển round tiếp theo:**
   ```c
   if (was_drawer && room->game && word_before[0] != '\0' && !should_end_game) {
       game_end_round(room->game, false, -1);
       protocol_handle_round_timeout(server, room, word_before);
   }
   ```

### 2. Xóa Người Chơi Khỏi Phòng

**File: `room.c` - `room_remove_player()`**

**Logic xử lý:**

1. **Nếu đang chơi game:**
   ```c
   if (room->state == ROOM_PLAYING) {
       // KHÔNG xóa người chơi khỏi mảng
       // Chỉ đánh dấu inactive (-1 = đã rời phòng)
       room->active_players[player_index] = -1;
       
       // Nếu owner rời phòng, chuyển owner cho người chơi active đầu tiên
       if (user_id == room->owner_id) {
           // Tìm người chơi active đầu tiên
           for (int i = 0; i < room->player_count; i++) {
               if (room->active_players[i] == 1) {
                   room->owner_id = room->players[i];
                   break;
               }
           }
       }
       
       // Đếm số người chơi active
       int active_count = 0;
       for (int i = 0; i < room->player_count; i++) {
           if (room->active_players[i] == 1) {
               active_count++;
           }
       }
       
       // Nếu < 2 người active → cần end game (sẽ xử lý ở nơi gọi)
       if (active_count < 2) {
           printf("Game can ket thuc do khong du nguoi choi active (active: %d)\n", active_count);
       }
       
       return true;
   }
   ```

2. **Nếu không phải đang chơi:**
   ```c
   // Xóa player bằng cách shift array
   for (int i = player_index; i < room->player_count - 1; i++) {
       room->players[i] = room->players[i + 1];
       room->active_players[i] = room->active_players[i + 1];
       room->db_player_ids[i] = room->db_player_ids[i + 1];
   }
   room->player_count--;
   
   // Nếu owner rời phòng, chuyển owner cho người chơi active đầu tiên
   if (user_id == room->owner_id && room->player_count > 0) {
       for (int i = 0; i < room->player_count; i++) {
           if (room->active_players[i] == 1) {
               room->owner_id = room->players[i];
               break;
           }
       }
   }
   ```

### 3. Broadcast ROOM_PLAYERS_UPDATE

**File: `protocol_room.c` - `protocol_broadcast_room_players_update()`**

Khi có người rời phòng, server broadcast danh sách người chơi cập nhật:

```c
// Xác định trạng thái active
if (room->active_players[i] == -1) {
    players[i].is_active = 255;  // 0xFF = đã rời phòng
} else if (room->active_players[i] == 1) {
    players[i].is_active = 1;  // Active
} else {
    players[i].is_active = 0;  // Đang chờ (join giữa chừng)
}
```

**Frontend nhận (`GameRoom.jsx`):**
```javascript
const handleRoomPlayersUpdate = (data) => {
  // Nếu game đã kết thúc, không cập nhật players state để tránh thay đổi leaderboard
  if (gameState === 'finished') {
    return;
  }
  
  setPlayers((prev) => {
    const mapped = data.players.map(p => ({
      id: p.user_id,
      username: p.username,
      avatar: p.avatar,
      score: existingPlayer ? existingPlayer.score : 0,  // Giữ lại score
      isDrawing: p.user_id === currentDrawerId,
      isOwner: p.is_owner === 1,
      isActive: p.is_active !== undefined ? p.is_active : 1,
      hasLeft: p.is_active === 255  // Đã rời phòng
    }));
    return mapped;
  });
};
```

### 4. Disconnect (Mất Kết Nối)

**File: `server.c` - `server_handle_disconnect()`**

Xử lý tương tự như explicit leave, nhưng không có response message:

```c
void server_handle_disconnect(server_t *server, int client_index)
{
    client_t* client = &server->clients[client_index];
    
    // Nếu client đang trong phòng
    if (client->user_id > 0 && 
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {
        
        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            // Xử lý tương tự protocol_handle_leave_room
            // - Lưu thông tin drawer
            // - Xóa player (đánh dấu inactive nếu đang chơi)
            // - Kiểm tra active_count < 2 → end game
            // - Nếu drawer rời → end round và chuyển round tiếp theo
            // - Broadcast ROOM_PLAYERS_UPDATE
        }
    }
    
    server_remove_client(server, client_index);
}
```

### 5. Logout

**File: `protocol_auth.c` - `protocol_handle_logout()`**

Xử lý tương tự như disconnect, nhưng client vẫn giữ kết nối:

```c
int protocol_handle_logout(server_t* server, int client_index, const message_t* msg)
{
    // Nếu đang trong phòng → leave room
    if (client->user_id > 0 &&
        (client->state == CLIENT_STATE_IN_ROOM || client->state == CLIENT_STATE_IN_GAME)) {
        
        room_t* room = server_find_room_by_user(server, client->user_id);
        if (room) {
            // Xử lý tương tự protocol_handle_leave_room
        }
    }
    
    // Reset client state về LOGGED_OUT
    client->state = CLIENT_STATE_LOGGED_OUT;
    client->user_id = -1;
}
```

### 6. Xử Lý Drawer Rời Phòng

**Khi drawer rời phòng trong lúc đang chơi:**

1. **Lưu word trước khi xóa:**
   ```c
   char word_before[64];
   strncpy(word_before, room->game->current_word, sizeof(word_before) - 1);
   ```

2. **End round hiện tại:**
   ```c
   game_end_round(room->game, false, -1);
   ```
   - Round được đếm (đảm bảo số lượng round đúng)
   - Word bị clear

3. **Bắt đầu round tiếp theo:**
   ```c
   protocol_handle_round_timeout(server, room, word_before);
   ```
   - Broadcast ROUND_END với word đã lưu
   - Thử bắt đầu round tiếp theo
   - Nếu drawer mới không active → tiếp tục thử drawer tiếp theo
   - Nếu tất cả drawer đều không active → kết thúc game

### 7. Xử Lý Khi Active < 2

**Khi số người chơi active < 2:**

1. **Đánh dấu game đã kết thúc:**
   ```c
   room->game->game_ended = true;
   ```

2. **Broadcast GAME_END:**
   ```c
   protocol_broadcast_game_end(server, room);
   ```
   - Tính winner
   - Lưu lịch sử vào database
   - Broadcast với scores

3. **End game và cleanup:**
   ```c
   room_end_game(room);
   ```
   - Free game state
   - Chuyển room state sang FINISHED

4. **Frontend hiển thị leaderboard:**
   ```javascript
   const handleGameEnd = (data) => {
     setLeaderboardPlayers(leaderboardData);  // Lưu riêng để không bị thay đổi
     setShowLeaderboard(true);
   };
   ```

---

## Lưu Kết Quả Vào Database

### 1. Lưu Game Round

**Khi bắt đầu round (`protocol_game.c`):**
```c
if (db && room->db_room_id > 0) {
    int drawer_pid = room_player_db_id(room, room->game->drawer_id);
    if (drawer_pid > 0) {
        room->game->db_round_id = db_create_game_round(
            db, 
            room->db_room_id, 
            room->game->current_round, 
            room->game->drawer_index, 
            drawer_pid, 
            room->game->current_word
        );
    }
}
```

### 2. Lưu Guess

**Khi người chơi đoán (`protocol_game.c`):**
```c
if (db && room->game->db_round_id > 0 && room->db_room_id > 0) {
    int pid = room_player_db_id(room, client->user_id);
    if (pid > 0) {
        db_save_guess(db, room->game->db_round_id, pid, guess, correct ? 1 : 0);
    }
}
```

### 3. Lưu Score Details

**Khi có người đoán đúng (`protocol_game.c`):**
```c
if (db && room->game->db_round_id > 0 && room->db_room_id > 0) {
    int guesser_pid = room_player_db_id(room, client->user_id);
    int drawer_pid = room_player_db_id(room, room->game->drawer_id);
    
    if (guesser_pid > 0) {
        db_save_score_detail(db, room->game->db_round_id, guesser_pid, guesser_points);
    }
    if (drawer_pid > 0) {
        db_save_score_detail(db, room->game->db_round_id, drawer_pid, drawer_points);
    }
}
```

### 4. Lưu Game History

**Khi kết thúc game (`protocol_game.c`):**
```c
if (db && game->score_count > 0) {
    // Sắp xếp scores theo điểm giảm dần
    player_score_t sorted_scores[MAX_PLAYERS_PER_ROOM];
    for (int i = 0; i < game->score_count; i++) {
        sorted_scores[i].user_id = game->scores[i].user_id;
        sorted_scores[i].score = game->scores[i].score;
    }
    
    // Bubble sort giảm dần
    for (int i = 0; i < game->score_count - 1; i++) {
        for (int j = 0; j < game->score_count - i - 1; j++) {
            if (sorted_scores[j].score < sorted_scores[j + 1].score) {
                player_score_t temp = sorted_scores[j];
                sorted_scores[j] = sorted_scores[j + 1];
                sorted_scores[j + 1] = temp;
            }
        }
    }
    
    // Lưu lịch sử với rank
    for (int i = 0; i < game->score_count; i++) {
        int rank = i + 1;  // rank từ 1, 2, 3, ...
        db_save_game_history(db, sorted_scores[i].user_id, sorted_scores[i].score, rank);
    }
}
```

**Database schema:**
- `game_history`: `user_id`, `score`, `rank`, `finished_at`
- `game_rounds`: `room_id`, `round_number`, `drawer_id`, `word`
- `guesses`: `round_id`, `player_id`, `guess`, `is_correct`
- `score_details`: `round_id`, `player_id`, `points`

---

## Hiển Thị Leaderboard

### 1. Frontend Nhận GAME_END

**File: `GameRoom.jsx` - `handleGameEnd()`**

```javascript
const handleGameEnd = (data) => {
  // Cập nhật scores vào players state và lưu leaderboard data riêng
  if (Array.isArray(data.scores)) {
    setPlayers(prevPlayers => {
      const map = new Map(data.scores.map(s => [s.user_id, s.score]));
      const leaderboardData = prevPlayers.map(p => {
        const newScore = map.get(p.id);
        return {
          ...p,
          score: newScore !== undefined ? newScore : (p.score ?? 0),
          isDrawing: false
        };
      });
      
      // Sắp xếp theo điểm giảm dần
      leaderboardData.sort((a, b) => (b.score || 0) - (a.score || 0));
      
      // Lưu vào leaderboardPlayers để hiển thị trong modal
      // (không bị thay đổi khi có người rời phòng)
      setLeaderboardPlayers(leaderboardData);
      return leaderboardData;
    });
  }
  
  setGameState('finished');
  setShowLeaderboard(true);  // Hiển thị modal
};
```

### 2. Bảo Vệ Leaderboard Không Bị Thay Đổi

**File: `GameRoom.jsx` - `handleRoomPlayersUpdate()`**

```javascript
const handleRoomPlayersUpdate = (data) => {
  // Nếu game đã kết thúc, không cập nhật players state để tránh thay đổi leaderboard
  if (gameState === 'finished') {
    console.log('Game đã kết thúc, bỏ qua room_players_update để giữ nguyên leaderboard');
    return;
  }
  
  // ... cập nhật players state
};
```

### 3. Hiển Thị LeaderboardModal

**File: `GameRoom.jsx`**

```javascript
<LeaderboardModal 
  players={leaderboardPlayers.length > 0 ? leaderboardPlayers : players} 
  onClose={handleCloseLeaderboard} 
  show={showLeaderboard}
/>

const handleCloseLeaderboard = () => {
  setShowLeaderboard(false);
  // Rời phòng và về lobby
  handleLeaveRoom();
};
```

### 4. LeaderboardModal Component

**File: `components/LeaderboardModal.jsx`**

- Hiển thị danh sách người chơi sắp xếp theo điểm giảm dần
- Hiển thị rank (1, 2, 3, ...)
- Hiển thị điểm số
- Hiển thị avatar và username
- Nút "Đóng" để rời phòng và về lobby

---

## Tóm Tắt Flow Hoàn Chỉnh

### Flow Bình Thường (Không Có Người Rời Phòng)

```
1. Chủ phòng ấn "Bắt đầu"
   ↓
2. room_start_game() → game_init()
   - Pre-select từ từ database
   - Khởi tạo scores
   - Chọn drawer (owner)
   ↓
3. game_start_round() → Round 1
   - Pop từ từ word_stack
   - Set drawer_id, current_word, round_start_time
   ↓
4. Broadcast GAME_START
   - Drawer nhận word đầy đủ
   - Người khác nhận word_length và category
   ↓
5. Vòng lặp:
   - Server tick mỗi giây → kiểm tra timeout
   - Broadcast TIMER_UPDATE mỗi giây
   - Người chơi đoán → game_handle_guess()
   - Nếu đúng → broadcast CORRECT_GUESS, cộng điểm
   - Nếu tất cả đoán đúng → end round sớm
   - Nếu timeout → end round
   ↓
6. game_end_round()
   - Reset word, timer
   - Kiểm tra hết rounds?
   ↓
7. protocol_handle_round_timeout()
   - Broadcast ROUND_END với scores
   - game_start_round() → Round tiếp theo
   - Lặp lại từ bước 4
   ↓
8. Hết tất cả rounds → game_end()
   ↓
9. protocol_broadcast_game_end()
   - Tính winner
   - Lưu lịch sử vào database
   - Broadcast GAME_END với scores
   ↓
10. Frontend hiển thị LeaderboardModal
```

### Flow Khi Có Người Rời Phòng

```
1. Người chơi rời phòng (explicit leave / disconnect / logout)
   ↓
2. room_remove_player()
   - Nếu đang chơi: đánh dấu active_players[i] = -1 (KHÔNG xóa khỏi mảng)
   - Nếu không chơi: xóa khỏi mảng
   ↓
3. Kiểm tra active_count
   - Nếu < 2 → should_end_game = true
   ↓
4. Nếu should_end_game:
   - game->game_ended = true
   - protocol_broadcast_game_end()
   - room_end_game()
   ↓
5. Nếu drawer rời phòng:
   - Lưu word_before
   - game_end_round() → end round hiện tại
   - protocol_handle_round_timeout() → bắt đầu round tiếp theo
   - Nếu drawer mới không active → tiếp tục thử drawer tiếp theo
   ↓
6. Broadcast ROOM_PLAYERS_UPDATE
   - is_active = 255 nếu đã rời phòng
   - is_active = 1 nếu active
   - is_active = 0 nếu đang chờ
   ↓
7. Frontend cập nhật danh sách người chơi
   - Nếu game đã kết thúc → không cập nhật (giữ nguyên leaderboard)
```

### Các Trường Hợp Đặc Biệt

1. **Drawer không active khi bắt đầu round:**
   - `game_start_round()` kiểm tra → end round ngay
   - Round vẫn được đếm
   - Thử drawer tiếp theo

2. **Tất cả drawer đều không active:**
   - Không thể bắt đầu round nào
   - Kết thúc game

3. **Người chơi join giữa chừng:**
   - `active_players[i] = 0` (đang chờ)
   - Sẽ active vào round tiếp theo

4. **Owner rời phòng:**
   - Chuyển owner cho người chơi active đầu tiên
   - Nếu không có người active → phòng sẽ bị xóa khi game kết thúc

5. **Game kết thúc do thiếu người chơi:**
   - `active_count < 2` → end game ngay
   - Lưu kết quả hiện tại
   - Broadcast GAME_END

---

## Kết Luận

Tài liệu này mô tả chi tiết toàn bộ logic xử lý game từ khi chủ phòng ấn "Bắt đầu" đến khi hiển thị leaderboard và lưu kết quả. Các trường hợp người chơi rời phòng giữa chừng đã được xử lý đầy đủ để đảm bảo game vẫn chạy ổn định và không bị lỗi.

