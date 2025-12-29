# Báo Cáo: Khắc Phục Vấn Đề Timeout Tự Động Trên VPS

## 📋 Tóm Tắt Vấn Đề

**Vấn đề:** Khi triển khai ứng dụng lên VPS, người dùng mới bị timeout tự động sau một thời gian ngắn mặc dù chưa đăng xuất.

**Nguyên nhân chính:**
1. Thiếu cơ chế keepalive/heartbeat để giữ kết nối sống
2. Firewall/NAT trên VPS đóng các kết nối idle
3. Timeout cấu hình quá ngắn
4. Không có cơ chế phát hiện và duy trì kết nối

---

## 🔍 Phân Tích Chi Tiết

### 1. Vấn Đề Với Kết Nối TCP/WebSocket Trên VPS

#### 1.1. Firewall và NAT Timeout
- **Vấn đề:** Firewall và NAT (Network Address Translation) trên VPS thường có timeout cho các kết nối idle
- **Cơ chế:** Khi không có traffic trong một khoảng thời gian (thường 30-120 giây), firewall/NAT sẽ đóng kết nối để giải phóng tài nguyên
- **Ảnh hưởng:** Client vẫn nghĩ kết nối còn sống, nhưng server không thể gửi dữ liệu

#### 1.2. TCP Connection Timeout
- **Vấn đề:** TCP connection không có keepalive sẽ bị đóng bởi OS sau một thời gian
- **Mặc định:** Hầu hết hệ điều hành có TCP timeout từ 2-4 giờ, nhưng firewall có thể đóng sớm hơn
- **Giải pháp:** Sử dụng SO_KEEPALIVE để OS tự động gửi keepalive packets

#### 1.3. WebSocket Connection Timeout
- **Vấn đề:** WebSocket connection có thể bị đóng bởi proxy, load balancer, hoặc reverse proxy
- **Cơ chế:** Nhiều proxy/load balancer có timeout mặc định cho WebSocket (thường 60-120 giây)
- **Giải pháp:** Sử dụng WebSocket ping/pong frames để giữ kết nối sống

---

## 🛠️ Giải Pháp Đã Triển Khai

### 2.1. SO_KEEPALIVE cho Server C

**File:** `src/server/server.c`

**Thay đổi:**
```c
// Thêm include
#include <netinet/tcp.h>

// Trong hàm server_add_client()
int keepalive = 1;
setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

// Cấu hình TCP keepalive parameters
int keepidle = 60;    // Bắt đầu keepalive sau 60 giây idle
int keepintvl = 10;   // Gửi probe mỗi 10 giây
int keepcnt = 3;      // Gửi 3 probes trước khi đóng
```

**Giải thích:**
- **SO_KEEPALIVE:** Bật tính năng keepalive cho socket
- **TCP_KEEPIDLE:** Thời gian idle trước khi bắt đầu gửi keepalive probes (60 giây)
- **TCP_KEEPINTVL:** Khoảng thời gian giữa các keepalive probes (10 giây)
- **TCP_KEEPCNT:** Số probes gửi trước khi đóng connection (3 probes)

**Cách hoạt động:**
1. Sau 60 giây không có traffic, OS bắt đầu gửi keepalive probes
2. Mỗi 10 giây, OS gửi một probe (packet rỗng với ACK flag)
3. Nếu nhận được response, kết nối vẫn sống
4. Nếu không nhận được response sau 3 probes (30 giây), OS đóng connection

**Lợi ích:**
- Tự động phát hiện kết nối bị đóng
- Giữ kết nối sống qua firewall/NAT
- Không cần code phức tạp, OS tự xử lý

---

### 2.2. WebSocket Ping/Pong cho Gateway

**File:** `src/gateway/index.js`

**Thay đổi:**
```javascript
// Thiết lập ping interval
let pingInterval = null;

const startPingInterval = () => {
    pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
            ws.ping();
        }
    }, 30000); // Ping mỗi 30 giây
};

// Xử lý pong response
ws.on('pong', () => {
    Logger.debug('Received WebSocket pong');
});
```

**Giải thích:**
- **WebSocket Ping:** Frame đặc biệt để kiểm tra kết nối còn sống
- **Interval:** Gửi ping mỗi 30 giây để giữ kết nối active
- **Pong Response:** Server tự động trả về pong khi nhận ping

**Cách hoạt động:**
1. Gateway gửi ping frame mỗi 30 giây
2. Browser/Client tự động trả về pong frame
3. Nếu không nhận pong, có thể kết nối đã bị đóng
4. Ping/pong frames không ảnh hưởng đến application data

**Lợi ích:**
- Giữ WebSocket connection sống qua proxy/load balancer
- Phát hiện kết nối bị đóng sớm
- Không tốn nhiều bandwidth (ping/pong frames rất nhỏ)

---

### 2.3. TCP Keepalive cho Gateway TCP Connection

**File:** `src/gateway/index.js`

**Thay đổi:**
```javascript
tcpClient.setKeepAlive(true, 60000);
```

