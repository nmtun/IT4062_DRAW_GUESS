const WebSocket = require('ws');
const net = require('net');
const http = require('http');
const { 
    MessageBuffer, 
    Logger, 
    TcpConnectionManager, 
    MessageValidator, 
    PerformanceMonitor 
} = require('./utils');

class Gateway {
    constructor(wsPort = 3000, tcpHost = 'localhost', tcpPort = 8080) {
        this.wsPort = wsPort;
        this.tcpHost = tcpHost;
        this.tcpPort = tcpPort;
        this.wsServer = null;
        this.httpServer = null;
        this.clients = new Map(); // websocket -> {tcpClient, messageBuffer} mapping
        this.performanceMonitor = new PerformanceMonitor();
        this.tcpConnectionManager = new TcpConnectionManager(tcpHost, tcpPort);
    }

    start() {
        // Tạo HTTP server để phục vụ WebSocket
        this.httpServer = http.createServer();
        
        // Tạo WebSocket server
        this.wsServer = new WebSocket.Server({ 
            server: this.httpServer,
            perMessageDeflate: false
        });

        this.wsServer.on('connection', (ws, req) => {
            Logger.info(`WebSocket client connected from ${req.socket.remoteAddress}`);
            this.performanceMonitor.incrementConnectionsActive();
            this.handleWebSocketConnection(ws);
        });

        this.httpServer.listen(this.wsPort, () => {
            Logger.success(`Gateway WebSocket server listening on port ${this.wsPort}`);
            Logger.info(`Forwarding to TCP server at ${this.tcpHost}:${this.tcpPort}`);
        });
    }

