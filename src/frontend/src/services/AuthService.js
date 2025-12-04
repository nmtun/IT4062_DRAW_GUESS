/**
 * AuthService - Xử lý authentication thông qua WebSocket Gateway
 */
class AuthService {
    constructor(gatewayUrl = 'ws://localhost:3000') {
        this.gatewayUrl = gatewayUrl;
        this.ws = null;
        this.isConnected = false;
        this.isConnecting = false;
        this.isDisconnecting = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.callbacks = new Map(); // Map<messageType, Set<callback>>
        this.messageQueue = [];
        this.connectionPromise = null;
        this.reconnectTimer = null;
    }

    /**
     * Kết nối đến WebSocket Gateway
     */
    connect() {
        // Nếu đang kết nối hoặc đã kết nối, return existing promise
        if (this.connectionPromise) {
            return this.connectionPromise;
        }

        // Nếu đã kết nối thành công
        if (this.isConnected && this.ws && this.ws.readyState === WebSocket.OPEN) {
            return Promise.resolve(true);
        }

        // Nếu đang disconnect, không cho phép kết nối mới
        if (this.isDisconnecting) {
            return Promise.reject(new Error('Currently disconnecting'));
        }

        this.isConnecting = true;
        this.connectionPromise = new Promise((resolve, reject) => {
            try {
                console.log(`Connecting to gateway at ${this.gatewayUrl}`);
                this.ws = new WebSocket(this.gatewayUrl);
                
                this.ws.onopen = () => {
                    console.log('Connected to gateway successfully');
                    this.isConnected = true;
                    this.isConnecting = false;
                    this.reconnectAttempts = 0;
                    this.connectionPromise = null;
                    
                    // Clear any existing reconnect timer
                    if (this.reconnectTimer) {
                        clearTimeout(this.reconnectTimer);
                        this.reconnectTimer = null;
                    }
                    
                    // Gửi các message đã được queue
                    this.processMessageQueue();
                    
                    resolve(true);
                };

                this.ws.onmessage = (event) => {
                    this.handleMessage(event);
                };

                this.ws.onclose = (event) => {
                    console.log('Gateway connection closed:', event.code, event.reason);
                    this.isConnected = false;
                    this.isConnecting = false;
                    this.connectionPromise = null;
                    
                    // Chỉ reconnect nếu không phải do user đóng kết nối và không đang disconnect
                    if (event.code !== 1000 && !this.isDisconnecting) {
                        this.attemptReconnect();
                    }
                };

                this.ws.onerror = (error) => {
                    // Network errors during handshake are common while server boots.
                    // Use warn to avoid scary red logs; onclose will drive reconnection.
                    console.warn('Gateway connection error:', error);
                    this.isConnecting = false;
                    this.connectionPromise = null;
                    try {
                        if (this.ws && (this.ws.readyState === WebSocket.CONNECTING)) {
                            // Avoid closing while CONNECTING to prevent browser error spam
                            const wsRef = this.ws;
                            wsRef.addEventListener('open', () => {
                                try { wsRef.close(1006, 'Error during connect'); } catch (_) {}
                            }, { once: true });
                        }
                    } catch (_) {}
                    reject(error);
                };

                // Timeout cho connection
                setTimeout(() => {
                    if (this.ws && this.ws.readyState !== WebSocket.OPEN) {
                        this.isConnecting = false;
                        this.ws.close();
                        this.connectionPromise = null;
                        reject(new Error('Connection timeout'));
                    }
                }, 10000);

            } catch (error) {
                console.error('Failed to create WebSocket connection:', error);
                this.isConnecting = false;
                this.connectionPromise = null;
                reject(error);
            }
        });

        return this.connectionPromise;
    }

