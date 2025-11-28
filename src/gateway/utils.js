// Message buffer để xử lý TCP messages có thể bị phân mảnh
class MessageBuffer {
    constructor() {
        this.buffer = Buffer.alloc(0);
    }

    addData(data) {
        this.buffer = Buffer.concat([this.buffer, data]);
        return this.extractMessages();
    }

    extractMessages() {
        const messages = [];
        
        while (this.buffer.length >= 3) {
            const type = this.buffer.readUInt8(0);
            const length = this.buffer.readUInt16BE(1);
            const totalLength = 3 + length;
            
            if (this.buffer.length >= totalLength) {
                // Có đủ data cho một message hoàn chỉnh
                const messageData = this.buffer.slice(0, totalLength);
                messages.push(messageData);
                
                // Cắt bỏ message đã xử lý
                this.buffer = this.buffer.slice(totalLength);
            } else {
                // Chưa đủ data, chờ thêm
                break;
            }
        }
        
        return messages;
    }

    clear() {
        this.buffer = Buffer.alloc(0);
    }
}

// Logger với màu sắc
class Logger {
    static info(message, ...args) {
        console.log('\x1b[36m[INFO]\x1b[0m', message, ...args);
    }
    
    static warn(message, ...args) {
        console.log('\x1b[33m[WARN]\x1b[0m', message, ...args);
    }
    
    static error(message, ...args) {
        console.log('\x1b[31m[ERROR]\x1b[0m', message, ...args);
    }
    
    static success(message, ...args) {
        console.log('\x1b[32m[SUCCESS]\x1b[0m', message, ...args);
    }
    
    static debug(message, ...args) {
        if (process.env.DEBUG) {
            console.log('\x1b[35m[DEBUG]\x1b[0m', message, ...args);
        }
    }
}

// Connection manager để quản lý kết nối TCP
class TcpConnectionManager {
    constructor(host, port, maxRetries = 5) {
        this.host = host;
        this.port = port;
        this.maxRetries = maxRetries;
        this.retryCount = 0;
        this.retryDelay = 1000; // 1 second
        this.isConnecting = false;
    }

    connect() {
        return new Promise((resolve, reject) => {
            if (this.isConnecting) {
                reject(new Error('Connection already in progress'));
                return;
            }

            this.isConnecting = true;
            const net = require('net');
            const client = new net.Socket();

            const connectTimer = setTimeout(() => {
                client.destroy();
                this.isConnecting = false;
                reject(new Error('Connection timeout'));
            }, 10000); // 10 second timeout

            client.connect(this.port, this.host, () => {
                clearTimeout(connectTimer);
                this.isConnecting = false;
                this.retryCount = 0;
                Logger.success(`Connected to TCP server ${this.host}:${this.port}`);
                resolve(client);
            });

            client.on('error', (error) => {
                clearTimeout(connectTimer);
                this.isConnecting = false;
                
                if (this.retryCount < this.maxRetries) {
                    this.retryCount++;
                    Logger.warn(`Connection failed, retrying (${this.retryCount}/${this.maxRetries})...`);
                    
                    setTimeout(() => {
                        this.connect().then(resolve).catch(reject);
                    }, this.retryDelay * this.retryCount);
                } else {
                    Logger.error('Max retries reached, connection failed');
                    reject(error);
                }
            });
        });
    }

    reset() {
        this.retryCount = 0;
        this.isConnecting = false;
    }
}

// Validation utilities
class MessageValidator {
    static validateWebSocketMessage(message) {
        if (!message || typeof message !== 'object') {
            throw new Error('Invalid message format');
        }
        
        if (!message.type || typeof message.type !== 'string') {
            throw new Error('Missing or invalid message type');
        }
        
        if (!message.data || typeof message.data !== 'object') {
            throw new Error('Missing or invalid message data');
        }
        
        // Validate specific message types
        switch (message.type) {
            case 'login':
                this.validateLogin(message.data);
                break;
            case 'register':
                this.validateRegister(message.data);
                break;
            // thêm các case khác ở đây
        }
        
        return true;
    }
    
    static validateLogin(data) {
        if (!data.username || typeof data.username !== 'string' || data.username.length > 32) {
            throw new Error('Invalid username');
        }
        if (!data.password || typeof data.password !== 'string' || data.password.length > 32) {
            throw new Error('Invalid password');
        }
    }
    
    static validateRegister(data) {
        this.validateLogin(data);
        if (!data.username || !data.password) {
            throw new Error('Invalid input')
        }
    }
    
    // thêm các validate khác nếu cần
}

// Performance monitoring
class PerformanceMonitor {
    constructor() {
        this.stats = {
            messagesReceived: 0,
            messagesSent: 0,
            connectionsActive: 0,
            connectionsTotal: 0,
            errors: 0,
            startTime: Date.now()
        };
        
        // Log stats every 30 seconds
        setInterval(() => {
            this.logStats();
        }, 30000);
    }
    
    incrementMessagesReceived() {
        this.stats.messagesReceived++;
    }
    
    incrementMessagesSent() {
        this.stats.messagesSent++;
    }
    
    incrementConnectionsActive() {
        this.stats.connectionsActive++;
        this.stats.connectionsTotal++;
    }
    
    decrementConnectionsActive() {
        this.stats.connectionsActive--;
    }
    
    incrementErrors() {
        this.stats.errors++;
    }
    
    logStats() {
        const uptime = Math.floor((Date.now() - this.stats.startTime) / 1000);
        Logger.info('Performance Stats:', {
            uptime: `${Math.floor(uptime / 60)}m ${uptime % 60}s`,
            activeConnections: this.stats.connectionsActive,
            totalConnections: this.stats.connectionsTotal,
            messagesReceived: this.stats.messagesReceived,
            messagesSent: this.stats.messagesSent,
            errors: this.stats.errors,
            avgMessagesPerMinute: Math.floor((this.stats.messagesReceived + this.stats.messagesSent) / (uptime / 60))
        });
    }
    
    getStats() {
        return { ...this.stats };
    }
}

module.exports = {
    MessageBuffer,
    Logger,
    TcpConnectionManager,
    MessageValidator,
    PerformanceMonitor
};