# 🎨 Draw & Guess - Game Vẽ và Đoán Từ

## 📖 Giới thiệu

**Draw & Guess** là một game multiplayer real-time cho phép người chơi vẽ tranh và đoán từ. Game hỗ trợ 2-8 người chơi cùng lúc, với các tính năng vẽ real-time, chat, tính điểm và lưu lịch sử trận đấu.

### Luật chơi
- Mỗi round: 1 người được chọn làm **drawer**, nhận từ bí mật
- Drawer vẽ tranh để gợi ý (không được viết chữ/số)
- Người chơi khác đoán từ qua chat
- Đoán đúng → +10 điểm (người đoán) và +5 điểm (drawer)
- Mỗi round: 60 giây
- Game kết thúc sau N rounds hoặc khi chỉ còn 1 người

### Công nghệ sử dụng
- **Server**: C + TCP Sockets + MySQL
- **Gateway**: Node.js WebSocket Gateway (chuyển đổi WebSocket ↔ TCP)
- **Frontend**: React + Vite + Material-UI
- **Database**: MySQL 8.0 (chạy trong Docker)

---

## 🚀 Hướng dẫn cài đặt

### Yêu cầu hệ thống
- **macOS** hoặc **Linux**
- **Docker** và **Docker Compose** (để chạy MySQL)
- **Node.js** >= 14.0.0 (cho Gateway và Frontend)
- **GCC** (cho việc compile server C)
- **MySQL Client Libraries** (sẽ được cài tự động)

### Bước 1: Clone repository
```bash
git clone https://github.com/nmtun/IT4062_DRAW_GUESS.git
cd IT4062_DRAW_GUESS
```

### Bước 2: Cài đặt dependencies

#### 2.1. Dependencies cho Server (C)
Vào thư mục `src` và chạy:
```bash
cd src
make install-deps
```

Lệnh này sẽ tự động cài đặt:
- **macOS**: `mysql-client`, `cjson`, `zstd` (qua Homebrew)
- **Linux**: `build-essential`, `gcc`, `libmysqlclient-dev`, `libcjson-dev`, `libzstd-dev`, `libssl-dev`, `zlib1g-dev` (qua apt)

#### 2.2. Dependencies cho Gateway (Node.js)
```bash
cd src/gateway
npm install
```

#### 2.3. Dependencies cho Frontend (React)
```bash
cd src/frontend
npm install
```

### Bước 3: Khởi động Database (MySQL)

Vẫn trong thư mục `src`, chạy:
```bash
docker compose up -d
```

Lệnh này sẽ:
- Tạo container MySQL 8.0
- Tự động chạy script `database/schema.sql` để tạo database và các bảng
- Database sẽ chạy trên port **3308** (localhost:3308)
- Thông tin đăng nhập mặc định:
  - **Username**: `root`
  - **Password**: `123456`
  - **Database**: `draw_guess`

Kiểm tra database đã chạy:
```bash
docker ps
```

### Bước 4: Build Server (C)

Trong thư mục `src`:
```bash
make
```

Lệnh này sẽ compile tất cả các file C và tạo file thực thi `main`.

**Lưu ý**: Nếu gặp lỗi về MySQL libraries, hãy đảm bảo đã chạy `make install-deps` trước đó.

---

## ▶️ Hướng dẫn chạy chương trình

Chương trình cần chạy **3 thành phần** theo thứ tự:

### 1. Chạy Database (nếu chưa chạy)
```bash
cd src
docker compose up -d
```

### 2. Chạy Server (C)
```bash
cd src
./main
```

Server sẽ chạy trên port **8080** (TCP).

### 3. Chạy Gateway (Node.js)
Mở terminal mới:
```bash
cd src/gateway
npm start
```

Gateway sẽ chạy trên port **3000** (WebSocket).

### 4. Chạy Frontend (React)
Mở terminal mới:
```bash
cd src/frontend
npm run dev
```

Frontend sẽ chạy trên port **5173** (hoặc port khác nếu 5173 đã được sử dụng).

