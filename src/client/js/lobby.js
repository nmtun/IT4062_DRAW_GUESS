// Lobby page functionality

import {
    MESSAGES,
    ICONS,
    NAV,
    STORAGE_KEYS,
    CONFIG,
    SELECTORS
} from './lobby-constants.js';

import {
    createPlayerItem,
    createEmptyPlayerSlot,
    initializePlayerList,
    findEmptySlot,
    findPlayerByUsername
} from './lobby-utils.js';

// DOM Cache
let domCache = {
    playerList: null,
    chatMessages: null,
    chatInput: null,
    answerInput: null,
    waitingMessage: null,
    soundBtn: null,
    infoBtn: null,
    closeBtn: null
};

// State
let isSoundEnabled = true;

/**
 * Cache DOM elements
 */
function cacheDOM() {
    domCache.playerList = document.querySelector(SELECTORS.PLAYER_LIST);
    domCache.chatMessages = document.querySelector(SELECTORS.CHAT_MESSAGES);
    domCache.chatInput = document.querySelector(SELECTORS.CHAT_INPUT);
    domCache.answerInput = document.querySelector(SELECTORS.ANSWER_INPUT);
    domCache.waitingMessage = document.querySelector(SELECTORS.WAITING_MESSAGE);
    domCache.soundBtn = document.querySelector(SELECTORS.SOUND_BTN);
    domCache.infoBtn = document.querySelector(SELECTORS.INFO_BTN);
    domCache.closeBtn = document.querySelector(SELECTORS.CLOSE_BTN);
}

/**
 * Kiểm tra authentication
 */
function checkAuthentication() {
    const isLoggedIn = sessionStorage.getItem(STORAGE_KEYS.IS_LOGGED_IN);
    const userId = sessionStorage.getItem(STORAGE_KEYS.USER_ID);
    const username = sessionStorage.getItem(STORAGE_KEYS.USERNAME);

    if (!isLoggedIn || !userId || !username) {
        // Chưa đăng nhập, chuyển về trang login
        alert(MESSAGES.AUTH_REQUIRED);
        window.location.href = NAV.INDEX;
        return false;
    }

    // Đã đăng nhập, enable chat input
    enableChatInput();

    // Cập nhật thông tin user trong player list
    updateCurrentUserInfo(username);

    // Đảm bảo socket connection được duy trì
    // Socket sẽ tự động được giữ lại từ lần đăng nhập trước
    if (window.Network && window.Network.isServerConnected) {
        const isConnected = window.Network.isServerConnected();
        if (!isConnected) {
            // Nếu socket bị mất, tự động kết nối lại
            console.log('Socket disconnected, reconnecting...');
            window.Network.connectToServer().catch(err => {
                console.error('Failed to reconnect:', err);
            });
        } else {
            console.log('Socket connection maintained');
        }
    }

    return true;
}

/**
 * Cập nhật thông tin user hiện tại
 * @param {string} username 
 */
function updateCurrentUserInfo(username) {
    if (!domCache.playerList) return;

    const emptySlot = findEmptySlot(domCache.playerList);
    if (emptySlot) {
        // Replace empty slot với current user
        const playerData = {
            username: username,
            avatar: CONFIG.DEFAULT_AVATAR,
            score: CONFIG.DEFAULT_SCORE
        };
        const playerItem = createPlayerItem(playerData);
        emptySlot.replaceWith(playerItem);
    }
}

/**
 * Initialize tab switching
 */
function initLobbyTabs() {
    const tabButtons = document.querySelectorAll('.tab-button');
    const tabPanes = document.querySelectorAll('.tab-pane');

    tabButtons.forEach(button => {
        button.addEventListener('click', function() {
            const targetTab = this.getAttribute('data-tab');

            // Update active state for buttons
            tabButtons.forEach(btn => btn.classList.remove('active'));
            this.classList.add('active');

            // Update active state for panes
            tabPanes.forEach(pane => pane.classList.remove('active'));
            const targetPane = document.getElementById(targetTab + 'Tab');
            if (targetPane) {
                targetPane.classList.add('active');
            }
        });
    });
}

/**
 * Initialize header controls
 */
function initLobbyControls() {
    // Sound button
    if (domCache.soundBtn) {
        domCache.soundBtn.addEventListener('click', handleSoundToggle);
    }

    // Info button
    if (domCache.infoBtn) {
        domCache.infoBtn.addEventListener('click', handleInfoClick);
    }

    // Close button
    if (domCache.closeBtn) {
        domCache.closeBtn.addEventListener('click', handleCloseClick);
    }
}

