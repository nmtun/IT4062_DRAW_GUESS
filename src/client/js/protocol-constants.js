// Protocol constants (phải khớp với protocol.h)

// Message Types
export const MSG_LOGIN_REQUEST = 0x01;
export const MSG_LOGIN_RESPONSE = 0x02;
export const MSG_REGISTER_REQUEST = 0x03;
export const MSG_REGISTER_RESPONSE = 0x04;
export const MSG_LOGOUT = 0x05;

// Status Codes
export const STATUS_SUCCESS = 0x00;
export const STATUS_ERROR = 0x01;
export const STATUS_INVALID_USERNAME = 0x02;
export const STATUS_INVALID_PASSWORD = 0x03;
export const STATUS_USER_EXISTS = 0x04;
export const STATUS_AUTH_FAILED = 0x05;

// Field Lengths
export const MAX_USERNAME_LEN = 32;
export const MAX_PASSWORD_LEN = 32;
export const MAX_EMAIL_LEN = 64;

// Server Configuration
export const WS_SERVER_URL = 'ws://localhost:3001';
export const TCP_SERVER_HOST = 'localhost';
export const TCP_SERVER_PORT = 8080;

// Timeout
export const REQUEST_TIMEOUT = 10000; // 10 seconds

// Helper function để hiển thị tên message type
export function getMessageTypeName(type) {
    const names = {
        0x01: 'LOGIN_REQUEST',
        0x02: 'LOGIN_RESPONSE',
        0x03: 'REGISTER_REQUEST',
        0x04: 'REGISTER_RESPONSE',
        0x05: 'LOGOUT'
    };
    return names[type] || `UNKNOWN(0x${type.toString(16)})`;
}

