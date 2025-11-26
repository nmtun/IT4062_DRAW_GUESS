// Lobby Utility Functions

import { CONFIG, MESSAGES, ICONS } from './lobby-constants.js';

/**
 * Tạo empty player slot
 * @returns {HTMLElement}
 */
export function createEmptyPlayerSlot() {
    const playerItem = document.createElement('div');
    playerItem.className = 'player-item empty';
    
    playerItem.innerHTML = `
        <div class="player-avatar empty-avatar">
            <span class="empty-icon">${ICONS.EMPTY_PLAYER}</span>
        </div>
        <div class="player-info">
            <div class="player-name">${MESSAGES.EMPTY_SLOT}</div>
        </div>
    `;
    
    return playerItem;
}

/**
 * Tạo player item element
 * @param {Object} playerData 
 * @param {string} playerData.username
 * @param {string} [playerData.avatar]
 * @param {number} [playerData.score]
 * @returns {HTMLElement}
 */
export function createPlayerItem(playerData) {
    const playerItem = document.createElement('div');
    playerItem.className = 'player-item';
    
    const avatar = playerData.avatar || CONFIG.DEFAULT_AVATAR;
    const score = playerData.score ?? CONFIG.DEFAULT_SCORE;
    
    playerItem.innerHTML = `
        <div class="player-avatar">
            <img src="${avatar}" alt="${playerData.username}">
        </div>
        <div class="player-info">
            <div class="player-name">${playerData.username}</div>
            <div class="player-score">${MESSAGES.SCORE_FORMAT(score)}</div>
        </div>
    `;
    
    return playerItem;
}

/**
 * Khởi tạo player list với empty slots
 * @param {HTMLElement} playerListContainer 
 * @param {number} maxPlayers 
 */
export function initializePlayerList(playerListContainer, maxPlayers = CONFIG.MAX_PLAYERS) {
    if (!playerListContainer) return;
    
    // Clear existing content
    playerListContainer.innerHTML = '';
    
    // Tạo empty slots
    for (let i = 0; i < maxPlayers; i++) {
        const emptySlot = createEmptyPlayerSlot();
        playerListContainer.appendChild(emptySlot);
    }
}

/**
 * Tìm empty slot đầu tiên
 * @param {HTMLElement} playerListContainer 
 * @returns {HTMLElement|null}
 */
export function findEmptySlot(playerListContainer) {
    if (!playerListContainer) return null;
    return playerListContainer.querySelector('.player-item.empty');
}

/**
 * Tìm player item theo username
 * @param {HTMLElement} playerListContainer 
 * @param {string} username 
 * @returns {HTMLElement|null}
 */
export function findPlayerByUsername(playerListContainer, username) {
    if (!playerListContainer) return null;
    
    const players = playerListContainer.querySelectorAll('.player-item:not(.empty)');
    for (const player of players) {
        const nameElement = player.querySelector('.player-name');
        if (nameElement && nameElement.textContent === username) {
            return player;
        }
    }
    return null;
}

