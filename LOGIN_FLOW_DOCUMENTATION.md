# Luồng Hoạt Động Chức Năng LOGIN

## Tổng Quan
Khi client gửi LOGIN_REQUEST message đến server, đây là luồng xử lý chi tiết:

```
Client → Socket → server_handle_client_data() → protocol_parse_message() 
→ protocol_handle_message() → protocol_handle_login() → db_authenticate_user() 
→ protocol_send_login_response() → Client
```

---

## Chi Tiết Từng Bước

### **BƯỚC 1: Nhận Dữ Liệu Từ Socket**
**File:** `src/server/server.c`  
**Hàm:** `server_handle_client_data(server_t *server, int client_index)`

**Vị trí code:**
```123:140:src/server/server.c
// Xử lý dữ liệu từ client
void server_handle_client_data(server_t *server, int client_index) {
    uint8_t buffer[BUFFER_SIZE];
    int client_fd = server->clients[client_index].fd;
    
    ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
    
    if (bytes_read <= 0) {
        // Lỗi hoặc kết nối đóng
        server_handle_disconnect(server, client_index);
        return;
    }
    
    // Parse message
    message_t msg;
    if (protocol_parse_message(buffer, bytes_read, &msg) == 0) {
        // Xử lý message
        protocol_handle_message(server, client_index, &msg);
```

**Chức năng:**
- Nhận raw bytes từ socket qua `recv()`
- Lưu vào buffer `uint8_t buffer[BUFFER_SIZE]`
- Gọi `protocol_parse_message()` để parse message

---

### **BƯỚC 2: Parse Message**
**File:** `src/server/protocol.c`  
**Hàm:** `protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out)`

**Vị trí code:**
```18:51:src/server/protocol.c
int protocol_parse_message(const uint8_t* buffer, size_t buffer_len, message_t* msg_out) {
    if (!buffer || !msg_out || buffer_len < 3) {
        return -1;
    }

    // Đọc type (1 byte)
    msg_out->type = buffer[0];

    // Đọc length (2 bytes, network byte order)
    uint16_t length_network;
    memcpy(&length_network, buffer + 1, 2);
    msg_out->length = ntohs(length_network);

    // Kiểm tra độ dài hợp lệ
    if (msg_out->length > buffer_len - 3) {
        fprintf(stderr, "Lỗi: Payload length (%d) vượt quá buffer còn lại (%zu)\n", 
                msg_out->length, buffer_len - 3);
        return -1;
    }

    // Cấp phát bộ nhớ cho payload
    if (msg_out->length > 0) {
        msg_out->payload = (uint8_t*)malloc(msg_out->length);
        if (!msg_out->payload) {
            fprintf(stderr, "Lỗi: Không thể cấp phát bộ nhớ cho payload\n");
            return -1;
        }
        memcpy(msg_out->payload, buffer + 3, msg_out->length);
    } else {
        msg_out->payload = NULL;
    }

    return 0;
}
```

**Chức năng:**
- Đọc message type (byte đầu tiên) → `msg_out->type = 0x01` (LOGIN_REQUEST)
- Đọc length (2 bytes tiếp theo, network byte order)
- Cấp phát bộ nhớ và copy payload vào `msg_out->payload`
- Trả về `message_t` struct đã được parse

**Format message:**
```
[0x01][LENGTH:2 bytes][PAYLOAD:username(32) + password(32)]
```

---

### **BƯỚC 3: Router Message (Switch Case)**
**File:** `src/server/protocol.c`  
**Hàm:** `protocol_handle_message(server_t* server, int client_index, const message_t* msg)`

**Vị trí code:**
```295:322:src/server/protocol.c
int protocol_handle_message(server_t* server, int client_index, const message_t* msg) {
    if (!server || !msg) {
        return -1;
    }

    switch (msg->type) {
        case MSG_LOGIN_REQUEST:
            return protocol_handle_login(server, client_index, msg);
            
        case MSG_REGISTER_REQUEST:
            return protocol_handle_register(server, client_index, msg);
            
        case MSG_LOGOUT:
            // TODO: Implement logout handler
            printf("Nhận LOGOUT từ client %d\n", client_index);
            return 0;
            
        default:
            fprintf(stderr, "Unknown message type: 0x%02X từ client %d\n", 
                    msg->type, client_index);
            return -1;
    }
}
```