    /**
     * Xử lý message từ server
     */
    handleMessage(event) {
        try {
            const message = JSON.parse(event.data);
            console.log('Received message:', message);
            
            // Trigger callbacks nếu có
            if (this.callbacks.has(message.type)) {
                const callbackSet = this.callbacks.get(message.type);
                if (callbackSet instanceof Set) {
                    callbackSet.forEach(callback => {
                        try {
                            callback(message.data);
                        } catch (err) {
                            console.error('Callback error:', err);
                        }
                    });
                } else {
                    // Backward compatibility với single callback
                    try {
                        callbackSet(message.data);
                    } catch (err) {
                        console.error('Callback error:', err);
                    }
                }
            }
            
            // Trigger global callback nếu có
            if (this.callbacks.has('*')) {
                const callbackSet = this.callbacks.get('*');
                if (callbackSet instanceof Set) {
                    callbackSet.forEach(callback => {
                        try {
                            callback(message);
                        } catch (err) {
                            console.error('Global callback error:', err);
                        }
                    });
                } else {
                    try {
                        callbackSet(message);
                    } catch (err) {
                        console.error('Global callback error:', err);
                    }
                }
            }
        } catch (error) {
            console.error('Error parsing message:', error);
            
            // Trigger error callback
            if (this.callbacks.has('error')) {
                const callbackSet = this.callbacks.get('error');
                const errorData = { message: 'Failed to parse server response' };
                if (callbackSet instanceof Set) {
                    callbackSet.forEach(callback => {
                        try {
                            callback(errorData);
                        } catch (err) {
                            console.error('Error callback error:', err);
                        }
                    });
                } else {
                    try {
                        callbackSet(errorData);
                    } catch (err) {
                        console.error('Error callback error:', err);
                    }
                }
            }
        }
    }

