// Message Builder - Tạo request payloads

import { MAX_USERNAME_LEN, MAX_PASSWORD_LEN, MAX_EMAIL_LEN } from './protocol-constants.js';

/**
 * Tạo login request payload
 * @param {string} username 
 * @param {string} password 
 * @returns {Uint8Array}
 */
export function createLoginRequest(username, password) {
    const buffer = new ArrayBuffer(MAX_USERNAME_LEN + MAX_PASSWORD_LEN);

    // Write username (32 bytes, null-padded)
    const usernameBytes = new TextEncoder().encode(username);
    const usernameArray = new Uint8Array(buffer, 0, MAX_USERNAME_LEN);
    usernameArray.set(usernameBytes);
    // Null padding is automatic (ArrayBuffer is zero-initialized)

    // Write password (32 bytes, null-padded)
    const passwordBytes = new TextEncoder().encode(password);
    const passwordArray = new Uint8Array(buffer, MAX_USERNAME_LEN, MAX_PASSWORD_LEN);
    passwordArray.set(passwordBytes);

    return new Uint8Array(buffer);
}

/**
 * Tạo register request payload
 * @param {string} username 
 * @param {string} password 
 * @param {string} email 
 * @returns {Uint8Array}
 */
export function createRegisterRequest(username, password, email = '') {
    const buffer = new ArrayBuffer(MAX_USERNAME_LEN + MAX_PASSWORD_LEN + MAX_EMAIL_LEN);

    // Write username (32 bytes)
    const usernameBytes = new TextEncoder().encode(username);
    const usernameArray = new Uint8Array(buffer, 0, MAX_USERNAME_LEN);
    usernameArray.set(usernameBytes);

    // Write password (32 bytes)
    const passwordBytes = new TextEncoder().encode(password);
    const passwordArray = new Uint8Array(buffer, MAX_USERNAME_LEN, MAX_PASSWORD_LEN);
    passwordArray.set(passwordBytes);

    // Write email (64 bytes)
    const emailBytes = new TextEncoder().encode(email);
    const emailArray = new Uint8Array(buffer, MAX_USERNAME_LEN + MAX_PASSWORD_LEN, MAX_EMAIL_LEN);
    emailArray.set(emailBytes);

    return new Uint8Array(buffer);
}

