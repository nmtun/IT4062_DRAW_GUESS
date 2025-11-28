# Draw & Guess Gateway

WebSocket to TCP Gateway cho ứng dụng Draw & Guess Game, kết nối frontend (WebSocket) với backend server (TCP C).

## Chức năng

Gateway này thực hiện:
- Nhận WebSocket connections từ frontend
- Chuyển đổi JSON messages thành binary protocol 
- Chuyển tiếp messages đến TCP server (C)
- Parse binary responses từ TCP server thành JSON
- Gửi responses về frontend qua WebSocket

## Cài đặt

```bash
npm install
```

## Chạy Gateway

```bash
# Chạy bình thường
npm start

# Chạy với debug mode
npm run dev
```

## Cấu hình

Mặc định:
- WebSocket Server: `localhost:3000`
- TCP Server: `localhost:8080`

Có thể thay đổi trong constructor của class Gateway:
```javascript
const gateway = new Gateway(wsPort, tcpHost, tcpPort);
```

## Message Protocol

### WebSocket Messages (JSON)

Từ Frontend -> Gateway:
```json
{
  "type": "login",
  "data": {
    "username": "user123",
    "password": "pass123"
  }
}
```

Từ Gateway -> Frontend:
```json
{
  "type": "login_response", 
  "data": {
    "status": "success",
    "userId": 123,
    "username": "user123"
  }
}
```

### Supported Message Types

#### Authentication
- `login` -> `login_response`
- `register` -> `register_response`
- `logout`

#### Room Management
- `room_list` -> `room_list_response`
- `create_room` -> `room_update`
- `join_room` -> `room_update`
- `leave_room`
- `start_game` -> `game_start`

#### Game Play
- `draw_data` -> `draw_broadcast`
- `guess_word` -> `correct_guess`/`wrong_guess`
- `chat_message` -> `chat_broadcast`

#### Game Events
- `game_state`
- `round_end`
- `game_end`

## Binary Protocol

Gateway chuyển đổi JSON thành binary protocol theo định dạng:
```
[TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]
```

### Message Types (Hex)
- 0x01: LOGIN_REQUEST
- 0x02: LOGIN_RESPONSE
- 0x03: REGISTER_REQUEST
- 0x04: REGISTER_RESPONSE
- 0x10: ROOM_LIST_REQUEST
- 0x11: ROOM_LIST_RESPONSE
- 0x22: DRAW_DATA
- 0x23: DRAW_BROADCAST
- 0x24: GUESS_WORD
- 0x30: CHAT_MESSAGE
- 0x31: CHAT_BROADCAST

## Testing

Có thể test gateway bằng WebSocket client:

```javascript
const ws = new WebSocket('ws://localhost:3000');

ws.onopen = function() {
    // Gửi login request
    ws.send(JSON.stringify({
        type: 'login',
        data: {
            username: 'testuser',
            password: 'testpass'
        }
    }));
};

ws.onmessage = function(event) {
    const message = JSON.parse(event.data);
    console.log('Received:', message);
};
```

## Logs

Gateway sẽ log:
- WebSocket connections/disconnections
- TCP server connections
- Message forwarding
- Errors

## Error Handling

- Tự động kết nối lại TCP server nếu bị ngắt
- Đóng WebSocket nếu TCP connection thất bại
- Parse error handling cho invalid messages
- Graceful shutdown với SIGINT/SIGTERM