**Giải thích:**
- **setKeepAlive(true, 60000):** Bật keepalive, bắt đầu sau 60 giây idle
- Áp dụng cho TCP connection giữa Gateway và Server C

**Cách hoạt động:**
- Tương tự SO_KEEPALIVE nhưng ở Node.js level
- Node.js sẽ gửi keepalive packets để giữ TCP connection sống

---

### 2.4. Tăng Message Timeout

**File:** `src/gateway/config.json`

**Thay đổi:**
```json
{
  "limits": {
    "messageTimeout": 300000  // Tăng từ 30000 (30s) lên 300000 (5 phút)
  }
}
```

**Giải thích:**
- **messageTimeout:** Thời gian timeout cho message processing
- Tăng lên 5 phút để tránh timeout không cần thiết khi user không hoạt động
- Vẫn đủ ngắn để phát hiện vấn đề thực sự

---

### 2.5. Giải Thích Về Keepalive Ở Cả Hai Phía

**Câu hỏi:** Có phải đang triển khai duy trì kết nối 2 lần, ở cả Server C và Gateway?

**Trả lời:** Đúng, nhưng đây là thiết kế hợp lý và không gây hại.

#### Kiến Trúc Kết Nối:

```
Client (Browser) ←→ Gateway (WebSocket) ←→ Server C (TCP)
```

Có **2 kết nối riêng biệt**:
1. **Client ↔ Gateway:** WebSocket connection
2. **Gateway ↔ Server C:** TCP connection

#### Keepalive Hiện Tại:

**1. WebSocket Connection (Client ↔ Gateway):**
- Gateway gửi WebSocket ping mỗi 30 giây
- **Cần thiết** để giữ WebSocket connection sống qua proxy/load balancer

**2. TCP Connection (Gateway ↔ Server C):**
- **Phía Gateway:** `tcpClient.setKeepAlive(true, 60000)` - Node.js level
- **Phía Server C:** `SO_KEEPALIVE` - OS level
- **Cả hai đều áp dụng cho cùng một TCP socket**, nhưng từ 2 phía khác nhau

#### Có Trùng Lặp Không?

**Có, nhưng không gây hại:**
- Gateway set `SO_KEEPALIVE` cho socket phía Gateway
- Server C set `SO_KEEPALIVE` cho socket phía Server C
- Cả hai đều có tác dụng giữ kết nối sống
- Đây là **best practice** trong network programming

#### Tại Sao Giữ Cả Hai?

**Lý do:**
1. **Redundancy (Dự phòng):** Nếu một phía không set keepalive, phía kia vẫn giữ kết nối sống
2. **Fault Tolerance (Chịu lỗi):** Nếu một phía có vấn đề, phía kia vẫn đảm bảo kết nối
3. **Best Practice:** Trong production, nên set keepalive ở cả hai phía để đảm bảo độ tin cậy

#### Có Thể Tối Ưu Không?

**Có thể bỏ một phía, nhưng không nên:**
- Nếu chỉ giữ ở Server C: Gateway có thể không phát hiện được connection bị đóng
- Nếu chỉ giữ ở Gateway: Server C có thể không phát hiện được connection bị đóng
- **Khuyến nghị:** Giữ cả hai để đảm bảo độ tin cậy tối đa

#### Kết Luận:

- ✅ **WebSocket ping/pong:** Cần thiết cho WebSocket connection
- ✅ **TCP keepalive ở Gateway:** Đảm bảo phía Gateway giữ kết nối
- ✅ **TCP keepalive ở Server C:** Đảm bảo phía Server C giữ kết nối
- ✅ **Giữ cả hai:** An toàn hơn cho production, không gây hại

**Tóm lại:** Có trùng lặp ở TCP keepalive, nhưng đây là thiết kế hợp lý và giúp tăng độ tin cậy của hệ thống.

---

## 📚 Kiến Thức Bổ Sung

### 3.1. TCP Keepalive - Chi Tiết Kỹ Thuật

#### Cơ Chế Hoạt Động:
1. **Idle Period:** Sau khoảng thời gian idle (TCP_KEEPIDLE), OS bắt đầu gửi keepalive probes
2. **Probe Interval:** Mỗi khoảng thời gian (TCP_KEEPINTVL), OS gửi một probe
3. **Probe Count:** Sau số probes nhất định (TCP_KEEPCNT) không có response, OS đóng connection

#### Keepalive Probe Packet:
- **Type:** TCP packet với ACK flag
- **Size:** Rất nhỏ (chỉ header, không có data)
- **Purpose:** Kiểm tra peer còn sống không

#### Tại Sao Cần Keepalive?
- **Phát hiện "half-open" connections:** Khi một bên bị crash/restart
- **Giữ connection sống qua NAT/Firewall:** NAT table entries có timeout
- **Tiết kiệm tài nguyên:** Đóng connections không còn sử dụng

---

### 3.2. WebSocket Ping/Pong - Chi Tiết Kỹ Thuật

