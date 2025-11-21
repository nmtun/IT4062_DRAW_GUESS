// Lobby page functionality
document.addEventListener('DOMContentLoaded', function() {
    initLobbyTabs();
    initLobbyControls();
});

// Initialize tab switching
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

// Initialize header controls
function initLobbyControls() {
    const soundBtn = document.getElementById('soundBtn');
    const infoBtn = document.getElementById('infoBtn');
    const closeBtn = document.getElementById('closeBtn');
    
    if (soundBtn) {
        soundBtn.addEventListener('click', function() {
            // Toggle sound
            const icon = this.querySelector('.icon');
            if (icon.textContent === '🔊') {
                icon.textContent = '🔇';
            } else {
                icon.textContent = '🔊';
            }
        });
    }
    
    if (infoBtn) {
        infoBtn.addEventListener('click', function() {
            alert('Draw & Guess - Trò chơi vẽ và đoán từ\nChờ người chơi khác tham gia để bắt đầu!');
        });
    }
    
    if (closeBtn) {
        closeBtn.addEventListener('click', function() {
            if (confirm('Bạn có chắc muốn rời khỏi phòng chờ?')) {
                window.location.href = 'index.html';
            }
        });
    }
}

// Add player to the list
function addPlayer(playerData) {
    const playerList = document.getElementById('playerList');
    const emptySlots = playerList.querySelectorAll('.player-item.empty');
    
    if (emptySlots.length > 0) {
        const firstEmpty = emptySlots[0];
        firstEmpty.classList.remove('empty');
        
        const avatar = firstEmpty.querySelector('.player-avatar');
        const name = firstEmpty.querySelector('.player-name');
        const info = firstEmpty.querySelector('.player-info');
        
        avatar.innerHTML = `<img src="${playerData.avatar || 'css/assets/avatar/avatar-cute-2.jpg'}" alt="${playerData.username}">`;
        name.textContent = playerData.username;
        
        if (!info.querySelector('.player-score')) {
            const score = document.createElement('div');
            score.className = 'player-score';
            score.textContent = '0 điểm';
            info.appendChild(score);
        }
    }
}

// Remove player from the list
function removePlayer(username) {
    const playerList = document.getElementById('playerList');
    const players = playerList.querySelectorAll('.player-item:not(.empty)');
    
    players.forEach(player => {
        const nameElement = player.querySelector('.player-name');
        if (nameElement && nameElement.textContent === username) {
            player.classList.add('empty');
            player.querySelector('.player-avatar').innerHTML = '<span class="empty-icon">👤</span>';
            player.querySelector('.player-avatar').classList.add('empty-avatar');
            player.querySelector('.player-name').textContent = 'Trống';
            const score = player.querySelector('.player-score');
            if (score) {
                score.remove();
            }
        }
    });
}

// Update player score
function updatePlayerScore(username, score) {
    const playerList = document.getElementById('playerList');
    const players = playerList.querySelectorAll('.player-item:not(.empty)');
    
    players.forEach(player => {
        const nameElement = player.querySelector('.player-name');
        if (nameElement && nameElement.textContent === username) {
            const scoreElement = player.querySelector('.player-score');
            if (scoreElement) {
                scoreElement.textContent = `${score} điểm`;
            }
        }
    });
}

// Add chat message
function addChatMessage(author, message) {
    const chatMessages = document.getElementById('chatMessages');
    const messageDiv = document.createElement('div');
    messageDiv.className = 'chat-message';
    messageDiv.innerHTML = `
        <span class="message-author">${author}:</span>
        <span class="message-text">${message}</span>
    `;
    chatMessages.appendChild(messageDiv);
    chatMessages.scrollTop = chatMessages.scrollHeight;
}

// Enable answer input when game starts
function enableAnswerInput() {
    const answerInput = document.getElementById('answerInput');
    if (answerInput) {
        answerInput.disabled = false;
        answerInput.placeholder = 'Nhập câu trả lời của bạn...';
    }
}

// Enable chat input when logged in
function enableChatInput() {
    const chatInput = document.getElementById('chatInput');
    if (chatInput) {
        chatInput.disabled = false;
        chatInput.placeholder = 'Nhập tin nhắn...';
    }
}

// Update waiting state
function updateWaitingState(message) {
    const waitingMessage = document.querySelector('.waiting-message');
    if (waitingMessage) {
        waitingMessage.textContent = message;
    }
}

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

