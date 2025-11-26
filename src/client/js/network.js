// Network module - Main entry point cho network operations
// Import các modules

import {
    MSG_LOGIN_REQUEST,
    MSG_LOGIN_RESPONSE,
    MSG_REGISTER_REQUEST,
    MSG_REGISTER_RESPONSE,
    REQUEST_TIMEOUT
} from './protocol-constants.js';

import { createLoginRequest, createRegisterRequest } from './message-builder.js';
import { parseLoginResponse, parseRegisterResponse } from './message-parser.js';
import {
    connectToServer,
    sendMessage,
    onMessage,
    removeMessageHandler,
    disconnect,
    isServerConnected
} from './websocket-client.js';

/**
 * Gửi login request
 * @param {string} username 
 * @param {string} password 
 * @returns {Promise<{status: number, userId: number, username: string}>}
 */
export function sendLoginRequest(username, password) {
    return new Promise((resolve, reject) => {
        let timeoutId;
        let handlerRegistered = false;

        // Timeout sau REQUEST_TIMEOUT giây
        timeoutId = setTimeout(() => {
            if (!handlerRegistered) {
                reject(new Error('Timeout: Không nhận được response từ server'));
            }
        }, REQUEST_TIMEOUT);

        // Đảm bảo đã kết nối
        connectToServer().then(() => {
            // Đăng ký handler cho response (chỉ một lần)
            const responseHandler = (payload, view) => {
                if (handlerRegistered) return; // Tránh xử lý nhiều lần
                handlerRegistered = true;
                clearTimeout(timeoutId);
                removeMessageHandler(MSG_LOGIN_RESPONSE); // Xóa handler sau khi dùng
                try {
                    const response = parseLoginResponse(payload, view);
                    console.log('✅ Parse login response thành công:', response);
                    resolve(response);
                } catch (error) {
                    console.error('❌ Lỗi parse login response:', error);
                    reject(error);
                }
            };

            onMessage(MSG_LOGIN_RESPONSE, responseHandler);

            // Gửi request
            console.log('📤 Gửi LOGIN_REQUEST...');
            const requestPayload = createLoginRequest(username, password);
            if (!sendMessage(MSG_LOGIN_REQUEST, requestPayload)) {
                clearTimeout(timeoutId);
                reject(new Error('Không thể gửi login request'));
            } else {
                console.log('✅ Đã gửi LOGIN_REQUEST, đang chờ response...');
            }
        }).catch((error) => {
            clearTimeout(timeoutId);
            reject(error);
        });
    });
}

/**
 * Gửi register request
 * @param {string} username 
 * @param {string} password 
 * @param {string} email 
 * @returns {Promise<{status: number, message: string}>}
 */
export function sendRegisterRequest(username, password, email = '') {
    return new Promise((resolve, reject) => {
        let timeoutId;
        let handlerRegistered = false;

        // Timeout sau REQUEST_TIMEOUT giây
        timeoutId = setTimeout(() => {
            if (!handlerRegistered) {
                reject(new Error('Timeout: Không nhận được response từ server'));
            }
        }, REQUEST_TIMEOUT);

        // Đảm bảo đã kết nối
        connectToServer().then(() => {
            // Đăng ký handler cho response
            const responseHandler = (payload, view) => {
                if (handlerRegistered) return; // Tránh xử lý nhiều lần
                handlerRegistered = true;
                clearTimeout(timeoutId);
                removeMessageHandler(MSG_REGISTER_RESPONSE); // Xóa handler sau khi dùng
                try {
                    const response = parseRegisterResponse(payload, view);
                    console.log('✅ Parse register response thành công:', response);
                    resolve(response);
                } catch (error) {
                    console.error('❌ Lỗi parse register response:', error);
                    reject(error);
                }
            };

            onMessage(MSG_REGISTER_RESPONSE, responseHandler);

            // Gửi request
            console.log('📤 Gửi REGISTER_REQUEST...');
            const requestPayload = createRegisterRequest(username, password, email);
            if (!sendMessage(MSG_REGISTER_REQUEST, requestPayload)) {
                clearTimeout(timeoutId);
                reject(new Error('Không thể gửi register request'));
            } else {
                console.log('✅ Đã gửi REGISTER_REQUEST, đang chờ response...');
            }
        }).catch((error) => {
            clearTimeout(timeoutId);
            reject(error);
        });
    });
}

// Export functions
if (typeof window !== 'undefined') {
    window.Network = {
        connectToServer,
        sendLoginRequest,
        sendRegisterRequest,
        disconnect,
        isServerConnected,
        onMessage,
        removeMessageHandler
    };
}