#### WebSocket Frame Types:
- **Ping (0x9):** Control frame để kiểm tra kết nối
- **Pong (0xA):** Control frame để response ping
- **Text/Binary (0x1/0x2):** Data frames cho application data

#### Ping/Pong Flow:
```
Client                    Server
  |                         |
  |---- Ping Frame -------->|
  |                         | (Server tự động trả pong)
  |<--- Pong Frame ---------|
  |                         |
```

#### Lưu Ý:
- Ping/Pong là control frames, không phải data frames
- Browser tự động xử lý pong khi nhận ping
- Có thể gửi ping từ cả client và server
- Ping/Pong không ảnh hưởng đến application logic

---

### 3.3. Network Timeout Trên VPS

#### Các Loại Timeout:

1. **OS TCP Timeout:**
   - Mặc định: 2-4 giờ
   - Có thể cấu hình qua sysctl

2. **Firewall Timeout:**
   - Thường: 30-120 giây
   - Cấu hình trong iptables/ufw/firewalld

3. **NAT Timeout:**
   - Thường: 30-300 giây
   - Phụ thuộc vào NAT implementation

4. **Load Balancer/Proxy Timeout:**
   - Nginx: 60-120 giây mặc định
   - Apache: 60 giây mặc định
   - Có thể cấu hình trong config

#### Best Practices:
- **Keepalive interval < Firewall timeout:** Đảm bảo keepalive gửi trước khi firewall timeout
- **Ping interval < Proxy timeout:** Đảm bảo ping gửi trước khi proxy timeout
- **Monitor connection state:** Log và monitor để phát hiện vấn đề sớm

---

### 3.4. Debugging Connection Issues

#### Công Cụ Hữu Ích:

1. **netstat/ss:**
   ```bash
   netstat -an | grep ESTABLISHED
   ss -o state established
   ```

2. **tcpdump/wireshark:**
   ```bash
   tcpdump -i any -n 'tcp port 8080'
   ```

3. **strace:**
   ```bash
   strace -e trace=network -p <pid>
   ```

4. **Logs:**
   - Check gateway logs cho ping/pong
   - Check server logs cho keepalive
   - Check browser console cho WebSocket errors

#### Các Dấu Hiệu Vấn Đề:
- Connection bị đóng đột ngột
- Timeout errors trong logs
- Users báo mất kết nối
- High connection churn (nhiều connect/disconnect)

---

## ✅ Kết Quả Mong Đợi

Sau khi triển khai các thay đổi:

1. **Kết nối ổn định hơn:**
   - Không còn timeout tự động
   - Connection được giữ sống qua keepalive/ping

2. **Tương thích tốt với VPS:**
   - Hoạt động qua firewall/NAT
   - Tương thích với proxy/load balancer

3. **Phát hiện vấn đề sớm:**
   - Keepalive phát hiện "half-open" connections
   - Ping/pong phát hiện WebSocket issues

4. **Hiệu suất tốt:**
   - Keepalive packets rất nhỏ
   - Ping/pong không ảnh hưởng performance

---

## 🔄 Các Bước Triển Khai

1. **Rebuild Server C:**
   ```bash
   cd src
   make clean
   make
   ```

2. **Restart Gateway:**
   ```bash
   cd src/gateway
   npm install  # Nếu cần
   node index.js
   ```

3. **Test:**
   - Kết nối và để idle 5-10 phút
   - Kiểm tra connection vẫn còn sống
   - Kiểm tra logs cho ping/pong và keepalive

4. **Monitor:**
   - Theo dõi logs trong vài ngày
   - Kiểm tra không còn timeout issues
   - Monitor connection count và stability

---

## 📖 Tài Liệu Tham Khảo

- [TCP Keepalive - RFC 1122](https://tools.ietf.org/html/rfc1122)
- [WebSocket RFC 6455 - Ping/Pong](https://tools.ietf.org/html/rfc6455#section-5.5.2)
- [Linux TCP Keepalive](https://tldp.org/HOWTO/TCP-Keepalive-HOWTO/)
- [Node.js net.setKeepAlive()](https://nodejs.org/api/net.html#net_socket_setkeepalive_enable_initialdelay)

---

## 🎯 Tóm Tắt

Vấn đề timeout trên VPS được giải quyết bằng cách:
1. ✅ Thêm SO_KEEPALIVE cho TCP connections (Server C)
2. ✅ Thêm WebSocket ping/pong cho WebSocket connections (Gateway)
3. ✅ Thêm TCP keepalive cho Gateway TCP connections (Gateway)
4. ✅ Tăng messageTimeout để tránh timeout không cần thiết

**Lưu ý quan trọng:** 
- TCP keepalive được thiết lập ở cả Gateway và Server C cho cùng một TCP connection
- Đây là thiết kế hợp lý và không gây hại, giúp tăng độ tin cậy
- WebSocket ping/pong chỉ áp dụng cho WebSocket connection (Client ↔ Gateway)

Các giải pháp này đảm bảo kết nối được giữ sống ngay cả khi không có traffic, giúp ứng dụng hoạt động ổn định trên VPS với firewall/NAT/proxy.

