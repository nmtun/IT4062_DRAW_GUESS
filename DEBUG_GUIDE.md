# Hướng Dẫn Debug - Không Nhận Được Response

## Các Bước Kiểm Tra

### 1. Kiểm Tra Server C Đang Chạy
```bash
# Kiểm tra process đang chạy trên port 8080
netstat -ano | findstr :8080
# Hoặc trên Linux
lsof -i :8080
```

**Nếu không có process nào:**
- Chạy server C: `cd src && make && ./main`
- Đảm bảo server hiển thị: `Server đang lắng nghe trên port 8080`

### 2. Kiểm Tra Gateway Đang Chạy
```bash
# Kiểm tra process đang chạy trên port 3001
netstat -ano | findstr :3001
```

**Nếu không có:**
- Chạy gateway: `npm start` (từ thư mục gốc)
- Đảm bảo gateway hiển thị: `WebSocket Gateway đang chạy trên port 3001`

### 3. Kiểm Tra Log Gateway

Khi client kết nối, gateway sẽ hiển thị:
```
🔌 Client WebSocket đã kết nối
Đang kết nối đến TCP server localhost:8080...
✅ Đã kết nối đến TCP server
✅ TCP connection established
```

**Nếu thấy lỗi:**
```
❌ TCP connection error: connect ECONNREFUSED
⚠️  Không thể kết nối đến server C tại localhost:8080
⚠️  Hãy đảm bảo server C đang chạy!
```
→ **Server C chưa chạy hoặc không đúng port**

### 4. Kiểm Tra Log Khi Gửi Request

Khi đăng nhập, gateway sẽ hiển thị:
```
📤 WS → TCP: 67 bytes (type=0x01, length=64)
```

**Nếu không thấy:**
- Client chưa gửi request
- WebSocket chưa kết nối

### 5. Kiểm Tra Log Khi Nhận Response

Khi server C trả về, gateway sẽ hiển thị:
```
📥 TCP → WS: 39 bytes (type=0x02, length=36)
```

**Nếu không thấy:**
- Server C không gửi response
- Server C chưa xử lý request
- Kiểm tra log server C

### 6. Kiểm Tra Log Server C

Khi nhận request, server C sẽ hiển thị:
```
Nhận LOGIN_REQUEST từ client 0: username=...
Client 0 đăng nhập thành công: user_id=1, username=...
```

**Nếu không thấy:**
- Server C không nhận được request
- Kiểm tra kết nối TCP

## Các Lỗi Thường Gặp

### Lỗi 1: Timeout - Không Nhận Được Response
**Nguyên nhân:**
- Server C chưa chạy
- Gateway không kết nối được đến server C
- Server C không xử lý request

**Giải pháp:**
1. Kiểm tra server C đang chạy
2. Kiểm tra log gateway xem có kết nối TCP không
3. Kiểm tra log server C xem có nhận request không

### Lỗi 2: ECONNREFUSED
**Nguyên nhân:**
- Server C chưa chạy
- Port không đúng

**Giải pháp:**
- Chạy server C trước khi chạy gateway

### Lỗi 3: TCP Chưa Kết Nối
**Nguyên nhân:**
- Gateway chưa kết nối được đến server C
- Server C chưa chạy

**Giải pháp:**
- Đảm bảo server C chạy trước gateway

## Thứ Tự Chạy Đúng

1. **Chạy Database** (nếu cần)
   ```bash
   cd src
   docker-compose up -d
   ```

2. **Chạy Server C**
   ```bash
   cd src
   make
   ./main
   ```
   → Đợi thấy: `Server đang lắng nghe trên port 8080`

3. **Chạy Gateway** (terminal mới)
   ```bash
   npm start
   ```
   → Đợi thấy: `Gateway sẵn sàng nhận kết nối...`

4. **Mở Browser**
   - Mở `src/client/index.html`
   - Mở Console (F12)
   - Thử đăng nhập

## Kiểm Tra Nhanh

Chạy lệnh này để kiểm tra tất cả:
```bash
# Kiểm tra server C
netstat -ano | findstr :8080

# Kiểm tra gateway
netstat -ano | findstr :3001
```

Cả hai đều phải có process đang LISTENING.


