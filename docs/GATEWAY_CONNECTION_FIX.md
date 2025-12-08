# Tài liệu: Sửa lỗi "Connection already in progress" trong Gateway

## 📋 Tổng quan

Tài liệu này mô tả chi tiết về lỗi **"Connection already in progress"** xảy ra trong Gateway WebSocket và cách khắc phục.

## 🐛 Mô tả lỗi

### Lỗi gặp phải

```
[ERROR] Failed to connect to TCP server: Error: Connection already in progress
    at /Users/andrew_ta/Andrew-Code/IT4062_DRAW_GUESS/src/gateway/utils.js:80:24
    at new Promise (<anonymous>)
    at TcpConnectionManager.connect (/Users/andrew_ta/Andrew-Code/IT4062_DRAW_GUESS/src/gateway/utils.js:78:16)
    at connectToTcpServer (/Users/andrew_ta/Andrew-Code/IT4062_DRAW_GUESS/src/gateway/index.js:54:61)
```

### Triệu chứng

- Gateway WebSocket server khởi động thành công
- Khi nhiều WebSocket client kết nối đồng thời hoặc một client gửi nhiều message liên tiếp, lỗi "Connection already in progress" xuất hiện
- Một số message không thể forward đến TCP server
- Client nhận được warning: "Cannot forward message - TCP not connected"

## 🔍 Nguyên nhân

### Vấn đề 1: TcpConnectionManager được chia sẻ giữa các clients

**Code ban đầu:**

```javascript
class Gateway {
    constructor(wsPort = 3000, tcpHost = 'localhost', tcpPort = 8080) {
        // ...
        this.tcpConnectionManager = new TcpConnectionManager(tcpHost, tcpPort);
    }
}
```

**Vấn đề:**
- Tất cả WebSocket clients dùng chung một instance `TcpConnectionManager`
- Biến `isConnecting` trong `TcpConnectionManager` là shared state
- Khi client A bắt đầu kết nối (`isConnecting = true`), client B cũng cố gắng kết nối → lỗi

### Vấn đề 2: Race condition khi cùng một client gửi nhiều message

**Code ban đầu:**

```javascript
const connectToTcpServer = async () => {
    try {
        tcpClient = await this.tcpConnectionManager.connect();
        // ...
    } catch (error) {
        // ...
    }
};

ws.on('message', async (data) => {
    if (!isConnected) {
        await connectToTcpServer();
    }
    // ...
});
```

**Vấn đề:**
- Client gửi message 1 → gọi `connectToTcpServer()` → `isConnecting = true`
- Client gửi message 2 ngay sau đó (trước khi kết nối hoàn tất) → gọi `connectToTcpServer()` lần nữa
- Lần gọi thứ 2 thấy `isConnecting = true` → throw error "Connection already in progress"

### Cơ chế bảo vệ trong TcpConnectionManager

```javascript
class TcpConnectionManager {
    connect() {
        return new Promise((resolve, reject) => {
            if (this.isConnecting) {
                reject(new Error('Connection already in progress'));
                return;
            }
            this.isConnecting = true;
            // ...
        });
    }
}
```

Cơ chế này đúng nhưng không phù hợp khi nhiều clients hoặc nhiều message cùng lúc.

## ✅ Giải pháp

### Fix 1: Tạo TcpConnectionManager riêng cho mỗi WebSocket client

**Thay đổi:**

```javascript
class Gateway {
    constructor(wsPort = 3000, tcpHost = 'localhost', tcpPort = 8080) {
        // XÓA dòng này:
        // this.tcpConnectionManager = new TcpConnectionManager(tcpHost, tcpPort);
    }

    handleWebSocketConnection(ws) {
        // TẠO TcpConnectionManager riêng cho mỗi client
        const tcpConnectionManager = new TcpConnectionManager(this.tcpHost, this.tcpPort);
        // ...
    }
}
```

**Lợi ích:**
- Mỗi client có `TcpConnectionManager` riêng
- Không còn conflict giữa các clients
- Mỗi client có thể kết nối độc lập

### Fix 2: Thêm cơ chế đợi (waiting mechanism) cho quá trình kết nối

**Thay đổi:**

