// Message Parser - Parse response messages

import { MAX_USERNAME_LEN } from './protocol-constants.js';

/**
 * Parse login response
 * @param {Uint8Array} payload 
 * @param {DataView} view 
 * @returns {{status: number, userId: number, username: string}}
 */
export function parseLoginResponse(payload, view) {
    const status = view.getUint8(0);
    const userId = view.getInt32(1, false); // big-endian (network byte order)

    // Read username (32 bytes, starting at offset 5)
    const usernameBytes = new Uint8Array(payload.buffer || payload, 5, MAX_USERNAME_LEN);
    const username = new TextDecoder().decode(usernameBytes).replace(/\0/g, '');

    return { status, userId, username };
}

/**
 * Parse register response
 * @param {Uint8Array} payload 
 * @param {DataView} view 
 * @returns {{status: number, message: string}}
 */
export function parseRegisterResponse(payload, view) {
    const status = view.getUint8(0);

    // Read message (128 bytes, starting at offset 1)
    const messageBytes = new Uint8Array(payload.buffer || payload, 1, 128);
    const message = new TextDecoder().decode(messageBytes).replace(/\0/g, '');

    return { status, message };
}

