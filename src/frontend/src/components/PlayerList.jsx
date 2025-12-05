import React from 'react';
import './PlayerList.css';
import { getAvatar, getCurrentUser } from '../utils/userStorage';

const MAX_PLAYERS = 10;

export default function PlayerList({ players = [], currentUserId = null, maxPlayers = MAX_PLAYERS }) {
  // Tạo danh sách đầy đủ với các slot trống
  const displayPlayers = [];
  
  // Thêm players hiện có với thông tin đầy đủ
  players.forEach(player => {
    // Lấy avatar từ localStorage hoặc avatar mặc định cho player hiện tại
    let playerAvatar = player.avatar;
    if (player.id === currentUserId) {
      // Nếu là user hiện tại, lấy avatar từ localStorage
      const currentUser = getCurrentUser();
      playerAvatar = currentUser?.avatar || getAvatar();
    }

    // Nếu avatar không phải là emoji, tạo đường dẫn đến file ảnh
    let avatarDisplay = playerAvatar;
    if (playerAvatar && !playerAvatar.includes('👤') && !playerAvatar.includes('🎭') && playerAvatar.includes('.jpg')) {
      avatarDisplay = `/src/assets/avt/${playerAvatar}`;
    }

    displayPlayers.push({
      ...player,
      avatar: avatarDisplay,
      isEmpty: false
    });
  });
  
  // Thêm các slot trống
  for (let i = players.length; i < maxPlayers; i++) {
    displayPlayers.push({
      id: `empty-${i}`,
      username: 'Trống',
      avatar: '👤',
      score: 0,
      isDrawing: false,
      isEmpty: true
    });
  }

  return (
    <div className="player-list">
      <div className="player-list-header">
        <h3>Draw & Guess</h3>
      </div>
      <div className="players-container">
        {displayPlayers.map((player) => (
          <div
            key={player.id}
            className={`player-item ${player.id === currentUserId ? 'current-player' : ''} ${player.isDrawing ? 'drawing' : ''} ${player.isEmpty ? 'empty-slot' : ''}`}
          >
            <div className="player-avatar">
              {player.avatar && player.avatar.startsWith('/src/assets/') ? (
                <img src={player.avatar} alt="avatar" className="avatar-image" />
              ) : (
                <span className="avatar-emoji">{player.avatar || '👤'}</span>
              )}
              {player.isDrawing && <span className="drawing-badge">✏️</span>}
            </div>
            <div className="player-info">
              <div className="player-name">
                {player.username}
                {player.isOwner && <span className="owner-badge">👑</span>}
              </div>
              <div className="player-score">{player.score || 0} điểm</div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