    /**
     * Thử reconnect với exponential backoff
     */
    attemptReconnect() {
        // Không reconnect nếu đang disconnect
        if (this.isDisconnecting) {
            return;
        }

        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error('Max reconnection attempts reached');
            
            if (this.callbacks.has('connection_failed')) {
                const callbackSet = this.callbacks.get('connection_failed');
                if (callbackSet instanceof Set) {
                    callbackSet.forEach(callback => {
                        try {
                            callback();
                        } catch (err) {
                            console.error('Connection failed callback error:', err);
                        }
                    });
                } else {
                    try {
                        callbackSet();
                    } catch (err) {
                        console.error('Connection failed callback error:', err);
                    }
                }
            }
            return;
        }

        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts - 1), 10000);
        
        console.log(`Reconnect attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts} in ${delay}ms`);
        
        // Clear any existing timer
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
        }
        
        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            // Kiểm tra lại trạng thái trước khi reconnect
            if (!this.isDisconnecting && !this.isConnected) {
                this.connect().catch(error => {
                    console.error('Reconnection failed:', error);
                });
            }
        }, delay);
    }

    /**
     * Xử lý message queue khi kết nối lại
     */
    processMessageQueue() {
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.send(message);
        }
    }

    /**
     * Subscribe to message type
     */
    subscribe(messageType, callback) {
        if (!this.callbacks.has(messageType)) {
            this.callbacks.set(messageType, new Set());
        }
        
        const callbackSet = this.callbacks.get(messageType);
        if (callbackSet instanceof Set) {
            callbackSet.add(callback);
        } else {
            // Convert single callback to Set
            const newSet = new Set([callbackSet, callback]);
            this.callbacks.set(messageType, newSet);
        }
    }

    /**
     * Unsubscribe from message type
     */
    unsubscribe(messageType, callback = null) {
        if (!this.callbacks.has(messageType)) {
            return;
        }
        
        const callbackSet = this.callbacks.get(messageType);
        if (callback) {
            // Remove specific callback
            if (callbackSet instanceof Set) {
                callbackSet.delete(callback);
                if (callbackSet.size === 0) {
                    this.callbacks.delete(messageType);
                }
            } else if (callbackSet === callback) {
                this.callbacks.delete(messageType);
            }
        } else {
            // Remove all callbacks for this message type
            this.callbacks.delete(messageType);
        }
    }

    /**
     * Gửi message đến server
     */
    send(message) {
        if (!this.isConnected || !this.ws || this.ws.readyState !== WebSocket.OPEN) {
            // Queue message để gửi sau khi kết nối
            this.messageQueue.push(message);
            
            // Thử kết nối lại nếu chưa kết nối
            if (!this.isConnected && !this.connectionPromise) {
                this.connect().catch(console.error);
            }
            
            return false;
        }

        try {
            this.ws.send(JSON.stringify(message));
            console.log('Sent message:', message.type);
            return true;
        } catch (error) {
            console.error('Error sending message:', error);
            
            // Queue lại message nếu gửi thất bại
            this.messageQueue.push(message);
            return false;
        }
    }

    /**
     * Đăng nhập
     */
    login(username, password, avatar) {
        const message = {
            type: 'login',
            data: { 
                username: username.trim(), 
                password,
                avatar: avatar || 'avt1.jpg'
            }
        };
        return this.send(message);
    }

    /**
     * Đăng ký
     */
    register(username, password) {
        const message = {
            type: 'register', 
            data: { 
                username: username.trim(), 
                password, 
            }
        };
        return this.send(message);
    }

    /**
     * Đăng xuất
     */
    logout() {
        const message = {
            type: 'logout',
            data: {}
        };
        return this.send(message);
    }

    /**
     * Ngắt kết nối
     */
    disconnect() {
        this.isDisconnecting = true;
        
        // Clear reconnect timer
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
        
        if (this.ws) {
            this.isConnected = false;
            try {
                if (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CLOSING) {
                    this.ws.close(1000, 'User disconnect');
                } else if (this.ws.readyState === WebSocket.CONNECTING) {
                    // Defer close until open to avoid: "closed before the connection is established"
                    const wsRef = this.ws;
                    wsRef.addEventListener('open', () => {
                        try { wsRef.close(1000, 'User disconnect'); } catch (_) {}
                    }, { once: true });
                }
            } catch (_) {}
            this.ws = null;
        }
        
        this.callbacks.clear();
        this.messageQueue.length = 0;
        this.reconnectAttempts = 0;
        this.connectionPromise = null;
        this.isConnecting = false;
        this.isDisconnecting = false;
    }

    /**
     * Kiểm tra trạng thái kết nối
     */
    getConnectionState() {
        if (!this.ws) return 'disconnected';
        
        switch (this.ws.readyState) {
            case WebSocket.CONNECTING: return 'connecting';
            case WebSocket.OPEN: return 'connected';
            case WebSocket.CLOSING: return 'closing';
            case WebSocket.CLOSED: return 'disconnected';
            default: return 'unknown';
        }
    }

    /**
     * Lấy thông tin connection
     */
    getConnectionInfo() {
        return {
            url: this.gatewayUrl,
            state: this.getConnectionState(),
            isConnected: this.isConnected,
            isConnecting: this.isConnecting,
            isDisconnecting: this.isDisconnecting,
            reconnectAttempts: this.reconnectAttempts,
            maxReconnectAttempts: this.maxReconnectAttempts,
            queuedMessages: this.messageQueue.length,
            hasReconnectTimer: !!this.reconnectTimer,
            subscriberCount: this.callbacks.size
        };
    }

    /**
     * Kiểm tra và báo cáo trạng thái kết nối chi tiết
     */
    diagnoseConnection() {
        const info = this.getConnectionInfo();
        console.log('=== WebSocket Connection Diagnosis ===');
        console.log(`URL: ${info.url}`);
        console.log(`State: ${info.state}`);
        console.log(`Connected: ${info.isConnected}`);
        console.log(`Connecting: ${info.isConnecting}`);
        console.log(`Disconnecting: ${info.isDisconnecting}`);
        console.log(`Reconnect attempts: ${info.reconnectAttempts}/${info.maxReconnectAttempts}`);
        console.log(`Queued messages: ${info.queuedMessages}`);
        console.log(`Active subscribers: ${info.subscriberCount}`);
        console.log(`Has reconnect timer: ${info.hasReconnectTimer}`);
        
        if (this.ws) {
            console.log(`WebSocket readyState: ${this.ws.readyState}`);
            console.log(`WebSocket URL: ${this.ws.url}`);
        } else {
            console.log('WebSocket instance: null');
        }
        console.log('=====================================');
        
        return info;
    }
}

// Tạo singleton instance
let authServiceInstance = null;

export const getAuthService = (gatewayUrl) => {
    if (!authServiceInstance) {
        authServiceInstance = new AuthService(gatewayUrl);
    }
    return authServiceInstance;
};

export default AuthService;