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
        let connectingPromise = null; // Promise để đợi quá trình kết nối hoàn tất
        const messageBuffer = new MessageBuffer();
        let pingInterval = null; // Interval cho WebSocket ping
        
        // Tạo TcpConnectionManager riêng cho mỗi WebSocket client
        const tcpConnectionManager = new TcpConnectionManager(this.tcpHost, this.tcpPort);
        
        // Thiết lập WebSocket ping để giữ kết nối sống (mỗi 30 giây)
        // Điều này rất quan trọng để tránh timeout trên VPS/proxy/load balancer
        const startPingInterval = () => {
            if (pingInterval) {
                clearInterval(pingInterval);
            }
            pingInterval = setInterval(() => {
                if (ws.readyState === WebSocket.OPEN) {
                    try {
                        ws.ping();
                        Logger.debug('Sent WebSocket ping to keep connection alive');
                    } catch (error) {
                        Logger.warn('Failed to send WebSocket ping:', error);
                    }
                }
            }, 30000); // Ping mỗi 30 giây
        };
        
        // Bắt đầu ping ngay khi kết nối
        startPingInterval();

        // Kết nối đến TCP server với retry logic
        const connectToTcpServer = async () => {
            // Nếu đang có quá trình kết nối đang diễn ra, đợi nó hoàn tất
            if (connectingPromise) {
                return await connectingPromise;
            }
            
            // Nếu đã kết nối rồi, không cần kết nối lại
            if (isConnected) {
                return;
            }
            
            // Tạo promise mới cho quá trình kết nối
            connectingPromise = (async () => {
                try {
                    tcpClient = await tcpConnectionManager.connect();
                    isConnected = true;
                    connectingPromise = null; // Reset promise sau khi kết nối thành công
                    
                    // Thiết lập TCP keepalive cho TCP connection
                    // Bắt đầu keepalive sau 60 giây idle, giúp giữ kết nối sống qua firewall/NAT
                    tcpClient.setKeepAlive(true, 60000);
                    Logger.debug('TCP keepalive enabled for connection');

                    tcpClient.on('data', (data) => {
                        try {
                            Logger.info(`[Gateway] Received raw TCP data: ${data.length} bytes, hex: ${data.toString('hex').substring(0, 100)}...`);
                            // Sử dụng message buffer để xử lý data có thể bị phân mảnh
                            const messages = messageBuffer.addData(data);
                            Logger.info(`[Gateway] Message buffer parsed ${messages.length} complete message(s)`);

                            messages.forEach((messageData, index) => {
                                Logger.info(`[Gateway] Parsing message ${index + 1}/${messages.length}, length: ${messageData.length}`);
                                const message = this.parseTcpMessage(messageData);
                                Logger.info(`[Gateway] Sending message to WebSocket client: ${message.type}`, message);
                                if (ws.readyState === WebSocket.OPEN) {
                                    ws.send(JSON.stringify(message));
                                    this.performanceMonitor.incrementMessagesSent();
                                }
                            });
                        } catch (error) {
                            Logger.error('Error parsing TCP data:', error);
                            Logger.error('Error stack:', error.stack);
                            this.performanceMonitor.incrementErrors();
                        }
                    });

                    tcpClient.on('close', () => {
                        Logger.warn('TCP connection closed');
                        isConnected = false;
                        connectingPromise = null; // Reset promise khi đóng kết nối
                        messageBuffer.clear();
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.close(1011, 'TCP server disconnected');
                        }
                    });

                    tcpClient.on('error', (error) => {
                        Logger.error('TCP connection error:', error);
                        isConnected = false;
                        connectingPromise = null; // Reset promise khi có lỗi
                        this.performanceMonitor.incrementErrors();
                        messageBuffer.clear();
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.close(1011, 'TCP server error');
                        }
                    });

                } catch (error) {
                    connectingPromise = null; // Reset promise khi có lỗi
                    Logger.error('Failed to connect to TCP server:', error);
                    this.performanceMonitor.incrementErrors();
                    if (ws.readyState === WebSocket.OPEN) {
                        ws.close(1011, 'Cannot connect to TCP server');
                    }
                    throw error; // Re-throw để các message đang đợi biết kết nối thất bại
                }
            })();
            
            return await connectingPromise;
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
            // Dọn dẹp ping interval
            if (pingInterval) {
                clearInterval(pingInterval);
                pingInterval = null;
            }
            if (tcpClient) {
                tcpClient.destroy();
            }
            this.clients.delete(ws);
        });

        ws.on('error', (error) => {
            Logger.error('WebSocket error:', error);
            this.performanceMonitor.incrementErrors();
            // Dọn dẹp ping interval
            if (pingInterval) {
                clearInterval(pingInterval);
                pingInterval = null;
            }
            if (tcpClient) {
                tcpClient.destroy();
            }
        });
        
        // Xử lý WebSocket pong để xác nhận connection còn sống
        ws.on('pong', () => {
            Logger.debug('Received WebSocket pong');
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
                payload = this.createDrawDataPayload(message.data);
                break;
            case 'start_game':
                payload = this.createStartGamePayload(message.data);
                break;
            case 'guess_word':
                payload = this.createGuessWordPayload(message.data);
                break;
            case 'chat_message':
                payload = this.createChatMessagePayload(message.data);
                break;
            case 'get_game_history':
                payload = this.createGetGameHistoryPayload(message.data);
                break;
            case 'change_password':
                payload = this.createChangePasswordPayload(message.data);
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
        Logger.info(`[Gateway] parseTcpMessage: data length=${data.length}`);
        if (data.length < 3) {
            throw new Error('Message too short');
        }

        const type = data.readUInt8(0);
        const length = data.readUInt16BE(1);
        Logger.info(`[Gateway] parseTcpMessage: type=0x${type.toString(16)}, payload_length=${length}, total_expected=${3 + length}`);

        if (data.length < 3 + length) {
            throw new Error(`Incomplete message: have ${data.length} bytes, need ${3 + length} bytes`);
        }

        const payload = data.slice(3, 3 + length);
        const messageType = this.getMessageTypeName(type);
        Logger.info(`[Gateway] parseTcpMessage: messageType="${messageType}", payload.length=${payload.length}`);

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
                parsedData = this.parseDrawBroadcast(payload);
                break;
            case 0x20: // GAME_START
                Logger.info(`[Gateway] Received GAME_START, payload length: ${payload.length}`);
                parsedData = this.parseGameStart(payload);
                if (parsedData.error) {
                    Logger.error(`[Gateway] Error parsing GAME_START: ${parsedData.error}`);
                }
                break;
            case 0x25: // CORRECT_GUESS
                parsedData = this.parseCorrectGuess(payload);
                break;
            case 0x26: // WRONG_GUESS
                parsedData = this.parseWrongGuess(payload);
                break;
            case 0x27: // ROUND_END
                parsedData = this.parseRoundEnd(payload);
                break;
            case 0x28: // GAME_END
                parsedData = this.parseGameEnd(payload);
                break;
            case 0x2A: // TIMER_UPDATE
                parsedData = this.parseTimerUpdate(payload);
                break;
            case 0x31: // CHAT_BROADCAST
                parsedData = this.parseChatBroadcast(payload);
                break;
            case 0x41: // GAME_HISTORY_RESPONSE
                parsedData = this.parseGameHistoryResponse(payload);
                break;
            case 0x50: // SERVER_SHUTDOWN
                parsedData = { message: 'Server đang tắt. Vui lòng đăng nhập lại sau.' };
                break;
            case 0x51: // ACCOUNT_LOGGED_IN_ELSEWHERE
                parsedData = { message: 'Tài khoản của bạn đang được đăng nhập ở nơi khác.' };
                break;
            case 0x07: // CHANGE_PASSWORD_RESPONSE
                parsedData = this.parseChangePasswordResponse(payload);
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
            'start_game': 0x16,
            'draw_data': 0x22,
            'guess_word': 0x24,
            'chat_message': 0x30,
            'get_game_history': 0x40,
            'change_password': 0x06,
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
            0x20: 'game_start',
            0x25: 'correct_guess',
            0x26: 'wrong_guess',
            0x27: 'round_end',
            0x28: 'game_end',
            0x2A: 'timer_update',
            0x23: 'draw_broadcast',
            0x41: 'game_history_response',
            0x31: 'chat_broadcast',
            0x50: 'server_shutdown',
            0x51: 'account_logged_in_elsewhere',
            0x07: 'change_password_response',
            // import các message khác ở đây
        };
        return types[type] || 'unknown';
    }

    // Payload creators
    createLoginPayload(data) {
        const buffer = Buffer.alloc(96); // 32 + 32 + 32 (username + password + avatar)
        buffer.write(data.username || '', 0, 32, 'utf8');
        buffer.write(data.password || '', 32, 32, 'utf8');
        buffer.write(data.avatar || 'avt1.jpg', 64, 32, 'utf8'); // Thêm avatar
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
        // create_room_request_t: room_name(32) + max_players(1) + rounds(1) + difficulty(16) = 50 bytes
        const buffer = Buffer.alloc(50);
        buffer.write(data.room_name || '', 0, 32, 'utf8');
        buffer.writeUInt8(data.max_players || 2, 32);
        buffer.writeUInt8(data.rounds || 1, 33);
        buffer.write(data.difficulty || 'easy', 34, 16, 'utf8');
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
        // Payload: action(1) + x1(2) + y1(2) + x2(2) + y2(2) + color(4) + width(1) = 14 bytes
        const buffer = Buffer.alloc(14);
        buffer.writeUInt8(data.action || 1, 0); // 1=LINE, 2=CLEAR, 3=ERASE
        buffer.writeUInt16BE(data.x1 || 0, 1);
        buffer.writeUInt16BE(data.y1 || 0, 3);
        buffer.writeUInt16BE(data.x2 || 0, 5);
        buffer.writeUInt16BE(data.y2 || 0, 7);
        buffer.writeUInt32BE(data.color || 0, 9);
        buffer.writeUInt8(data.width || 5, 13);
        return buffer;
    }

    createStartGamePayload() {
        return Buffer.alloc(0);
    }

    createGuessWordPayload(data) {
        const buffer = Buffer.alloc(64);
        buffer.write((data && data.word) ? String(data.word) : '', 0, 64, 'utf8');
        return buffer;
    }

    createChatMessagePayload(data) {
        const buffer = Buffer.alloc(256);
        buffer.write((data && data.message) ? String(data.message) : '', 0, 256, 'utf8');
        return buffer;
    }

    createGetGameHistoryPayload(data) {
        // No payload needed, server uses user_id from authenticated session
        return Buffer.alloc(0);
    }

    createChangePasswordPayload(data) {
        // Payload: old_password(32) + new_password(32) = 64 bytes
        const buffer = Buffer.alloc(64);
        buffer.write((data && data.old_password) ? String(data.old_password) : '', 0, 32, 'utf8');
        buffer.write((data && data.new_password) ? String(data.new_password) : '', 32, 32, 'utf8');
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

    parseChangePasswordResponse(payload) {
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

            // Read owner_username (32 bytes)
            const owner_username = payload.slice(offset + 43, offset + 75).toString('utf8').replace(/\0/g, '').trim();

            Logger.info('Parsed room:', { room_id, room_name, owner_id, owner_username });
            rooms.push({ room_id, room_name, player_count, max_players, state, owner_id, owner_username });
            offset += 75; // 4 + 32 + 1 + 1 + 1 + 4 + 32 = 75 bytes
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

        // Read owner_username (32 bytes)
        const owner_username = payload.slice(43, 75).toString('utf8').replace(/\0/g, '');

        return { room_id, room_name, player_count, max_players, state, owner_id, owner_username };
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

            const avatar = payload.slice(offset, offset + 32).toString('utf8').replace(/\0/g, '') || 'avt1.jpg';
            offset += 32;

            const is_owner = payload.readUInt8(offset++);
            const is_active = payload.readUInt8(offset++); // 1 = active, 0 = đang chờ, 255 = đã rời phòng

            players.push({ user_id, username, avatar, is_owner, is_active });
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
            Logger.warn('DRAW_BROADCAST payload too short');
            return { error: 'Invalid payload' };
        }

        const action = payload.readUInt8(0);
        const x1 = payload.readUInt16BE(1);
        const y1 = payload.readUInt16BE(3);
        const x2 = payload.readUInt16BE(5);
        const y2 = payload.readUInt16BE(7);
        const colorInt = payload.readUInt32BE(9);
        const width = payload.readUInt8(13);

        // Convert color integer to hex string
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
            color: colorInt,
            colorHex,
            width
        };
    }

    // --------------------------
    // Game payload parsers
    // --------------------------
    parseGameStart(payload) {
        // drawer_id(4) + word_length(1) + time_limit(2) + round_start_ms(8) 
        // + current_round(4) + player_count(1) + total_rounds(1) + word(64) + category(64) = 149 bytes
        Logger.info(`[Gateway] parseGameStart: payload length=${payload.length}, expected=149`);
        if (payload.length < 85) {
            Logger.warn(`[Gateway] GAME_START payload too short: ${payload.length} < 85`);
            Logger.warn(`[Gateway] Payload hex: ${payload.toString('hex').substring(0, 100)}...`);
            return { error: 'Invalid payload' };
        }
        const drawer_id_raw = payload.readUInt32BE(0);
        const drawer_id = drawer_id_raw > 0x7FFFFFFF ? drawer_id_raw - 0x100000000 : drawer_id_raw;
        const word_length = payload.readUInt8(4);
        const time_limit = payload.readUInt16BE(5);
        let round_start_ms = 0;
        try {
            round_start_ms = Number(payload.readBigUInt64BE(7));
        } catch (_) {
            // fallback (older node)
            const hi = payload.readUInt32BE(7);
            const lo = payload.readUInt32BE(11);
            round_start_ms = hi * 4294967296 + lo;
        }
        const current_round_raw = payload.readUInt32BE(15);
        const current_round = current_round_raw > 0x7FFFFFFF ? current_round_raw - 0x100000000 : current_round_raw;
        const player_count = payload.readUInt8(19);
        const total_rounds = payload.readUInt8(20);
        const word = payload.slice(21, 85).toString('utf8').replace(/\0/g, '');
        // Parse category (64 bytes starting at offset 85)
        let category = '';
        if (payload.length >= 149) {
            category = payload.slice(85, 149).toString('utf8').replace(/\0/g, '');
        }
        Logger.info(`[Gateway] Parsed GAME_START: current_round=${current_round}, player_count=${player_count}, total_rounds=${total_rounds}, category=${category}`);
        return { drawer_id, word_length, time_limit, round_start_ms, current_round, player_count, total_rounds, word, category };
    }

    parseTimerUpdate(payload) {
        // Payload: time_left(2 bytes) - thời gian còn lại tính bằng giây
        if (payload.length < 2) {
            Logger.warn('[Gateway] TIMER_UPDATE payload too short');
            return { error: 'Invalid payload' };
        }
        const time_left = payload.readUInt16BE(0);
        return { time_left };
    }

    parseCorrectGuess(payload) {
        // player_id(4) + word(64) + guesser_points(2) + drawer_points(2) + username(32) = 104 bytes
        if (payload.length < 104) {
            // Fallback cho payload cũ (72 bytes) nếu server chưa update
            if (payload.length >= 72) {
                const player_id_raw = payload.readUInt32BE(0);
                const player_id = player_id_raw > 0x7FFFFFFF ? player_id_raw - 0x100000000 : player_id_raw;
                const word = payload.slice(4, 68).toString('utf8').replace(/\0/g, '');
                const guesser_points = payload.readUInt16BE(68);
                const drawer_points = payload.readUInt16BE(70);
                return { player_id, word, points: guesser_points, guesser_points, drawer_points, username: null };
            }
            Logger.warn('CORRECT_GUESS payload too short');
            return { error: 'Invalid payload' };
        }
        const player_id_raw = payload.readUInt32BE(0);
        const player_id = player_id_raw > 0x7FFFFFFF ? player_id_raw - 0x100000000 : player_id_raw;
        const word = payload.slice(4, 68).toString('utf8').replace(/\0/g, '');
        const guesser_points = payload.readUInt16BE(68);
        const drawer_points = payload.readUInt16BE(70);
        const username = payload.slice(72, 104).toString('utf8').replace(/\0/g, '').trim();
        Logger.info(`[Gateway] Parsed CORRECT_GUESS: player_id=${player_id}, username="${username}", points=${guesser_points}`);
        return { player_id, word, points: guesser_points, guesser_points, drawer_points, username: username || null };
    }

    parseWrongGuess(payload) {
        // player_id(4) + guess(64) = 68 bytes
        if (payload.length < 68) {
            Logger.warn('WRONG_GUESS payload too short');
            return { error: 'Invalid payload' };
        }
        const player_id_raw = payload.readUInt32BE(0);
        const player_id = player_id_raw > 0x7FFFFFFF ? player_id_raw - 0x100000000 : player_id_raw;
        const guess = payload.slice(4, 68).toString('utf8').replace(/\0/g, '');
        return { player_id, guess };
    }

    parseRoundEnd(payload) {
        // word(64) + score_count(2) + pairs(user_id(4), score(4))...
        if (payload.length < 66) {
            Logger.warn('ROUND_END payload too short');
            return { error: 'Invalid payload' };
        }
        const word = payload.slice(0, 64).toString('utf8').replace(/\0/g, '');
        const score_count = payload.readUInt16BE(64);
        let offset = 66;
        const scores = [];
        for (let i = 0; i < score_count; i++) {
            if (offset + 8 > payload.length) break;
            const uid_raw = payload.readUInt32BE(offset);
            const user_id = uid_raw > 0x7FFFFFFF ? uid_raw - 0x100000000 : uid_raw;
            const score_raw = payload.readUInt32BE(offset + 4);
            const score = score_raw > 0x7FFFFFFF ? score_raw - 0x100000000 : score_raw;
            scores.push({ user_id, score });
            offset += 8;
        }
        return { word, score_count, scores };
    }

    parseGameEnd(payload) {
        // winner_id(4) + score_count(2) + pairs(user_id(4), score(4))...
        if (payload.length < 6) {
            Logger.warn('GAME_END payload too short');
            return { error: 'Invalid payload' };
        }
        const winner_id_raw = payload.readUInt32BE(0);
        const winner_id = winner_id_raw > 0x7FFFFFFF ? winner_id_raw - 0x100000000 : winner_id_raw;
        const score_count = payload.readUInt16BE(4);
        let offset = 6;
        const scores = [];
        for (let i = 0; i < score_count; i++) {
            if (offset + 8 > payload.length) break;
            const uid_raw = payload.readUInt32BE(offset);
            const user_id = uid_raw > 0x7FFFFFFF ? uid_raw - 0x100000000 : uid_raw;
            const score_raw = payload.readUInt32BE(offset + 4);
            const score = score_raw > 0x7FFFFFFF ? score_raw - 0x100000000 : score_raw;
            scores.push({ user_id, score });
            offset += 8;
        }
        return { winner_id, score_count, scores };
    }

    parseChatBroadcast(payload) {
        // username(32) + message(256) + timestamp(8)
        if (payload.length < 296) {
            Logger.warn('CHAT_BROADCAST payload too short');
            return { error: 'Invalid payload' };
        }
        const username = payload.slice(0, 32).toString('utf8').replace(/\0/g, '');
        const message = payload.slice(32, 288).toString('utf8').replace(/\0/g, '');
        let timestamp = 0;
        try {
            timestamp = Number(payload.readBigUInt64BE(288));
        } catch (_) {
            const hi = payload.readUInt32BE(288);
            const lo = payload.readUInt32BE(292);
            timestamp = hi * 4294967296 + lo;
        }
        return { username, message, timestamp };
    }

    parseGameHistoryResponse(payload) {
        // count(2) + entries (score(4) + rank(4) + finished_at(32)) * count
        if (payload.length < 2) {
            Logger.warn('GAME_HISTORY_RESPONSE payload too short');
            return { error: 'Invalid payload', history: [] };
        }
        
        const count = payload.readUInt16BE(0);
        const history = [];
        let offset = 2;
        
        for (let i = 0; i < count; i++) {
            if (offset + 40 > payload.length) break;
            
            // Read score (4 bytes, signed int32)
            const score_raw = payload.readUInt32BE(offset);
            const score = score_raw > 0x7FFFFFFF ? score_raw - 0x100000000 : score_raw;
            
            // Read rank (4 bytes, signed int32)
            const rank_raw = payload.readUInt32BE(offset + 4);
            const rank = rank_raw > 0x7FFFFFFF ? rank_raw - 0x100000000 : rank_raw;
            
            // Read finished_at (32 bytes, null-terminated string)
            const finished_at = payload.slice(offset + 8, offset + 40).toString('utf8').replace(/\0/g, '');
            
            history.push({ score, rank, finished_at });
            offset += 40;
        }
        
        Logger.info(`[Gateway] Parsed game history: ${count} entries`);
        return { history };
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