/**
 * Handle sound toggle
 */
function handleSoundToggle() {
    isSoundEnabled = !isSoundEnabled;
    const icon = domCache.soundBtn.querySelector('.icon');
    if (icon) {
        icon.textContent = isSoundEnabled ? ICONS.SOUND_ON : ICONS.SOUND_OFF;
    }
}

/**
 * Handle info button click
 */
function handleInfoClick() {
    alert(`${MESSAGES.INFO_TITLE}\n${MESSAGES.INFO_CONTENT}`);
}

/**
 * Handle close button click
 */
function handleCloseClick() {
    if (confirm(MESSAGES.LEAVE_CONFIRM)) {
        window.location.href = NAV.INDEX;
    }
}

/**
 * Add player to the list
 * @param {Object} playerData 
 * @param {string} playerData.username
 * @param {string} [playerData.avatar]
 * @param {number} [playerData.score]
 */
export function addPlayer(playerData) {
    if (!domCache.playerList) return;

    const emptySlot = findEmptySlot(domCache.playerList);
    if (emptySlot) {
        const playerItem = createPlayerItem(playerData);
        emptySlot.replaceWith(playerItem);
    }
}

/**
 * Remove player from the list
 * @param {string} username 
 */
export function removePlayer(username) {
    if (!domCache.playerList) return;

    const playerItem = findPlayerByUsername(domCache.playerList, username);
    if (playerItem) {
        const emptySlot = createEmptyPlayerSlot();
        playerItem.replaceWith(emptySlot);
    }
}

/**
 * Update player score
 * @param {string} username 
 * @param {number} score 
 */
export function updatePlayerScore(username, score) {
    if (!domCache.playerList) return;

    const playerItem = findPlayerByUsername(domCache.playerList, username);
    if (playerItem) {
        const scoreElement = playerItem.querySelector('.player-score');
        if (scoreElement) {
            scoreElement.textContent = MESSAGES.SCORE_FORMAT(score);
        }
    }
}

/**
 * Add chat message
 * @param {string} author 
 * @param {string} message 
 */
export function addChatMessage(author, message) {
    if (!domCache.chatMessages) return;

    const messageDiv = document.createElement('div');
    messageDiv.className = 'chat-message';
    messageDiv.innerHTML = `
        <span class="message-author">${author}:</span>
        <span class="message-text">${message}</span>
    `;
    domCache.chatMessages.appendChild(messageDiv);
    domCache.chatMessages.scrollTop = domCache.chatMessages.scrollHeight;
}

/**
 * Enable answer input when game starts
 */
export function enableAnswerInput() {
    if (domCache.answerInput) {
        domCache.answerInput.disabled = false;
        domCache.answerInput.placeholder = MESSAGES.ANSWER_PLACEHOLDER_READY;
    }
}

/**
 * Enable chat input when logged in
 */
export function enableChatInput() {
    if (domCache.chatInput) {
        domCache.chatInput.disabled = false;
        domCache.chatInput.placeholder = MESSAGES.CHAT_PLACEHOLDER_LOGGED_IN;
    }
}

/**
 * Update waiting state
 * @param {string} message 
 */
export function updateWaitingState(message) {
    if (domCache.waitingMessage) {
        domCache.waitingMessage.textContent = message;
    }
}

/**
 * Main initialization
 */
document.addEventListener('DOMContentLoaded', function() {
    cacheDOM();
    
    // Kiểm tra authentication trước
    if (!checkAuthentication()) {
        return; // Redirect đã được xử lý trong checkAuthentication
    }

    // Khởi tạo player list với empty slots
    if (domCache.playerList) {
        initializePlayerList(domCache.playerList, CONFIG.MAX_PLAYERS);
    }

    // Set initial placeholders và messages
    if (domCache.waitingMessage) {
        domCache.waitingMessage.textContent = MESSAGES.WAITING;
    }
    
    if (domCache.answerInput) {
        domCache.answerInput.placeholder = MESSAGES.ANSWER_PLACEHOLDER_WAITING;
    }
    
    if (domCache.chatInput) {
        domCache.chatInput.placeholder = MESSAGES.CHAT_PLACEHOLDER_NOT_LOGGED;
    }

    initLobbyTabs();
    initLobbyControls();
});

// Export functions for use in other scripts
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        addPlayer,
        removePlayer,
        updatePlayerScore,
        addChatMessage,
        enableAnswerInput,
        enableChatInput,
        updateWaitingState
    };
}

