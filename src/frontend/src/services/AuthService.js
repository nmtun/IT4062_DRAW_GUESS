/**
 * AuthService - Xử lý authentication thông qua WebSocket Gateway
 */
class AuthService {
    constructor(gatewayUrl = 'ws://localhost:3000') {
        this.gatewayUrl = gatewayUrl;
        this.ws = null;
        this.isConnected = false;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.callbacks = new Map();
        this.messageQueue = [];
        this.connectionPromise = null;
    }

    /**
     * Kết nối đến WebSocket Gateway
     */
    connect() {
        if (this.connectionPromise) {
            return this.connectionPromise;
        }

        this.connectionPromise = new Promise((resolve, reject) => {
            try {
                console.log(`Connecting to gateway at ${this.gatewayUrl}`);
                this.ws = new WebSocket(this.gatewayUrl);
                
                this.ws.onopen = () => {
                    console.log('Connected to gateway successfully');
                    this.isConnected = true;
                    this.reconnectAttempts = 0;
                    this.connectionPromise = null;
                    
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
                    this.connectionPromise = null;
                    
                    // Chỉ reconnect nếu không phải do user đóng kết nối
                    if (event.code !== 1000) {
                        this.attemptReconnect();
                    }
                };

                this.ws.onerror = (error) => {
                    console.error('Gateway connection error:', error);
                    this.connectionPromise = null;
                    reject(error);
                };

                // Timeout cho connection
                setTimeout(() => {
                    if (this.ws.readyState !== WebSocket.OPEN) {
                        this.ws.close();
                        this.connectionPromise = null;
                        reject(new Error('Connection timeout'));
                    }
                }, 10000);

            } catch (error) {
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
            
            // Trigger callback nếu có
            if (this.callbacks.has(message.type)) {
                this.callbacks.get(message.type)(message.data);
            }
            
            // Trigger global callback nếu có
            if (this.callbacks.has('*')) {
                this.callbacks.get('*')(message);
            }
        } catch (error) {
            console.error('Error parsing message:', error);
            
            // Trigger error callback
            if (this.callbacks.has('error')) {
                this.callbacks.get('error')({ 
                    message: 'Failed to parse server response' 
                });
            }
        }
    }

    /**
     * Thử reconnect với exponential backoff
     */
    attemptReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            console.error('Max reconnection attempts reached');
            
            if (this.callbacks.has('connection_failed')) {
                this.callbacks.get('connection_failed')();
            }
            return;
        }

        this.reconnectAttempts++;
        const delay = Math.min(1000 * Math.pow(2, this.reconnectAttempts - 1), 10000);
        
        console.log(`Reconnect attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts} in ${delay}ms`);
        
        setTimeout(() => {
            this.connect().catch(error => {
                console.error('Reconnection failed:', error);
            });
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
        this.callbacks.set(messageType, callback);
    }

    /**
     * Unsubscribe from message type
     */
    unsubscribe(messageType) {
        this.callbacks.delete(messageType);
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
    login(username, password) {
        const message = {
            type: 'login',
            data: { 
                username: username.trim(), 
                password
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
        if (this.ws) {
            this.isConnected = false;
            this.ws.close(1000, 'User disconnect');
            this.ws = null;
        }
        
        this.callbacks.clear();
        this.messageQueue.length = 0;
        this.reconnectAttempts = 0;
        this.connectionPromise = null;
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
            reconnectAttempts: this.reconnectAttempts,
            queuedMessages: this.messageQueue.length
        };
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