**Chức năng:**
- Kiểm tra `msg->type`
- Nếu là `MSG_LOGIN_REQUEST` (0x01) → gọi `protocol_handle_login()`
- Nếu là message type khác → xử lý tương ứng hoặc báo lỗi

---

### **BƯỚC 4: Xử Lý Login Request**
**File:** `src/server/protocol.c`  
**Hàm:** `protocol_handle_login(server_t* server, int client_index, const message_t* msg)`

**Vị trí code:**
```147:209:src/server/protocol.c
static int protocol_handle_login(server_t* server, int client_index, const message_t* msg) {
    if (!server || client_index < 0 || client_index >= MAX_CLIENTS || !msg) {
        return -1;
    }

    client_t* client = &server->clients[client_index];
    if (!client->active) {
        return -1;
    }

    // Kiểm tra payload size
    if (msg->length < sizeof(login_request_t)) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Parse payload
    login_request_t* req = (login_request_t*)msg->payload;
    
    // Đảm bảo null-terminated
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    strncpy(username, req->username, MAX_USERNAME_LEN - 1);
    username[MAX_USERNAME_LEN - 1] = '\0';
    strncpy(password, req->password, MAX_PASSWORD_LEN - 1);
    password[MAX_PASSWORD_LEN - 1] = '\0';

    printf("Nhận LOGIN_REQUEST từ client %d: username=%s\n", client_index, username);

    // Kiểm tra database connection
    if (!db) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Hash password
    char password_hash[65];
    if (auth_hash_password(password, password_hash) != 0) {
        protocol_send_login_response(client->fd, STATUS_ERROR, -1, "");
        return -1;
    }

    // Xác thực user
    int user_id = db_authenticate_user(db, username, password_hash);
    
    if (user_id > 0) {
        // Đăng nhập thành công
        client->user_id = user_id;
        strncpy(client->username, username, sizeof(client->username) - 1);
        client->username[sizeof(client->username) - 1] = '\0';
        client->state = CLIENT_STATE_LOGGED_IN;
        
        protocol_send_login_response(client->fd, STATUS_SUCCESS, user_id, username);
        printf("Client %d đăng nhập thành công: user_id=%d, username=%s\n", 
               client_index, user_id, username);
        return 0;
    } else {
        // Đăng nhập thất bại
        protocol_send_login_response(client->fd, STATUS_AUTH_FAILED, -1, "");
        printf("Client %d đăng nhập thất bại: username=%s\n", client_index, username);
        return -1;
    }
}
```

**Các bước xử lý:**

1. **Validate input:**
   - Kiểm tra server, client_index, msg hợp lệ
   - Kiểm tra payload size >= `sizeof(login_request_t)`

2. **Parse payload:**
   - Cast payload thành `login_request_t*`
   - Extract `username` và `password` (đảm bảo null-terminated)

3. **Hash password:**
   - Gọi `auth_hash_password()` (file: `src/server/auth.c`)
   - Hash password bằng SHA256

4. **Xác thực user:**
   - Gọi `db_authenticate_user()` (file: `src/server/database.c`)
   - Query database để kiểm tra username + password_hash
   - Trả về `user_id` nếu thành công, `-1` nếu thất bại

5. **Cập nhật client state:**
   - Nếu thành công:
     - `client->user_id = user_id`
     - `client->username = username`
     - `client->state = CLIENT_STATE_LOGGED_IN`
   - Gọi `protocol_send_login_response()` để gửi response

---

### **BƯỚC 5: Gửi Login Response**
**File:** `src/server/protocol.c`  
**Hàm:** `protocol_send_login_response(int client_fd, uint8_t status, int32_t user_id, const char* username)`

**Vị trí code:**
```105:120:src/server/protocol.c
int protocol_send_login_response(int client_fd, uint8_t status, int32_t user_id, const char* username) {
    login_response_t response;
    memset(&response, 0, sizeof(response));
    
    response.status = status;
    response.user_id = (int32_t)htonl((uint32_t)user_id);  // Convert to network byte order
    
    if (username) {
        strncpy(response.username, username, MAX_USERNAME_LEN - 1);
        response.username[MAX_USERNAME_LEN - 1] = '\0';
    }

    return protocol_send_message(client_fd, MSG_LOGIN_RESPONSE, 
                                (uint8_t*)&response, sizeof(response));
}
```

