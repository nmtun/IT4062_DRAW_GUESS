// Lobby Constants

export const MESSAGES = {
    AUTH_REQUIRED: 'Bạn cần đăng nhập để vào phòng chờ!',
    LEAVE_CONFIRM: 'Bạn có chắc muốn rời khỏi phòng chờ?',
    INFO_TITLE: 'Draw & Guess - Trò chơi vẽ và đoán từ',
    INFO_CONTENT: 'Chờ người chơi khác tham gia để bắt đầu!',
    WAITING: 'Đang chờ người chơi',
    EMPTY_SLOT: 'Trống',
    SCORE_FORMAT: (score) => `${score} điểm`,
    CHAT_PLACEHOLDER_LOGGED_IN: 'Nhập tin nhắn...',
    CHAT_PLACEHOLDER_NOT_LOGGED: 'Bạn phải đăng nhập để chat',
    ANSWER_PLACEHOLDER_WAITING: 'Đang chờ...',
    ANSWER_PLACEHOLDER_READY: 'Nhập câu trả lời của bạn...'
};

export const ICONS = {
    SOUND_ON: '🔊',
    SOUND_OFF: '🔇',
    INFO: 'ℹ️',
    CLOSE: '✕',
    EMPTY_PLAYER: '👤'
};

export const NAV = {
    INDEX: 'index.html'
};

export const STORAGE_KEYS = {
    USER_ID: 'userId',
    USERNAME: 'username',
    IS_LOGGED_IN: 'isLoggedIn'
};

export const CONFIG = {
    DEFAULT_AVATAR: 'css/assets/avatar/avatar-cute-2.jpg',
    MAX_PLAYERS: 8,
    DEFAULT_SCORE: 0
};

export const SELECTORS = {
    PLAYER_LIST: '#playerList',
    CHAT_MESSAGES: '#chatMessages',
    CHAT_INPUT: '#chatInput',
    ANSWER_INPUT: '#answerInput',
    WAITING_MESSAGE: '.waiting-message',
    SOUND_BTN: '#soundBtn',
    INFO_BTN: '#infoBtn',
    CLOSE_BTN: '#closeBtn'
};

