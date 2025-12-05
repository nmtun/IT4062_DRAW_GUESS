# Draw Client - Test Đồng Bộ Vẽ

Client HTML đơn giản để test việc đồng bộ hình vẽ giữa các user.

## Cách sử dụng

### 1. Khởi động Server

```bash
cd src
./main
```

### 2. Khởi động Gateway (WebSocket)

```bash
cd src/gateway
npm install  # Nếu chưa cài
npm start
```

Gateway sẽ chạy trên port 3000 (WebSocket) và forward đến TCP server port 8080.

### 3. Mở Client HTML

Mở file `index.html` trong trình duyệt (hoặc dùng local server):

```bash
# Cách 1: Dùng Python
cd DrawClient
python3 -m http.server 8000

# Cách 2: Dùng Node.js http-server
npx http-server -p 8000

# Sau đó mở: http://localhost:8000
```

### 4. Test Đồng Bộ

1. **Mở nhiều tab/window** của trình duyệt
2. **Đăng nhập** với cùng username/password (hoặc khác nhau)
3. **Vẽ trên một tab** - các tab khác sẽ thấy hình vẽ được đồng bộ real-time
4. **Test các tính năng:**
   - Thay đổi màu sắc
   - Thay đổi độ rộng bút vẽ
   - Xóa canvas (Clear)

## Lưu ý

- Client cần đăng nhập thành công trước khi có thể vẽ
- Để test đồng bộ, cần mở ít nhất 2 tab/window
- Client gửi DRAW_DATA khi vẽ, server sẽ broadcast DRAW_BROADCAST đến các clients khác
- WebSocket URL mặc định: `ws://localhost:3000`

## Tính năng

- ✅ Vẽ trên canvas với chuột
- ✅ Thay đổi màu sắc
- ✅ Thay đổi độ rộng bút vẽ (1-20px)
- ✅ Xóa canvas
- ✅ Đồng bộ real-time giữa các clients
- ✅ Hiển thị log events
- ✅ Trạng thái kết nối

## Protocol

Client sử dụng binary protocol trực tiếp:
- `MSG_LOGIN_REQUEST` (0x01): Đăng nhập
- `MSG_DRAW_DATA` (0x22): Gửi dữ liệu vẽ
- `MSG_DRAW_BROADCAST` (0x23): Nhận dữ liệu vẽ từ server

## Troubleshooting

### Không kết nối được
- Kiểm tra server đã chạy chưa: `./main`
- Kiểm tra gateway đã chạy chưa: `npm start` trong `src/gateway`
- Kiểm tra WebSocket URL trong client

### Không thấy đồng bộ
- Đảm bảo đã đăng nhập thành công
- Kiểm tra console log trong browser (F12)
- Kiểm tra gateway log để xem messages có được forward không

