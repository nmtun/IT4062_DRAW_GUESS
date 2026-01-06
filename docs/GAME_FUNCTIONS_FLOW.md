# Tổng Hợp Flow Các Chức Năng Game

Tài liệu này mô tả chi tiết các hàm được gọi theo thứ tự cho mỗi chức năng liên quan đến game.

---

## Mục Lục

1. [Bắt Đầu Game](#1-bắt-đầu-game)
2. [Xử Lý Đoán Từ](#2-xử-lý-đoán-từ)
3. [Xử Lý Vẽ (Drawing)](#3-xử-lý-vẽ-drawing)
4. [Kiểm Tra Timeout Round](#4-kiểm-tra-timeout-round)
5. [Cập Nhật Timer](#5-cập-nhật-timer)
6. [Kết Thúc Round](#6-kết-thúc-round)
7. [Kết Thúc Game](#7-kết-thúc-game)
8. [Người Chơi Rời Phòng Trong Game](#8-người-chơi-rời-phòng-trong-game)
9. [Người Chơi Disconnect Trong Game](#9-người-chơi-disconnect-trong-game)

---

## 1. Bắt Đầu Game

### Mô tả
Khi chủ phòng ấn nút "Bắt đầu", server sẽ khởi tạo game state và bắt đầu round đầu tiên.

### Flow các hàm được gọi:

```
1. Frontend: handleStartGame()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Gọi services.startGame()

2. Gateway: startGame()
   File: src/gateway/index.js
   → Chuyển đổi message thành MSG_START_GAME (0x16)
   → Gửi đến TCP server

3. Server: protocol_handle_start_game()
   File: src/server/protocol_game.c (dòng 217)
   → Kiểm tra client active và đã đăng nhập
   → Kiểm tra client ở trạng thái CLIENT_STATE_IN_ROOM
   → Kiểm tra client là owner của phòng
   → Kiểm tra phòng ở trạng thái ROOM_WAITING

4. Server: room_start_game()
   File: src/server/room.c (dòng 404)
   → Kiểm tra phòng ở trạng thái ROOM_WAITING
   → Kiểm tra có ít nhất 2 người chơi
   → Đánh dấu tất cả người chơi hiện tại là active_players[i] = 1
   → Chuyển trạng thái phòng sang ROOM_PLAYING
   → Gọi game_init()

5. Server: game_init()
   File: src/server/game.c (dòng 55)
   → Tính tổng số từ cần: total_words_needed = room->player_count * total_rounds
   → Pre-select từ từ database theo difficulty
   → Shuffle từ 5 lần để đảm bảo ngẫu nhiên
   → Lưu vào game->word_stack[]
   → Khởi tạo game state (current_round=0, drawer_index=owner_index, ...)
   → Gọi ensure_scores_initialized()

6. Server: ensure_scores_initialized() (static)
   File: src/server/game.c (dòng 31)
   → Tạo score entry cho mỗi người chơi với score=0, words_guessed=0, rounds_won=0

7. Server: protocol_handle_start_game() (tiếp tục)
   File: src/server/protocol_game.c (dòng 238-248)
   → Cập nhật trạng thái tất cả clients trong phòng sang CLIENT_STATE_IN_GAME
   → Gọi game_start_round()

8. Server: game_start_round()
   File: src/server/game.c (dòng 196)
   → Kiểm tra game đã kết thúc chưa
   → Kiểm tra hết rounds chưa (nếu có → gọi game_end())
   → Tăng current_round++
   → Chọn drawer (round đầu: giữ nguyên owner, round sau: pick_next_drawer_index())
   → Kiểm tra drawer hợp lệ và active
   → Gọi assign_new_word() để pop từ từ word_stack
   → Reset round state (word_guessed=false, round_start_time=time(NULL), guessed_count=0)

9. Server: pick_next_drawer_index() (static)
   File: src/server/game.c (dòng 161)
   → Chuyển sang người tiếp theo: (drawer_index + 1) % player_count

10. Server: assign_new_word() (static)
    File: src/server/game.c (dòng 173)
    → Pop từ từ word_stack (FIFO - lấy từ đầu)
    → Gán vào game->current_word và game->current_category
    → Tăng word_stack_top++

11. Server: protocol_handle_start_game() (tiếp tục)
    File: src/server/protocol_game.c (dòng 251-263)
    → Tạo database round (db_create_game_round) nếu có DB
    → Cập nhật room status = 'in_progress' trong DB
    → Gọi broadcast_game_start()

12. Server: broadcast_game_start() (static)
    File: src/server/protocol_game.c (dòng 80)
    → Tạo payload GAME_START với drawer_id, word_length, time_limit, round_start_ms, ...
    → Gửi đến drawer: word đầy đủ
    → Gửi đến người khác: word rỗng (chỉ word_length)
    → Tất cả nhận category

13. Frontend: handleGameStart()
    File: src/frontend/src/pages/game-room/GameRoom.jsx
    → Cập nhật gameState = 'playing'
    → Set currentDrawerId, isDrawing, word, category, timeLimit, roundStartMs
```

---

## 2. Xử Lý Đoán Từ

### Mô tả
Khi người chơi gửi tin nhắn chat trong lúc đang chơi, server tự động xử lý như một lần đoán từ.

### Flow các hàm được gọi:

```
1. Frontend: handleSendMessage(text)
   File: src/frontend/src/pages/game-room/GameRoom.jsx (dòng 525)
   → Gọi services.sendChatMessage(text)

2. Gateway: sendChatMessage()
   File: src/gateway/index.js
   → Chuyển đổi message thành MSG_CHAT_MESSAGE (0x30)
   → Gửi đến TCP server

3. Server: protocol_handle_chat_message()
   File: src/server/protocol_chat.c (dòng 35)
   → Kiểm tra client active và đã đăng nhập
   → Parse message text từ payload
   → Kiểm tra phòng đang chơi (ROOM_PLAYING) và có game
   → Kiểm tra client không phải drawer
   → Gọi protocol_process_guess() để xử lý như guess

4. Server: protocol_process_guess()
   File: src/server/protocol_game.c (dòng 295)
   → Lưu current_word trước khi xử lý (để dùng sau khi game_end_round xóa)
   → Gọi game_handle_guess()

5. Server: game_handle_guess()
   File: src/server/game.c (dòng 281)
   → Kiểm tra game hợp lệ và chưa kết thúc
   → Kiểm tra guess word không rỗng
   → Kiểm tra drawer không được đoán
   → So sánh từ (case-insensitive) với words_equal_case_insensitive()
   → Kiểm tra đã đoán đúng chưa (tránh cộng điểm 2 lần)
   → Tính điểm theo thứ tự: 1st (10/5), 2nd (7/4), 3rd (5/3), 4th+ (3/2)
   → Cộng điểm cho người đoán đúng (guesser_points)
   → Cộng điểm cho người vẽ (drawer_points)
   → Thêm vào danh sách guessed_user_ids[]
   → Tăng guessed_count++
   → Set word_guessed = true
   → Return true nếu đoán đúng, false nếu sai

6. Server: words_equal_case_insensitive() (static)
   File: src/server/game.c (dòng 271)
   → Chuyển cả 2 từ sang lowercase
   → So sánh với strcmp()

7. Server: find_score_index() (static)
   File: src/server/game.c (dòng 23)
   → Tìm index của user_id trong game->scores[]

8. Server: protocol_process_guess() (tiếp tục - nếu đoán sai)
   File: src/server/protocol_game.c (dòng 331-334)
   → Nếu đoán sai: return -1 (không broadcast, chỉ hiển thị chat message)

9. Server: protocol_process_guess() (tiếp tục - nếu đoán đúng)
   File: src/server/protocol_game.c (dòng 324-446)
   → Lưu guess vào database (db_save_guess) nếu có DB
   → Tính điểm theo thứ tự (guesser_order = guessed_count)
   → Tạo payload CORRECT_GUESS với player_id, word, guesser_points, drawer_points, username
   → Broadcast MSG_CORRECT_GUESS đến tất cả clients trong phòng
   → Lưu score details vào database (db_save_score_detail) nếu có DB
   → Kiểm tra tất cả đã đoán đúng chưa (guessed_count >= total_active_guessers)
   → Nếu tất cả đã đoán đúng: gọi game_end_round(true, -1) và protocol_handle_round_timeout()

10. Server: server_broadcast_to_room()
    File: src/server/server.c
    → Gửi message đến tất cả clients trong phòng (trừ exclude_user_id)

11. Frontend: handleCorrectGuess()
    File: src/frontend/src/pages/game-room/GameRoom.jsx
    → Cập nhật điểm cho người đoán đúng và người vẽ
    → Hiển thị thông báo trong chat
```

---

## 3. Xử Lý Vẽ (Drawing)

### Mô tả
Khi drawer vẽ trên canvas, dữ liệu vẽ được gửi đến server và broadcast đến các người chơi khác.

### Flow các hàm được gọi:

```
1. Frontend: handleDraw(drawData)
   File: src/frontend/src/pages/game-room/GameRoom.jsx (dòng 508)
   → Kiểm tra drawData.action
   → Nếu action = 2 (CLEAR): gọi services.sendClearCanvas()
   → Nếu action = 1 (LINE): gọi services.sendDrawData()

2. Frontend: services.sendDrawData()
   File: src/frontend/src/services/Services.js (dòng 715)
   → Tạo payload DRAW_DATA với x1, y1, x2, y2, color, width, isEraser
   → Gửi qua WebSocket gateway

3. Gateway: handleDrawData()
   File: src/gateway/index.js
   → Chuyển đổi message thành MSG_DRAW_DATA (0x22)
   → Gửi đến TCP server

4. Server: protocol_handle_draw_data()
   File: src/server/protocol_drawing.c (dòng 15)
   → Kiểm tra client active và đã đăng nhập
   → Kiểm tra client trong phòng
   → Kiểm tra phòng đang chơi (ROOM_PLAYING)
   → Kiểm tra client là drawer (room->game->drawer_id == client->user_id)
   → Gọi drawing_parse_action() để parse payload

5. Server: drawing_parse_action()
   File: src/server/drawing.c (dòng 9)
   → Parse action type (1 byte)
   → Parse tọa độ x1, y1, x2, y2 (mỗi 2 bytes, network byte order)
   → Parse color (4 bytes, network byte order)
   → Parse width (1 byte)
   → Validate action với drawing_validate_action()

6. Server: protocol_handle_draw_data() (tiếp tục)
   File: src/server/protocol_drawing.c (dòng 64-75)
   → Gọi drawing_serialize_action() để serialize lại action
   → Broadcast MSG_DRAW_BROADCAST đến tất cả clients trong phòng (trừ drawer)

7. Server: drawing_serialize_action()
   File: src/server/drawing.c (dòng 57)
   → Serialize action thành binary format (14 bytes)
   → Return payload length

8. Server: server_broadcast_to_room()
   File: src/server/server.c
   → Gửi MSG_DRAW_BROADCAST đến tất cả clients trong phòng (trừ drawer)

9. Frontend: handleDrawBroadcast()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Parse draw action từ payload
   → Vẽ trên canvas của người chơi khác (không phải drawer)
```

---

## 4. Kiểm Tra Timeout Round

### Mô tả
Server tick mỗi giây để kiểm tra timeout cho tất cả phòng đang chơi.

### Flow các hàm được gọi:

```
1. Server: server_event_loop()
   File: src/server/server.c (dòng 430)
   → Vòng lặp chính của server với select() timeout 1 giây
   → Sau mỗi lần select(), kiểm tra timeout cho tất cả phòng

2. Server: server_event_loop() (tiếp tục)
   File: src/server/server.c (dòng 474-491)
   → Duyệt qua tất cả phòng (MAX_ROOMS)
   → Kiểm tra phòng đang chơi (ROOM_PLAYING) và có game
   → Lưu current_word trước khi kiểm tra (vì game_end_round sẽ xóa)
   → Gọi game_check_timeout()

3. Server: game_check_timeout()
   File: src/server/game.c (dòng 258)
   → Kiểm tra game hợp lệ và chưa kết thúc
   → Kiểm tra round_start_time != 0
   → Tính elapsed = now - round_start_time
   → Nếu elapsed > time_limit: gọi game_end_round(false, -1) và return true
   → Return false nếu chưa timeout

4. Server: game_end_round()
   File: src/server/game.c (dòng 406)
   → Reset word/timer: current_word[0] = '\0', round_start_time = 0
   → Kiểm tra hết rounds chưa (current_round >= total_rounds)
   → Nếu hết: gọi game_end()

5. Server: game_end()
   File: src/server/game.c (dòng 428)
   → Set game_ended = true
   → Gọi compute_winner_user_id() để tính winner

6. Server: compute_winner_user_id() (static)
   File: src/server/game.c (dòng 393)
   → Tìm user_id có điểm cao nhất trong game->scores[]

7. Server: server_event_loop() (tiếp tục)
   File: src/server/server.c (dòng 487-490)
   → Nếu game_check_timeout() return true: gọi protocol_handle_round_timeout()

8. Server: protocol_handle_round_timeout()
   File: src/server/protocol_game.c (dòng 452)
   → Broadcast ROUND_END với word đã lưu (nếu word_before_clear != '\0')
   → Kiểm tra game đã kết thúc chưa
   → Nếu đã kết thúc: gọi protocol_broadcast_game_end() và room_end_game()
   → Nếu chưa: thử bắt đầu round mới với game_start_round()
   → Nếu drawer không active: tiếp tục thử drawer tiếp theo (tối đa max_attempts)
   → Nếu bắt đầu thành công: gọi broadcast_game_start()
```

---

## 5. Cập Nhật Timer

### Mô tả
Server gửi timer update mỗi giây đến tất cả phòng đang chơi để đồng bộ thời gian.

### Flow các hàm được gọi:

```
1. Server: server_event_loop()
   File: src/server/server.c (dòng 493-503)
   → Kiểm tra đã qua 1 giây kể từ last_timer_update
   → Duyệt qua tất cả phòng đang chơi
   → Gọi protocol_broadcast_timer_update() cho mỗi phòng

2. Server: protocol_broadcast_timer_update()
   File: src/server/protocol_game.c (dòng 513)
   → Kiểm tra game hợp lệ và chưa kết thúc
   → Tính elapsed = now - round_start_time
   → Tính time_left = time_limit - elapsed
   → Đảm bảo time_left >= 0
   → Tạo payload TIMER_UPDATE với time_left (2 bytes)
   → Broadcast đến tất cả clients trong phòng

3. Frontend: handleTimerUpdate()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Cập nhật serverTimeLeft (thời gian từ server - authoritative)
   → Hybrid timer: kết hợp server time với client smoothing
```

---

## 6. Kết Thúc Round

### Mô tả
Round kết thúc khi timeout hoặc tất cả người chơi đã đoán đúng. Server broadcast ROUND_END và bắt đầu round tiếp theo.

### Flow các hàm được gọi:

```
1. Trigger: Timeout hoặc tất cả đã đoán đúng
   → Từ game_check_timeout() hoặc protocol_process_guess()

2. Server: game_end_round()
   File: src/server/game.c (dòng 406)
   → Reset word/timer: current_word[0] = '\0', round_start_time = 0
   → Kiểm tra hết rounds chưa
   → Nếu hết: gọi game_end()

3. Server: protocol_handle_round_timeout()
   File: src/server/protocol_game.c (dòng 452)
   → Broadcast ROUND_END với word đã lưu (nếu word_before_clear != '\0')

4. Server: broadcast_round_end() (static)
   File: src/server/protocol_game.c (dòng 124)
   → Tạo payload ROUND_END với word, score_count, và pairs (user_id, score)
   → Broadcast đến tất cả clients trong phòng

5. Frontend: handleRoundEnd()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Cập nhật scores từ payload
   → Hiển thị từ đúng trong chat
   → Reset UI (isDrawing=false, word='', category='')

6. Server: protocol_handle_round_timeout() (tiếp tục)
   File: src/server/protocol_game.c (dòng 467-506)
   → Kiểm tra game đã kết thúc chưa
   → Nếu chưa: thử bắt đầu round mới với game_start_round()
   → Nếu drawer không active: tiếp tục thử drawer tiếp theo
   → Nếu bắt đầu thành công: gọi broadcast_game_start()
```

---

## 7. Kết Thúc Game

### Mô tả
Game kết thúc khi hết tất cả rounds hoặc số người chơi active < 2. Server tính winner, lưu lịch sử và broadcast GAME_END.

### Flow các hàm được gọi:

```
1. Trigger: Hết rounds hoặc active_count < 2
   → Từ game_end_round() hoặc protocol_handle_leave_room()

2. Server: game_end()
   File: src/server/game.c (dòng 428)
   → Set game_ended = true
   → Gọi compute_winner_user_id()

3. Server: compute_winner_user_id() (static)
   File: src/server/game.c (dòng 393)
   → Tìm user_id có điểm cao nhất trong game->scores[]

4. Server: protocol_broadcast_game_end()
   File: src/server/protocol_game.c (dòng 149)
   → Tính winner_id (user_id có điểm cao nhất)
   → Sắp xếp scores theo điểm giảm dần (bubble sort)
   → Lưu lịch sử vào database (db_save_game_history) với rank cho mỗi người chơi
   → Tạo payload GAME_END với winner_id, score_count, và pairs (user_id, score)
   → Broadcast đến tất cả clients trong phòng

5. Server: room_end_game()
   File: src/server/room.c (dòng 451)
   → Free game state (game_destroy)
   → Set room->game = NULL
   → Chuyển trạng thái phòng sang ROOM_FINISHED

6. Server: game_destroy()
   File: src/server/game.c (dòng 156)
   → Free game_state_t

7. Frontend: handleGameEnd()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Cập nhật scores và lưu leaderboard data riêng
   → Sắp xếp players theo điểm giảm dần
   → Set gameState = 'finished'
   → Set showLeaderboard = true (hiển thị modal)
```

---

## 8. Người Chơi Rời Phòng Trong Game

### Mô tả
Khi người chơi rời phòng trong lúc đang chơi, server đánh dấu inactive, kiểm tra số người active, và xử lý nếu drawer rời phòng.

### Flow các hàm được gọi:

```
1. Frontend: handleLeaveRoom()
   File: src/frontend/src/pages/game-room/GameRoom.jsx (dòng 491)
   → Gọi services.leaveRoom(roomId)

2. Gateway: leaveRoom()
   File: src/gateway/index.js
   → Chuyển đổi message thành MSG_LEAVE_ROOM (0x14)
   → Gửi đến TCP server

3. Server: protocol_handle_leave_room()
   File: src/server/protocol_room.c
   → Kiểm tra client active và đã đăng nhập
   → Kiểm tra client trong phòng
   → Lưu thông tin: was_playing, was_drawer, word_before
   → Gọi room_remove_player()

4. Server: room_remove_player()
   File: src/server/room.c (dòng 184)
   → Tìm player_index của user_id
   → Nếu đang chơi (ROOM_PLAYING):
      → Đánh dấu active_players[player_index] = -1 (KHÔNG xóa khỏi mảng)
      → Nếu owner rời: chuyển owner cho người chơi active đầu tiên
      → Đếm số người chơi active
      → Nếu active_count < 2: in cảnh báo (game sẽ được end ở nơi gọi)
   → Nếu không chơi: xóa khỏi mảng (shift array)

5. Server: protocol_handle_leave_room() (tiếp tục)
   File: src/server/protocol_room.c
   → Đếm số người chơi active
   → Nếu active_count < 2 và đang chơi:
      → Set game->game_ended = true
      → Gọi protocol_broadcast_game_end()
      → Gọi room_end_game()
   → Nếu drawer rời phòng và chưa end game:
      → Gọi game_end_round(false, -1)
      → Gọi protocol_handle_round_timeout() để bắt đầu round tiếp theo
   → Broadcast ROOM_PLAYERS_UPDATE với is_active = 255 (đã rời phòng)

6. Server: protocol_broadcast_room_players_update()
   File: src/server/protocol_room.c
   → Tạo payload ROOM_PLAYERS_UPDATE với danh sách người chơi
   → is_active = 255 nếu đã rời phòng, 1 nếu active, 0 nếu đang chờ
   → Broadcast đến tất cả clients trong phòng

7. Frontend: handleRoomPlayersUpdate()
   File: src/frontend/src/pages/game-room/GameRoom.jsx
   → Nếu game đã kết thúc: không cập nhật (giữ nguyên leaderboard)
   → Cập nhật danh sách người chơi với isActive và hasLeft
```

---

## 9. Người Chơi Disconnect Trong Game

### Mô tả
Khi người chơi mất kết nối (disconnect), server xử lý tương tự như rời phòng nhưng không có response message.

### Flow các hàm được gọi:

```
1. Server: server_handle_client_data()
   File: src/server/server.c
   → Phát hiện socket đóng (recv() return 0 hoặc error)
   → Gọi server_handle_disconnect()

2. Server: server_handle_disconnect()
   File: src/server/server.c (dòng ~350)
   → Kiểm tra client đang trong phòng (CLIENT_STATE_IN_ROOM hoặc CLIENT_STATE_IN_GAME)
   → Lưu thông tin: was_playing, was_drawer, word_before
   → Gọi room_remove_player() (tương tự như leave room)

3. Server: room_remove_player()
   File: src/server/room.c (dòng 184)
   → Xử lý tương tự như leave room
   → Đánh dấu inactive nếu đang chơi

4. Server: server_handle_disconnect() (tiếp tục)
   File: src/server/server.c
   → Kiểm tra active_count < 2 → end game
   → Nếu drawer disconnect → end round và chuyển round tiếp theo
   → Broadcast ROOM_PLAYERS_UPDATE
   → Gọi server_remove_client() để xóa client khỏi danh sách
```

---

## Tóm Tắt Các File Chính

### Server Side (C)

| File | Chức năng chính |
|------|----------------|
| `src/server/protocol_game.c` | Xử lý protocol game (start, guess, round timeout, game end) |
| `src/server/game.c` | Logic game (init, start round, handle guess, end round, end game) |
| `src/server/room.c` | Quản lý phòng (start game, remove player, end game) |
| `src/server/protocol_chat.c` | Xử lý chat message (tự động xử lý như guess nếu đang chơi) |
| `src/server/protocol_drawing.c` | Xử lý drawing data (broadcast đến các client khác) |
| `src/server/drawing.c` | Parse và serialize drawing actions |
| `src/server/server.c` | Server event loop (timeout check, timer update) |

### Frontend Side (JavaScript/React)

| File | Chức năng chính |
|------|----------------|
| `src/frontend/src/pages/game-room/GameRoom.jsx` | Component chính của game room, xử lý tất cả events |
| `src/frontend/src/services/Services.js` | Service layer để giao tiếp với gateway |
| `src/gateway/index.js` | Gateway WebSocket, chuyển đổi message giữa frontend và TCP server |

---

## Lưu Ý Quan Trọng

1. **Drawer không active**: Khi bắt đầu round, nếu drawer không active, round sẽ kết thúc ngay nhưng vẫn được đếm. Server sẽ thử drawer tiếp theo.

2. **Active players tracking**: 
   - `active_players[i] = 1`: Active
   - `active_players[i] = 0`: Đang chờ (join giữa chừng)
   - `active_players[i] = -1`: Đã rời phòng

3. **Word stack**: Từ được pre-select khi khởi tạo game và lưu trong `word_stack[]`. Khi bắt đầu round, pop từ đầu stack (FIFO).

4. **Score calculation**: Điểm được tính theo thứ tự đoán đúng:
   - 1st: guesser +10, drawer +5
   - 2nd: guesser +7, drawer +4
   - 3rd: guesser +5, drawer +3
   - 4th+: guesser +3, drawer +2

5. **Timer synchronization**: Server gửi timer update mỗi giây (authoritative). Frontend kết hợp với client timer để làm mượt hiển thị.

6. **Database persistence**: Tất cả actions đều được lưu vào database (best-effort):
   - Game rounds
   - Guesses
   - Score details
   - Game history