**Chức năng:**
- Tạo `login_response_t` struct với:
  - `status`: STATUS_SUCCESS (0x00) hoặc STATUS_AUTH_FAILED (0x05)
  - `user_id`: user_id (network byte order) hoặc -1
  - `username`: username string
- Gọi `protocol_send_message()` để gửi

---

### **BƯỚC 6: Gửi Message Qua Socket**
**File:** `src/server/protocol.c`  
**Hàm:** `protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len)`

**Vị trí code:**
```80:104:src/server/protocol.c
int protocol_send_message(int client_fd, uint8_t type, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[BUFFER_SIZE];
    
    if (3 + payload_len > BUFFER_SIZE) {
        fprintf(stderr, "Lỗi: Message quá lớn (%d bytes)\n", 3 + payload_len);
        return -1;
    }

    int msg_len = protocol_create_message(type, payload, payload_len, buffer);
    if (msg_len < 0) {
        return -1;
    }

    ssize_t sent = send(client_fd, buffer, msg_len, 0);
    if (sent < 0) {
        perror("send() failed");
        return -1;
    }

    if (sent != msg_len) {
        fprintf(stderr, "Cảnh báo: Chỉ gửi được %zd/%d bytes\n", sent, msg_len);
        return -1;
    }

    return 0;
}
```

**Chức năng:**
- Tạo message theo format: `[TYPE][LENGTH][PAYLOAD]`
- Gửi qua socket bằng `send()`
- Trả về 0 nếu thành công, -1 nếu lỗi

---

## Sơ Đồ Luồng Hoạt Động

```
┌─────────┐
│ Client  │
└────┬────┘
     │ Gửi LOGIN_REQUEST
     │ [0x01][LENGTH][username(32) + password(32)]
     ▼
┌─────────────────────────────────────┐
│ server_handle_client_data()         │
│ File: server.c                      │
│ - recv() từ socket                  │
│ - Gọi protocol_parse_message()      │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ protocol_parse_message()            │
│ File: protocol.c                    │
│ - Parse type, length, payload       │
│ - Trả về message_t struct           │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ protocol_handle_message()           │
│ File: protocol.c                   │
│ - Switch case theo msg->type       │
│ - Nếu 0x01 → gọi protocol_handle_login() │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ protocol_handle_login()             │
│ File: protocol.c                   │
│ 1. Parse username, password        │
│ 2. auth_hash_password()            │
│ 3. db_authenticate_user()          │
│ 4. Cập nhật client state            │
│ 5. Gọi protocol_send_login_response() │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ protocol_send_login_response()      │
│ File: protocol.c                    │
│ - Tạo login_response_t struct       │
│ - Gọi protocol_send_message()       │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│ protocol_send_message()             │
│ File: protocol.c                    │
│ - Tạo message format                │
│ - send() qua socket                 │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────┐
│ Client  │ Nhận LOGIN_RESPONSE
│         │ [0x02][LENGTH][status + user_id + username]
└─────────┘
```

---

## Tóm Tắt

| Bước | File | Hàm | Chức năng |
|------|------|-----|-----------|
| 1 | `server.c` | `server_handle_client_data()` | Nhận raw bytes từ socket |
| 2 | `protocol.c` | `protocol_parse_message()` | Parse message thành struct |
| 3 | `protocol.c` | `protocol_handle_message()` | Router: switch case theo type |
| 4 | `protocol.c` | `protocol_handle_login()` | Xử lý login logic |
| 4a | `auth.c` | `auth_hash_password()` | Hash password |
| 4b | `database.c` | `db_authenticate_user()` | Query database |
| 5 | `protocol.c` | `protocol_send_login_response()` | Tạo response struct |
| 6 | `protocol.c` | `protocol_send_message()` | Gửi qua socket |

---

## Lưu Ý Quan Trọng

1. **Memory Management:**
   - `protocol_parse_message()` cấp phát `msg->payload` bằng `malloc()`
   - Phải `free(msg->payload)` sau khi xử lý xong (đã làm trong `server_handle_client_data()`)

2. **Network Byte Order:**
   - Length và user_id được convert sang network byte order (big-endian)
   - Dùng `htons()`, `ntohs()`, `htonl()`, `ntohl()`

3. **Error Handling:**
   - Mỗi bước đều có kiểm tra lỗi và trả về response phù hợp
   - Client luôn nhận được response (thành công hoặc thất bại)

