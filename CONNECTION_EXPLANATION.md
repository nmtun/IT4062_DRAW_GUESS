# Giải Thích Code Kết Nối Với Server C

## Tổng Quan Kiến Trúc

```
Browser (JavaScript)
    ↓ WebSocket
websocket-gateway.js (Node.js)
    ↓ TCP Socket (net.Socket)
Server C (main.c, server.c, protocol.c)
```

## Phần Code Kết Nối Với Server C

### File: `websocket-gateway.js`

Đây là file **DUY NHẤT** kết nối trực tiếp với server C qua TCP socket.

#### 1. Import Module TCP Socket (dòng 3)
```javascript
const net = require('net');
```
- Module `net` của Node.js để tạo TCP socket connection

#### 2. Cấu Hình Kết Nối (dòng 5-7)
```javascript
const WS_PORT = 3001; // WebSocket server port
const TCP_HOST = 'localhost';
const TCP_PORT = 8080; // C server port
```
- `TCP_PORT = 8080`: Port mà server C đang lắng nghe
- `TCP_HOST = 'localhost'`: Địa chỉ server C

#### 3. Tạo TCP Socket Connection (dòng 19-27)
```javascript
// Tạo TCP connection đến C server
const tcpClient = new net.Socket();
let isTcpConnected = false;

// Kết nối đến TCP server
console.log(`Đang kết nối đến TCP server ${TCP_HOST}:${TCP_PORT}...`);
tcpClient.connect(TCP_PORT, TCP_HOST, function() {
    console.log('✅ Đã kết nối đến TCP server');
    isTcpConnected = true;
});
```

**Đây là phần code QUAN TRỌNG NHẤT:**
- `new net.Socket()`: Tạo TCP socket client
- `tcpClient.connect(TCP_PORT, TCP_HOST, callback)`: Kết nối đến server C tại `localhost:8080`
- Khi kết nối thành công, callback được gọi và set `isTcpConnected = true`

#### 4. Gửi Data Đến Server C (dòng 53-65, 86-88)
```javascript
// Nhận dữ liệu từ WebSocket client → gửi đến TCP server
ws.on('message', function(message) {
    sendOrQueueMessage(message);
});

function sendOrQueueMessage(message) {
    const buffer = Buffer.isBuffer(message) ? message : Buffer.from(message);
    
    if (isTcpConnected && tcpClient.writable) {
        console.log(`📤 WS → TCP: ${buffer.length} bytes`);
        tcpClient.write(buffer);  // ← GỬI DATA ĐẾN SERVER C
    } else {
        // Queue message nếu chưa kết nối
        messageQueue.push(buffer);
    }
}
```

**Giải thích:**
- `ws.on('message')`: Nhận message từ browser (WebSocket)
- `tcpClient.write(buffer)`: **GỬI DATA ĐẾN SERVER C** qua TCP socket
- Data được gửi dưới dạng Buffer (raw bytes)

#### 5. Nhận Data Từ Server C (dòng 37-47)
```javascript
// Nhận dữ liệu từ TCP server → gửi đến WebSocket client
tcpClient.on('data', function(data) {
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(data);  // ← NHẬN DATA TỪ SERVER C
        const type = data[0];
        const length = (data[1] << 8) | data[2];
        console.log(`📥 TCP → WS: ${data.length} bytes`);
    }
});
```

**Giải thích:**
- `tcpClient.on('data')`: Event listener khi server C gửi data về
- `data`: Buffer chứa raw bytes từ server C
- `ws.send(data)`: Chuyển tiếp data đến browser qua WebSocket

## Luồng Dữ Liệu

### Khi Browser Gửi Login Request:

1. **Browser** (`network.js`):
   ```javascript
   socket.send(buffer);  // Gửi qua WebSocket
   ```

2. **Gateway** (`websocket-gateway.js`):
   ```javascript
   ws.on('message', function(message) {
       tcpClient.write(buffer);  // ← GỬI ĐẾN SERVER C
   });
   ```

3. **Server C** (`server.c`):
   ```c
   recv(client_fd, buffer, BUFFER_SIZE, 0);  // Nhận data
   protocol_handle_message(...);  // Xử lý
   ```

### Khi Server C Gửi Response:

1. **Server C** (`protocol.c`):
   ```c
   send(client_fd, buffer, msg_len, 0);  // Gửi data
   ```

2. **Gateway** (`websocket-gateway.js`):
   ```javascript
   tcpClient.on('data', function(data) {
       ws.send(data);  // ← NHẬN TỪ SERVER C, CHUYỂN ĐẾN BROWSER
   });
   ```

3. **Browser** (`network.js`):
   ```javascript
   socket.onmessage = function(event) {
       handleMessage(event.data);  // Nhận response
   };
   ```

## Protocol Format

Cả gateway và server C đều sử dụng cùng format:

```
[TYPE: 1 byte][LENGTH: 2 bytes][PAYLOAD: variable]
```

Ví dụ Login Request:
- Type: `0x01` (MSG_LOGIN_REQUEST)
- Length: `64` (32 bytes username + 32 bytes password)
- Payload: `username(32) + password(32)`

## Tóm Tắt

**File kết nối với server C:**
- ✅ `websocket-gateway.js` - File DUY NHẤT kết nối TCP với server C

**Các dòng code quan trọng:**
1. **Dòng 19**: `const tcpClient = new net.Socket();` - Tạo TCP socket
2. **Dòng 24**: `tcpClient.connect(TCP_PORT, TCP_HOST, ...)` - Kết nối đến server C
3. **Dòng 60**: `tcpClient.write(buffer)` - Gửi data đến server C
4. **Dòng 38**: `tcpClient.on('data', ...)` - Nhận data từ server C

**Các file khác KHÔNG kết nối trực tiếp:**
- ❌ `network.js` - Chỉ kết nối WebSocket đến gateway
- ❌ `auth.js` - Chỉ gọi hàm từ network.js
- ❌ Browser HTML/JS - Chỉ giao tiếp qua WebSocket


