# Hướng Dẫn Chạy Hệ Thống

## Các Bước Chạy

### 1. Chạy Database (nếu chưa chạy)
```bash
cd src
docker-compose up -d
```

### 2. Chạy Server C
```bash
cd src
make
./main
```
Server sẽ chạy trên port **8080** (TCP)

### 3. Chạy WebSocket Gateway
Mở terminal mới:
```bash
npm install
npm start
```
Gateway sẽ chạy trên port **3000** (WebSocket)

### 4. Mở Client
Mở file `src/client/index.html` trong browser

## Kiến Trúc

```
Browser (index.html)
    ↓ WebSocket (ws://localhost:3000)
WebSocket Gateway (Node.js, port 3000)
    ↓ TCP Socket (localhost:8080)
Server C (port 8080)
    ↓ MySQL
Database (port 3308)
```

## Lưu Ý

- Đảm bảo server C đã chạy trước khi chạy gateway
- Nếu thay đổi port, cập nhật trong:
  - `websocket-gateway.js`: `TCP_PORT` và `WS_PORT`
  - `src/client/js/network.js`: `WS_SERVER_URL`