### 5. Truy cập ứng dụng
Mở trình duyệt và truy cập:
```
http://localhost:5173
```

---

## 📁 Cấu trúc dự án

```
IT4062_DRAW_GUESS/
├── src/
│   ├── server/              # Server C (TCP)
│   │   ├── main.c          # Entry point
│   │   ├── server.c        # TCP server core
│   │   ├── auth.c          # Xác thực
│   │   ├── database.c     # Kết nối MySQL
│   │   ├── room.c          # Quản lý phòng
│   │   ├── game.c          # Game logic
│   │   └── protocol_*.c    # Xử lý protocol
│   │
│   ├── gateway/            # WebSocket Gateway (Node.js)
│   │   ├── index.js        # Gateway chính
│   │   └── config.json     # Cấu hình gateway
│   │
│   ├── frontend/           # React Frontend
│   │   ├── src/
│   │   │   ├── pages/      # Các trang (Login, Lobby, GameRoom)
│   │   │   ├── components/ # Components React
│   │   │   └── services/   # API services
│   │   └── package.json
│   │
│   ├── database/
│   │   └── schema.sql      # Database schema
│   │
│   ├── data/
│   │   └── words.txt       # Danh sách từ để đoán
│   │
│   ├── common/
│   │   └── protocol.h      # Protocol definitions (shared)
│   │
│   ├── Makefile            # Build script cho server
│   └── docker-compose.yml  # Docker config cho MySQL
│
└── docs/                   # Tài liệu
```

---

## 🔧 Các lệnh hữu ích

### Build và Clean
```bash
cd src
make              # Build server
make clean        # Xóa các file build
make rebuild      # Clean và build lại
```

### Docker
```bash
cd src
docker compose up -d        # Khởi động database
docker compose down         # Dừng database
docker compose down -v      # Dừng và xóa data
```

### Kiểm tra Database
```bash
mysql --protocol=TCP -h 127.0.0.1 -P 3308 -u root -p
# Password: 123456
```

### Test (nếu có)
```bash
cd src
make test_client           # Build test client
./test_client login <username> <password>
./test_client register <username> <password>
```

---

## ⚙️ Cấu hình

### Gateway
File `src/gateway/config.json`:
- WebSocket port: **3000**
- TCP server: **localhost:8080**

### Database
File `src/docker-compose.yml`:
- MySQL port: **3308**
- Root password: **123456**
- Database name: **draw_guess**

### Frontend
File `src/frontend/vite.config.js`:
- Development server port: **5173** (mặc định)

---

## 🐛 Xử lý lỗi thường gặp

### Lỗi compile MySQL
```bash
# Đảm bảo đã cài dependencies
cd src
make install-deps
make clean
make
```

### Lỗi kết nối Database
```bash
# Kiểm tra Docker đang chạy
docker ps

# Khởi động lại database
cd src
docker compose down
docker compose up -d
```

### Lỗi port đã được sử dụng
- **Port 8080**: Đổi port trong server code hoặc dừng process đang dùng port
- **Port 3000**: Đổi trong `src/gateway/config.json`
- **Port 5173**: Vite sẽ tự động chọn port khác

### Lỗi Gateway không kết nối được Server
- Đảm bảo Server đã chạy trước Gateway
- Kiểm tra `src/gateway/config.json` có đúng host và port của server

---

## 📝 Ghi chú

- Server C sử dụng `select()` để xử lý multiple clients
- Protocol message format: `[TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]`
- Frontend sử dụng WebSocket để giao tiếp với Gateway
- Gateway chuyển đổi WebSocket messages sang TCP protocol của Server

---

## 👥 Tác giả

Dự án được phát triển cho học phần **Lập trình Mạng (IT4062)**.
[**Tạ Hồng Phúc**](https://github.com/andrew-taphuc)
[**Bùi Quang Hưng**](https://github.com/Gnuhq26)
[**Nguyễn Mạnh Tùng**](https://github.com/nmtun)
[**Nguyễn Văn Hiếu**](https://github.com/iamhieu213)

