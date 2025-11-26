// WebSocket Client - Quản lý kết nối WebSocket và message handling

import { WS_SERVER_URL, getMessageTypeName } from './protocol-constants.js';

// WebSocket connection state
let socket = null;
let isConnected = false;
let messageHandlers = {};
let receiveBuffer = new Uint8Array(0);

/**
 * Kết nối đến WebSocket server
 * @returns {Promise<void>}
 */
export function connectToServer() {
    return new Promise((resolve, reject) => {
        if (socket && socket.readyState === WebSocket.OPEN) {
            resolve();
            return;
        }

        socket = new WebSocket(WS_SERVER_URL);
        socket.binaryType = 'arraybuffer';

        socket.onopen = function() {
            console.log('✅ Đã kết nối đến WebSocket server');
            isConnected = true;
            resolve();
        };

        socket.onmessage = function(event) {
            if (event.data instanceof ArrayBuffer) {
                appendToReceiveBuffer(new Uint8Array(event.data));
            } else if (event.data instanceof Blob) {
                event.data.arrayBuffer().then(buffer => {
                    appendToReceiveBuffer(new Uint8Array(buffer));
                });
            } else if (event.data instanceof Uint8Array) {
                appendToReceiveBuffer(event.data);
            } else {
                console.warn('⚠️ Nhận dữ liệu không xác định từ WebSocket:', event.data);
            }
        };

        socket.onerror = function(error) {
            console.error('Lỗi kết nối:', error);
            isConnected = false;
            reject(error);
        };

        socket.onclose = function() {
            console.log('Đã ngắt kết nối');
            isConnected = false;
        };
    });
}

/**
 * Append chunk to receive buffer
 * @param {Uint8Array} chunk 
 */
function appendToReceiveBuffer(chunk) {
    if (!chunk || chunk.length === 0) {
        return;
    }

    const combined = new Uint8Array(receiveBuffer.length + chunk.length);
    combined.set(receiveBuffer, 0);
    combined.set(chunk, receiveBuffer.length);
    receiveBuffer = combined;

    processReceiveBuffer();
}

/**
 * Process receive buffer and dispatch complete messages
 */
function processReceiveBuffer() {
    while (receiveBuffer.length >= 3) {
        const view = new DataView(receiveBuffer.buffer, receiveBuffer.byteOffset, receiveBuffer.length);
        const type = view.getUint8(0);
        const length = view.getUint16(1, true); // little-endian
        const totalLength = 3 + length;

        if (receiveBuffer.length < totalLength) {
            break;
        }

        const payload = receiveBuffer.slice(3, totalLength);
        dispatchMessage(type, payload);

        receiveBuffer = receiveBuffer.slice(totalLength);
    }
}

/**
 * Dispatch message to registered handler
 * @param {number} type 
 * @param {Uint8Array} payload 
 */
function dispatchMessage(type, payload) {
    const payloadView = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);

    if (messageHandlers[type]) {
        console.log(`📨 Nhận message type: 0x${type.toString(16).padStart(2, '0')} (${getMessageTypeName(type)})`);
        messageHandlers[type](payload, payloadView);
    } else {
        console.warn('⚠️ Không có handler cho message type:', type);
    }
}

/**
 * Đăng ký handler cho message type
 * @param {number} type 
 * @param {Function} handler 
 */
export function onMessage(type, handler) {
    const oldHandler = messageHandlers[type];
    if (oldHandler) {
        // Nếu đã có handler, wrap lại để gọi cả hai
        messageHandlers[type] = (payload, view) => {
            oldHandler(payload, view);
            handler(payload, view);
        };
    } else {
        messageHandlers[type] = handler;
    }
}

/**
 * Xóa handler cho message type
 * @param {number} type 
 */
export function removeMessageHandler(type) {
    delete messageHandlers[type];
}

/**
 * Gửi message đến server
 * @param {number} type 
 * @param {Uint8Array} payload 
 * @returns {boolean}
 */
export function sendMessage(type, payload) {
    if (!socket || socket.readyState !== WebSocket.OPEN) {
        console.error('Chưa kết nối đến server');
        return false;
    }

    const payloadLength = payload ? payload.length : 0;
    const messageLength = 3 + payloadLength;
    const buffer = new ArrayBuffer(messageLength);
    const view = new DataView(buffer);

    // Write message type (1 byte)
    view.setUint8(0, type);

    // Write length (2 bytes, network byte order = big-endian)
    view.setUint16(1, payloadLength, false);

    // Write payload
    if (payload && payloadLength > 0) {
        const payloadView = new Uint8Array(buffer, 3);
        payloadView.set(payload);
    }

    socket.send(buffer);
    return true;
}

/**
 * Ngắt kết nối
 */
export function disconnect() {
    if (socket) {
        socket.close();
        socket = null;
        isConnected = false;
    }
}

/**
 * Kiểm tra trạng thái kết nối
 * @returns {boolean}
 */
export function isServerConnected() {
    return isConnected && socket && socket.readyState === WebSocket.OPEN;
}