```javascript
handleWebSocketConnection(ws) {
    let tcpClient = null;
    let isConnected = false;
    let connectingPromise = null; // ✅ Thêm biến này
    const messageBuffer = new MessageBuffer();
    
    const tcpConnectionManager = new TcpConnectionManager(this.tcpHost, this.tcpPort);

    const connectToTcpServer = async () => {
        // ✅ Nếu đang có quá trình kết nối, đợi nó hoàn tất
        if (connectingPromise) {
            return await connectingPromise;
        }
        
        // ✅ Nếu đã kết nối, không cần kết nối lại
        if (isConnected) {
            return;
        }
        
        // ✅ Tạo promise mới cho quá trình kết nối
        connectingPromise = (async () => {
            try {
                tcpClient = await tcpConnectionManager.connect();
                isConnected = true;
                connectingPromise = null; // Reset sau khi thành công
                
                // Setup event handlers...
                
            } catch (error) {
                connectingPromise = null; // Reset khi có lỗi
                throw error;
            }
        })();
        
        return await connectingPromise;
    };
}
```

**Cách hoạt động:**
1. Message đầu tiên đến → tạo `connectingPromise` mới
2. Message thứ 2 đến (trước khi kết nối hoàn tất) → thấy `connectingPromise` đã tồn tại → đợi promise đó
3. Khi kết nối hoàn tất → `connectingPromise = null`, `isConnected = true`
4. Các message tiếp theo → thấy `isConnected = true` → không cần kết nối lại

### Reset connectingPromise trong các trường hợp

```javascript
// Khi kết nối thành công
connectingPromise = null;

// Khi có lỗi trong catch block
catch (error) {
    connectingPromise = null;
    // ...
}

// Khi TCP connection đóng
tcpClient.on('close', () => {
    connectingPromise = null;
    // ...
});

// Khi TCP connection có lỗi
tcpClient.on('error', (error) => {
    connectingPromise = null;
    // ...
});
```

## 📝 Tóm tắt thay đổi

### File: `src/gateway/index.js`

1. **Xóa** trong constructor:
   ```javascript
   this.tcpConnectionManager = new TcpConnectionManager(tcpHost, tcpPort);
   ```

2. **Thêm** trong `handleWebSocketConnection`:
   ```javascript
   let connectingPromise = null;
   const tcpConnectionManager = new TcpConnectionManager(this.tcpHost, this.tcpPort);
   ```

3. **Sửa** hàm `connectToTcpServer`:
   - Thêm logic kiểm tra `connectingPromise`
   - Thêm logic kiểm tra `isConnected`
   - Wrap kết nối trong IIFE async để tạo promise
   - Reset `connectingPromise` ở các điểm thích hợp

## 🎯 Kết quả

### Trước khi fix:
- ❌ Lỗi "Connection already in progress" khi nhiều client kết nối
- ❌ Message bị mất khi client gửi nhiều message liên tiếp
- ❌ Race condition giữa các clients

### Sau khi fix:
- ✅ Mỗi client có TcpConnectionManager riêng
- ✅ Các message từ cùng một client được xử lý tuần tự
- ✅ Không còn race condition
- ✅ Tất cả message đều được forward đến TCP server

## 🔧 Testing

### Các scenario cần test:

1. **Nhiều clients kết nối đồng thời:**
   - Mở nhiều tab trình duyệt
   - Tất cả đều login thành công
   - Không có lỗi "Connection already in progress"

2. **Client gửi nhiều message liên tiếp:**
   - Client gửi login message
   - Ngay sau đó gửi room_list message
   - Cả hai message đều được xử lý thành công

3. **Client reconnect:**
   - Client disconnect và reconnect
   - Kết nối mới hoạt động bình thường

## 📚 Tham khảo

- File liên quan: `src/gateway/index.js`, `src/gateway/utils.js`
- Class: `TcpConnectionManager`, `Gateway`
- Protocol: WebSocket → TCP Gateway

## 📅 Lịch sử

- **Ngày fix:** [Ngày hiện tại]
- **Người fix:** [Tên]
- **Version:** 1.0

---

**Lưu ý:** Tài liệu này mô tả fix cho lỗi race condition trong Gateway. Nếu gặp vấn đề tương tự trong tương lai, tham khảo phần "Giải pháp" để áp dụng.

