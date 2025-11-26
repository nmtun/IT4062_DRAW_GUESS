// WebSocket Gateway - Chuyển đổi WebSocket (browser) ↔ TCP Socket (C server)
const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 3001; // WebSocket server port
const TCP_HOST = 'localhost';
const TCP_PORT = 8080; // C server port

// Tạo WebSocket server
const wss = new WebSocket.Server({ port: WS_PORT });

console.log(`WebSocket Gateway đang chạy trên port ${WS_PORT}`);
console.log(`Kết nối đến TCP server: ${TCP_HOST}:${TCP_PORT}`);

wss.on('connection', function connection(ws) {
    console.log('\n========== NEW WEBSOCKET CONNECTION ==========');
    console.log('🔌 Client WebSocket đã kết nối');
    console.log('   Time:', new Date().toISOString());

    // Tạo TCP connection đến C server
    const tcpClient = new net.Socket();
    let isTcpConnected = false;

    // Kết nối đến TCP server
    console.log(`\n📡 Đang kết nối đến TCP server ${TCP_HOST}:${TCP_PORT}...`);
    
    tcpClient.on('error', function(error) {
        console.error('\n❌ ========== TCP CONNECTION ERROR ==========');
        console.error('❌ TCP connection error:', error.message);
        console.error('   Error code:', error.code);
        console.error('   Error syscall:', error.syscall);
        if (error.code === 'ECONNREFUSED') {
            console.error(`\n⚠️  ⚠️  ⚠️  KHÔNG THỂ KẾT NỐI ĐẾN SERVER C ⚠️  ⚠️  ⚠️`);
            console.error(`⚠️  Server C tại ${TCP_HOST}:${TCP_PORT} không phản hồi`);
            console.error('⚠️  Hãy đảm bảo server C đang chạy!');
            console.error('⚠️  Chạy lệnh sau trong terminal khác:');
            console.error('⚠️    cd src');
            console.error('⚠️    make');
            console.error('⚠️    ./main');
            console.error('⚠️  Sau đó thử lại đăng nhập\n');
        }
        isTcpConnected = false;
    });
    
    tcpClient.connect(TCP_PORT, TCP_HOST, function() {
        console.log('\n✅ ========== TCP CONNECTED ==========');
        console.log('✅ Đã kết nối đến TCP server');
        isTcpConnected = true;
    });

    // Nhận dữ liệu từ TCP server → gửi đến WebSocket client
    tcpClient.on('data', function(data) {
        console.log('\n📥 ========== RECEIVED FROM SERVER C ==========');
        console.log(`📥 TCP → WS: Nhận ${data.length} bytes từ server C`);
        if (data.length >= 3) {
            const type = data[0];
            const length = (data[1] << 8) | data[2];
            const typeNames = {
                0x01: 'LOGIN_REQUEST',
                0x02: 'LOGIN_RESPONSE',
                0x03: 'REGISTER_REQUEST',
                0x04: 'REGISTER_RESPONSE'
            };
            console.log(`   Message type: 0x${type.toString(16).padStart(2, '0')} (${typeNames[type] || 'UNKNOWN'})`);
            console.log(`   Payload length: ${length} bytes`);
            console.log(`   Total message: ${data.length} bytes`);
        }
        
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(data);
            console.log(`   ✅ Đã chuyển tiếp đến WebSocket client`);
        } else {
            console.warn('⚠️  WebSocket không mở (readyState=' + ws.readyState + '), bỏ qua data từ TCP');
        }
        console.log('==========================================\n');
    });

    // Queue để lưu message chờ gửi khi TCP chưa kết nối
    const messageQueue = [];
    
    // Hàm gửi message hoặc thêm vào queue
    function sendOrQueueMessage(message) {
        const buffer = Buffer.isBuffer(message) ? message : Buffer.from(message);
        const type = buffer[0];
        const length = (buffer[1] << 8) | buffer[2];
        
        const typeNames = {
            0x01: 'LOGIN_REQUEST',
            0x02: 'LOGIN_RESPONSE',
            0x03: 'REGISTER_REQUEST',
            0x04: 'REGISTER_RESPONSE'
        };
        
        console.log('\n📤 ========== SENDING TO SERVER C ==========');
        console.log(`📤 Nhận message từ WebSocket: ${buffer.length} bytes`);
        console.log(`   Type: 0x${type.toString(16).padStart(2, '0')} (${typeNames[type] || 'UNKNOWN'})`);
        console.log(`   Payload length: ${length} bytes`);
        console.log(`   TCP connected: ${isTcpConnected}`);
        console.log(`   TCP writable: ${tcpClient.writable}`);
        console.log(`   TCP destroyed: ${tcpClient.destroyed}`);
        
        if (isTcpConnected && tcpClient.writable) {
            const written = tcpClient.write(buffer);
            if (written) {
                console.log(`✅ Đã gửi ${buffer.length} bytes đến server C`);
                console.log('   Đang chờ response...');
            } else {
                console.warn(`⚠️  Buffer full, message queued`);
                messageQueue.push(buffer);
            }
        } else {
            console.warn(`\n⚠️  ⚠️  ⚠️  KHÔNG THỂ GỬI - TCP CHƯA KẾT NỐI ⚠️  ⚠️  ⚠️`);
            console.warn(`   isTcpConnected: ${isTcpConnected}`);
            console.warn(`   writable: ${tcpClient.writable}`);
            console.warn(`   destroyed: ${tcpClient.destroyed}`);
            console.warn(`   Message đã được thêm vào queue`);
            messageQueue.push(buffer);
        }
        console.log('==========================================\n');
    }
    
    // Khi TCP kết nối thành công, gửi tất cả message trong queue
    tcpClient.on('connect', function() {
        console.log('\n✅ ========== TCP CONNECTION ESTABLISHED ==========');
        console.log('✅ TCP connection established');
        console.log('   Local address:', tcpClient.localAddress + ':' + tcpClient.localPort);
        console.log('   Remote address:', tcpClient.remoteAddress + ':' + tcpClient.remotePort);
        isTcpConnected = true;
        
        // Gửi tất cả message đã queue
        if (messageQueue.length > 0) {
            console.log(`📤 Gửi ${messageQueue.length} message đã queue...`);
            messageQueue.forEach((buffer, index) => {
                const type = buffer[0];
                const length = (buffer[1] << 8) | buffer[2];
                console.log(`📤 WS → TCP [queued ${index + 1}]: ${buffer.length} bytes (type=0x${type.toString(16).padStart(2, '0')}, length=${length})`);
                const written = tcpClient.write(buffer);
                if (!written) {
                    console.warn(`⚠️  Buffer full, message ${index + 1} queued`);
                }
            });
            messageQueue.length = 0; // Clear queue
        }
    });
    
    // Nhận dữ liệu từ WebSocket client → gửi đến TCP server
    ws.on('message', function(message) {
        sendOrQueueMessage(message);
    });

    // Xử lý lỗi TCP (đã di chuyển lên trên)

    tcpClient.on('close', function() {
        console.log('🔌 TCP connection đã đóng');
        isTcpConnected = false;
        if (ws.readyState === WebSocket.OPEN) {
            ws.close();
        }
    });

    // Xử lý đóng WebSocket
    ws.on('close', function() {
        console.log('🔌 WebSocket connection đã đóng');
        if (tcpClient && !tcpClient.destroyed) {
            tcpClient.destroy();
        }
    });

    ws.on('error', function(error) {
        console.error('WebSocket error:', error);
        if (tcpClient && !tcpClient.destroyed) {
            tcpClient.destroy();
        }
    });
});

wss.on('error', function(error) {
    console.error('WebSocket Server error:', error);
});

console.log('Gateway sẵn sàng nhận kết nối...');