    handleWebSocketConnection(ws) {
        let tcpClient = null;
        let isConnected = false;
        const messageBuffer = new MessageBuffer();

        // Kết nối đến TCP server với retry logic
        const connectToTcpServer = async () => {
            try {
                tcpClient = await this.tcpConnectionManager.connect();
                isConnected = true;

                tcpClient.on('data', (data) => {
                    try {
                        // Sử dụng message buffer để xử lý data có thể bị phân mảnh
                        const messages = messageBuffer.addData(data);
                        
                        messages.forEach(messageData => {
                            const message = this.parseTcpMessage(messageData);
                            if (ws.readyState === WebSocket.OPEN) {
                                ws.send(JSON.stringify(message));
                                this.performanceMonitor.incrementMessagesSent();
                            }
                        });
                    } catch (error) {
                        Logger.error('Error parsing TCP data:', error);
                        this.performanceMonitor.incrementErrors();
                    }
                });

                tcpClient.on('close', () => {
                    Logger.warn('TCP connection closed');
                    isConnected = false;
                    messageBuffer.clear();
                    if (ws.readyState === WebSocket.OPEN) {
                        ws.close(1011, 'TCP server disconnected');
                    }
                });

                tcpClient.on('error', (error) => {
                    Logger.error('TCP connection error:', error);
                    isConnected = false;
                    this.performanceMonitor.incrementErrors();
                    messageBuffer.clear();
                    if (ws.readyState === WebSocket.OPEN) {
                        ws.close(1011, 'TCP server error');
                    }
                });

            } catch (error) {
                Logger.error('Failed to connect to TCP server:', error);
                this.performanceMonitor.incrementErrors();
                if (ws.readyState === WebSocket.OPEN) {
                    ws.close(1011, 'Cannot connect to TCP server');
                }
            }
        };

        // Xử lý tin nhắn từ WebSocket client
        ws.on('message', async (data) => {
            try {
                const message = JSON.parse(data.toString());
                Logger.debug('Received WebSocket message:', message);
                this.performanceMonitor.incrementMessagesReceived();
                
                // Validate message
                MessageValidator.validateWebSocketMessage(message);
                
                if (!isConnected) {
                    await connectToTcpServer();
                }
                
                if (isConnected) {
                    this.forwardToTcpServer(tcpClient, message);
                } else {
                    Logger.warn('Cannot forward message - TCP not connected');
                    ws.send(JSON.stringify({
                        type: 'error',
                        data: { message: 'Server temporarily unavailable' }
                    }));
                }
            } catch (error) {
                Logger.error('Error handling WebSocket message:', error);
                this.performanceMonitor.incrementErrors();
                
                // Send error back to client
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({
                        type: 'error',
                        data: { message: error.message || 'Invalid message format' }
                    }));
                }
            }
        });

        ws.on('close', () => {
            Logger.info('WebSocket client disconnected');
            this.performanceMonitor.decrementConnectionsActive();
            if (tcpClient) {
                tcpClient.destroy();
            }
            this.clients.delete(ws);
        });

        ws.on('error', (error) => {
            Logger.error('WebSocket error:', error);
            this.performanceMonitor.incrementErrors();
            if (tcpClient) {
                tcpClient.destroy();
            }
        });

        this.clients.set(ws, { tcpClient, messageBuffer });
    }

    // Chuyển đổi JSON message từ WebSocket thành binary protocol cho TCP server
    forwardToTcpServer(tcpClient, message) {
        try {
            const binaryData = this.createTcpMessage(message);
            if (tcpClient && tcpClient.writable) {
                tcpClient.write(binaryData);
                Logger.debug('Forwarded to TCP server:', message.type);
            } else {
                throw new Error('TCP client not writable');
            }
        } catch (error) {
            Logger.error('Error forwarding to TCP server:', error);
            this.performanceMonitor.incrementErrors();
            throw error;
        }
    }

    // Tạo binary message theo protocol C server
    createTcpMessage(message) {
        const type = this.getMessageType(message.type);
        let payload = Buffer.alloc(0);

        switch (message.type) {
            case 'login':
                payload = this.createLoginPayload(message.data);
                break;
            case 'register':
                payload = this.createRegisterPayload(message.data);
                break;
            // import các case khác ở đây
            default:
                Logger.warn('Unknown message type:', message.type);
                return Buffer.alloc(0);
        }

        // Tạo header: [TYPE:1 byte][LENGTH:2 bytes][PAYLOAD:variable]
        const length = payload.length;
        const header = Buffer.alloc(3);
        header.writeUInt8(type, 0);
        header.writeUInt16BE(length, 1); // Big-endian

        return Buffer.concat([header, payload]);
    }

    // Parse binary message từ TCP server thành JSON
    parseTcpMessage(data) {
        if (data.length < 3) {
            throw new Error('Message too short');
        }

        const type = data.readUInt8(0);
        const length = data.readUInt16BE(1);
        
        if (data.length < 3 + length) {
            throw new Error('Incomplete message');
        }

        const payload = data.slice(3, 3 + length);
        const messageType = this.getMessageTypeName(type);
        
        let parsedData = {};
        
        switch (type) {
            case 0x02: // LOGIN_RESPONSE
                parsedData = this.parseLoginResponse(payload);
                break;
            case 0x04: // REGISTER_RESPONSE
                parsedData = this.parseRegisterResponse(payload);
                break;
            // import các case khác ở đây
            default:
                Logger.warn('Unknown message type from server:', type);
                parsedData = { raw: payload.toString('hex') };
        }

        return {
            type: messageType,
            data: parsedData
        };
    }

    // Message type mapping
    getMessageType(typeName) {
        const types = {
            'login': 0x01,
            'register': 0x03,
            // import các message khác ở đây
        };
        return types[typeName] || 0x00;
    }

    getMessageTypeName(type) {
        const types = {
            0x02: 'login_response',
            0x04: 'register_response',
            // import các message khác ở đây
        };
        return types[type] || 'unknown';
    }

    // Payload creators
    createLoginPayload(data) {
        const buffer = Buffer.alloc(64); // 32 + 32
        buffer.write(data.username || '', 0, 32, 'utf8');
        buffer.write(data.password || '', 32, 32, 'utf8');
        return buffer;
    }

    createRegisterPayload(data) {
        const buffer = Buffer.alloc(128); // 32 + 32 + 64
        buffer.write(data.username || '', 0, 32, 'utf8');
        buffer.write(data.password || '', 32, 32, 'utf8');
        return buffer;
    }

    // import các play load khác ở đây

    // Payload parsers
    parseLoginResponse(payload) {
        const status = payload.readUInt8(0);
        const userId = payload.readInt32BE(1);
        const username = payload.slice(5, 37).toString('utf8').replace(/\0/g, '');
        
        return {
            status: status === 0 ? 'success' : 'error',
            userId: userId,
            username: username
        };
    }

    parseRegisterResponse(payload) {
        const status = payload.readUInt8(0);
        const message = payload.slice(1, 129).toString('utf8').replace(/\0/g, '');
        
        return {
            status: status === 0 ? 'success' : 'error',
            message: message
        };
    }

    // parse response khác ở đây

    stop() {
        if (this.wsServer) {
            this.wsServer.close();
        }
        if (this.httpServer) {
            this.httpServer.close();
        }
        // Đóng tất cả TCP connections
        this.clients.forEach(({ tcpClient }) => {
            if (tcpClient) {
                tcpClient.destroy();
            }
        });
        this.clients.clear();
        
        Logger.info('Gateway stopped successfully');
    }
}

// Load configuration
let config;
try {
    config = require('./config.json');
} catch (error) {
    Logger.warn('Config file not found, using defaults');
    config = {
        gateway: {
            websocket: { port: 3000, host: "0.0.0.0" },
            tcp: { host: "localhost", port: 8080 }
        }
    };
}

// Command line arguments override config
const wsPort = process.argv[2] ? parseInt(process.argv[2]) : config.gateway.websocket.port;
const tcpHost = process.argv[3] || config.gateway.tcp.host;
const tcpPort = process.argv[4] ? parseInt(process.argv[4]) : config.gateway.tcp.port;

Logger.info('Starting gateway with configuration:', {
    websocketPort: wsPort,
    tcpHost: tcpHost,
    tcpPort: tcpPort
});

// Khởi động gateway
const gateway = new Gateway(wsPort, tcpHost, tcpPort);
gateway.start();

// Xử lý tín hiệu dừng
process.on('SIGINT', () => {
    Logger.info('\nShutting down gateway...');
    gateway.stop();
    process.exit(0);
});

process.on('SIGTERM', () => {
    Logger.info('\nShutting down gateway...');
    gateway.stop();
    process.exit(0);
});

// Xử lý uncaught exceptions
process.on('uncaughtException', (error) => {
    Logger.error('Uncaught Exception:', error);
    gateway.stop();
    process.exit(1);
});

process.on('unhandledRejection', (reason, promise) => {
    Logger.error('Unhandled Rejection at:', promise, 'reason:', reason);
});
