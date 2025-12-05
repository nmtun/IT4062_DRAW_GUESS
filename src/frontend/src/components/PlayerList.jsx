import React from 'react';
import './PlayerList.css';
import { getAvatar, getCurrentUser } from '../utils/userStorage';

export default function PlayerList({ players, currentUserId }) {
  console.log('Rendering PlayerList with players:', players, 'and currentUserId:', currentUserId);
  // Chỉ hiển thị đúng danh sách players truyền vào, không tạo slot trống
  const displayPlayers = players.map(player => {
    let playerAvatar = player.avatar;
    if (player.id === currentUserId) {
      const currentUser = getCurrentUser();
      playerAvatar = currentUser?.avatar || getAvatar();
    }
    let avatarDisplay = playerAvatar;
    if (playerAvatar && !playerAvatar.includes('👤') && !playerAvatar.includes('🎭') && playerAvatar.includes('.jpg')) {
      avatarDisplay = `/src/assets/avt/${playerAvatar}`;
    }
    return {
      ...player,
      avatar: avatarDisplay
    };
  });

  return (
    <div className="player-list">
      <div className="player-list-header">
        <h3>Draw & Guess</h3>
      </div>
      <div className="players-container">
        {displayPlayers.map((player, index) => (
          <div
            key={player.id || `slot-${index}`}
            className={`player-item ${player.id === currentUserId ? 'current-player' : ''} ${player.isDrawing ? 'drawing' : ''}`}
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

