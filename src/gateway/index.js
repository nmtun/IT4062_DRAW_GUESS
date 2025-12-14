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
                        Logger.debug(`[Gateway] Received ${data.length} bytes from TCP server`);
                        // Sử dụng message buffer để xử lý data có thể bị phân mảnh
                        const messages = messageBuffer.addData(data);
                        Logger.debug(`[Gateway] Parsed ${messages.length} complete message(s) from TCP data`);

                        messages.forEach(messageData => {
                            const message = this.parseTcpMessage(messageData);
                            Logger.info(`[Gateway] Sending message to WebSocket client: ${message.type}`, message);
                            
                            // Đặc biệt log draw_broadcast
                            if (message.type === 'draw_broadcast') {
                                Logger.info(`[Gateway] *** DRAW_BROADCAST detected! Sending to WebSocket client ***`);
                                Logger.info(`[Gateway] DRAW_BROADCAST data:`, JSON.stringify(message.data, null, 2));
                                Logger.info(`[Gateway] WebSocket readyState: ${ws.readyState}, client count: ${this.clients.size}`);
                            }
                            
                            if (ws.readyState === WebSocket.OPEN) {
                                const jsonMessage = JSON.stringify(message);
                                Logger.debug(`[Gateway] WebSocket readyState: ${ws.readyState}, sending JSON (${jsonMessage.length} bytes)`);
                                if (message.type === 'draw_broadcast') {
                                    Logger.info(`[Gateway] *** SENDING draw_broadcast JSON to WebSocket ***`);
                                    Logger.info(`[Gateway] JSON message: ${jsonMessage.substring(0, 200)}...`);
                                }
                                ws.send(jsonMessage);
                                this.performanceMonitor.incrementMessagesSent();
                                Logger.debug(`[Gateway] Message sent successfully`);
                                if (message.type === 'draw_broadcast') {
                                    Logger.info(`[Gateway] *** draw_broadcast sent successfully ***`);
                                }
                            } else {
                                Logger.warn(`[Gateway] WebSocket not open (readyState: ${ws.readyState}), cannot send message`);
                                if (message.type === 'draw_broadcast') {
                                    Logger.error(`[Gateway] *** CANNOT SEND draw_broadcast - WebSocket not open! ***`);
                                }
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
        Logger.debug(`Creating TCP message: type="${message.type}" -> 0x${type.toString(16)}`);
        let payload = Buffer.alloc(0);

        switch (message.type) {
            case 'login':
                payload = this.createLoginPayload(message.data);
                break;
            case 'register':
                payload = this.createRegisterPayload(message.data);
                break;
            case 'logout':
                payload = this.createLogoutPayload(message.data);
                break;
            case 'room_list':
                payload = this.createRoomListPayload(message.data);
                break;
            case 'create_room':
                payload = this.createCreateRoomPayload(message.data);
                break;
            case 'join_room':
                payload = this.createJoinRoomPayload(message.data);
                break;
            case 'leave_room':
                payload = this.createLeaveRoomPayload(message.data);
                break;
            case 'draw_data':
                Logger.info('[Gateway] Creating draw_data payload:', message.data);
                payload = this.createDrawDataPayload(message.data);
                Logger.info('[Gateway] Created draw_data payload, length:', payload.length);
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
            case 0x11: // ROOM_LIST_RESPONSE
                parsedData = this.parseRoomListResponse(payload);
                break;
            case 0x12: // CREATE_ROOM_RESPONSE
                parsedData = this.parseCreateRoomResponse(payload);
                break;
            case 0x13: // JOIN_ROOM_RESPONSE
                parsedData = this.parseJoinRoomResponse(payload);
                break;
            case 0x14: // LEAVE_ROOM_RESPONSE
                parsedData = this.parseLeaveRoomResponse(payload);
                break;
            case 0x15: // ROOM_UPDATE
                parsedData = this.parseRoomUpdate(payload);
                break;
            case 0x17: // ROOM_PLAYERS_UPDATE
                parsedData = this.parseRoomPlayersUpdate(payload);
                break;
            case 0x23: // DRAW_BROADCAST
                Logger.info('[Gateway] Received DRAW_BROADCAST, parsing...');
                parsedData = this.parseDrawBroadcast(payload);
                Logger.info('[Gateway] Parsed DRAW_BROADCAST:', parsedData);
                break;
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
            'logout': 0x05,
            'register': 0x03,
            'room_list': 0x10,
            'create_room': 0x12,
            'join_room': 0x13,
            'leave_room': 0x14,
            'draw_data': 0x22,
            // import các message khác ở đây
        };

        const result = types[typeName];
        if (!result) {
            Logger.warn(`getMessageType: Unknown message type: "${typeName}"`);
        }

        return result || 0x00;
    }

    getMessageTypeName(type) {
        const types = {
            0x02: 'login_response',
            0x04: 'register_response',
            0x11: 'room_list_response',
            0x12: 'create_room_response',
            0x13: 'join_room_response',
            0x14: 'leave_room_response',
            0x15: 'room_update',
            0x17: 'room_players_update',
            0x23: 'draw_broadcast',
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

    createLogoutPayload() {
        return Buffer.alloc(0);
    }

    createRoomListPayload() {
        return Buffer.alloc(0);
    }

    createCreateRoomPayload(data) {
        const buffer = Buffer.alloc(34); // 32 + 1 + 1
        buffer.write(data.room_name || '', 0, 32, 'utf8');
        buffer.writeUInt8(data.max_players || 2, 32);
        buffer.writeUInt8(data.rounds || 1, 33);
        return buffer;
    }

    createJoinRoomPayload(data) {
        const buffer = Buffer.alloc(4);
        buffer.writeInt32BE(data.room_id, 0);
        return buffer;
    }

    createLeaveRoomPayload(data) {
        const buffer = Buffer.alloc(4);
        buffer.writeInt32BE(data.room_id, 0);
        return buffer;
    }

    createDrawDataPayload(data) {
        // Protocol: [action:1][x1:2][y1:2][x2:2][y2:2][color:4][width:1] = 14 bytes
        const buffer = Buffer.alloc(14);
        buffer.writeUInt8(data.action || 1, 0); // 1=LINE, 2=CLEAR, 3=ERASE
        buffer.writeUInt16BE(data.x1 || 0, 1);
        buffer.writeUInt16BE(data.y1 || 0, 3);
        buffer.writeUInt16BE(data.x2 || 0, 5);
        buffer.writeUInt16BE(data.y2 || 0, 7);
        buffer.writeUInt32BE(data.color || 0, 9); // RGBA integer
        buffer.writeUInt8(data.width || 5, 13);
        return buffer;
    }

    // import các play load khác ở đây

    // Payload parsers
    parseLoginResponse(payload) {
        const status = payload.readUInt8(0);
        const userId = payload.readInt32BE(1);  // NodeJS handle signed automatically
        const username = payload.toString('utf8', 5, 37).replace(/\0/g, '');
        return {
            status: status === 0 ? 'success' : 'error',
            userId,
            username
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

    parseRoomListResponse(payload) {
        const roomCount = payload.readUInt16BE(0);
        const rooms = [];
        let offset = 2;

        for (let i = 0; i < roomCount; i++) {
            // Read room_id as UInt32BE and convert to signed if needed
            const room_id_raw = payload.readUInt32BE(offset);
            const room_id = room_id_raw > 0x7FFFFFFF ? room_id_raw - 0x100000000 : room_id_raw;

            const room_name = payload.slice(offset + 4, offset + 36).toString('utf8').replace(/\0/g, '');
            const player_count = payload.readUInt8(offset + 36);
            const max_players = payload.readUInt8(offset + 37);
            const state = payload.readUInt8(offset + 38);

            // Read owner_id similarly
            const owner_id_raw = payload.readUInt32BE(offset + 39);
            const owner_id = owner_id_raw > 0x7FFFFFFF ? owner_id_raw - 0x100000000 : owner_id_raw;

            rooms.push({ room_id, room_name, player_count, max_players, state, owner_id });
            offset += 43;
        }
        return { room_count: roomCount, rooms };
    }

    parseCreateRoomResponse(payload) {
        const status = payload.readUInt8(0);
        // C server sends htonl((uint32_t)room_id), so read as UInt32BE then convert to signed
        const room_id_raw = payload.readUInt32BE(1);
        const room_id = room_id_raw > 0x7FFFFFFF ? room_id_raw - 0x100000000 : room_id_raw;
        const message = payload.slice(5, 133).toString('utf8').replace(/\0/g, '');

        return { status: status === 0 ? 'success' : 'error', room_id, message };
    }

    parseJoinRoomResponse(payload) {
        const status = payload.readUInt8(0);
        // C server sends htonl((uint32_t)room_id), so read as UInt32BE then convert to signed
        const room_id_raw = payload.readUInt32BE(1);
        const room_id = room_id_raw > 0x7FFFFFFF ? room_id_raw - 0x100000000 : room_id_raw;
        const message = payload.slice(5, 133).toString('utf8').replace(/\0/g, '');
        return { status: status === 0 ? 'success' : 'error', room_id, message };
    }

    parseLeaveRoomResponse(payload) {
        const status = payload.readUInt8(0);
        const message = payload.slice(1, 129).toString('utf8').replace(/\0/g, '');
        return { status: status === 0 ? 'success' : 'error', message };
    }

    parseRoomUpdate(payload) {
        // room_info_protocol_t structure
        const room_id_raw = payload.readUInt32BE(0);
        const room_id = room_id_raw > 0x7FFFFFFF ? room_id_raw - 0x100000000 : room_id_raw;

        const room_name = payload.slice(4, 36).toString('utf8').replace(/\0/g, '');
        const player_count = payload.readUInt8(36);
        const max_players = payload.readUInt8(37);
        const state = payload.readUInt8(38);

        const owner_id_raw = payload.readUInt32BE(39);
        const owner_id = owner_id_raw > 0x7FFFFFFF ? owner_id_raw - 0x100000000 : owner_id_raw;

        return { room_id, room_name, player_count, max_players, state, owner_id };
    }

    parseRoomPlayersUpdate(payload) {
        let offset = 0;

        // room_players_update_t structure
        const room_id_raw = payload.readUInt32BE(offset);
        const room_id = room_id_raw > 0x7FFFFFFF ? room_id_raw - 0x100000000 : room_id_raw;
        offset += 4;

        const room_name = payload.slice(offset, offset + 32).toString('utf8').replace(/\0/g, '');
        offset += 32;

        const max_players = payload.readUInt8(offset++);
        const state = payload.readUInt8(offset++);

        const owner_id_raw = payload.readUInt32BE(offset);
        const owner_id = owner_id_raw > 0x7FFFFFFF ? owner_id_raw - 0x100000000 : owner_id_raw;
        offset += 4;

        const action = payload.readUInt8(offset++); // 0 = JOIN, 1 = LEAVE

        const changed_user_id_raw = payload.readUInt32BE(offset);
        const changed_user_id = changed_user_id_raw > 0x7FFFFFFF ? changed_user_id_raw - 0x100000000 : changed_user_id_raw;
        offset += 4;

        const changed_username = payload.slice(offset, offset + 32).toString('utf8').replace(/\0/g, '');
        offset += 32;

        const player_count = payload.readUInt16BE(offset);
        offset += 2;

        // Parse players list
        const players = [];
        for (let i = 0; i < player_count; i++) {
            const user_id_raw = payload.readUInt32BE(offset);
            const user_id = user_id_raw > 0x7FFFFFFF ? user_id_raw - 0x100000000 : user_id_raw;
            offset += 4;

            const username = payload.slice(offset, offset + 32).toString('utf8').replace(/\0/g, '');
            offset += 32;

            const is_owner = payload.readUInt8(offset++);

            players.push({ user_id, username, is_owner });
        }

        Logger.info('room infor and data players:', { room_id, room_name, max_players, state, owner_id, action, changed_user_id, changed_username, player_count, players });

        return {
            room_id,
            room_name,
            max_players,
            state,
            owner_id,
            action, // 0 = JOIN, 1 = LEAVE
            changed_user_id,
            changed_username,
            player_count,
            players
        };
    }

    parseDrawBroadcast(payload) {
        if (payload.length < 14) {
            Logger.warn('DRAW_BROADCAST payload too short:', payload.length);
            return { error: 'Invalid payload' };
        }

        const action = payload.readUInt8(0);
        const x1 = payload.readUInt16BE(1);
        const y1 = payload.readUInt16BE(3);
        const x2 = payload.readUInt16BE(5);
        const y2 = payload.readUInt16BE(7);
        const colorInt = payload.readUInt32BE(9);
        const width = payload.readUInt8(13);

        Logger.debug(`[Gateway] Parsed DRAW_BROADCAST: action=${action}, (${x1},${y1})->(${x2},${y2}), color=0x${colorInt.toString(16)}, width=${width}`);

        // Convert color integer to hex string (for reference, but we return colorInt for Canvas)
        const r = (colorInt >>> 24) & 0xFF;
        const g = (colorInt >>> 16) & 0xFF;
        const b = (colorInt >>> 8) & 0xFF;
        const colorHex = `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;

        return {
            action, // 1=LINE, 2=CLEAR, 3=ERASE
            x1,
            y1,
            x2,
            y2,
            color: colorInt, // Keep as integer for Canvas.jsx
            colorHex, // For reference
            width
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
