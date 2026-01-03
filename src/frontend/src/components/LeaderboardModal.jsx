import React, { useEffect } from 'react';
import './LeaderboardModal.css';

export default function LeaderboardModal({ players = [], onClose, show }) {
  // Hàm xử lý avatar path
  const getAvatarPath = (avatarStr) => {
    if (!avatarStr) return '👤';
    // Nếu là emoji thì return luôn
    if (avatarStr.includes('👤') || avatarStr.includes('🎭')) {
      return avatarStr;
    }
    // Nếu đã là path đầy đủ thì return luôn
    if (avatarStr.startsWith('/assets/') || avatarStr.startsWith('/src/assets/')) {
      return avatarStr;
    }
    // Nếu là filename thì convert thành path
    if (avatarStr.includes('.jpg') || avatarStr.includes('.png') || avatarStr.includes('.jpeg')) {
      return `/assets/avt/${avatarStr}`;
    }
    return avatarStr;
  };

  // Sắp xếp players theo điểm giảm dần và xử lý avatar
  const sortedPlayers = [...players].map(p => ({
    ...p,
    avatarPath: getAvatarPath(p.avatar)
  })).sort((a, b) => (b.score || 0) - (a.score || 0));
  
  // Lấy winner (người đứng đầu)
  const winner = sortedPlayers[0];
  const otherPlayers = sortedPlayers.slice(1);

  // Close modal khi ấn ESC
  useEffect(() => {
    const handleEscape = (e) => {
      if (e.key === 'Escape' && show) {
        onClose();
      }
    };
    
    window.addEventListener('keydown', handleEscape);
    return () => window.removeEventListener('keydown', handleEscape);
  }, [show, onClose]);

  // Xử lý click outside modal
  const handleBackdropClick = (e) => {
    if (e.target.classList.contains('leaderboard-modal-backdrop')) {
      onClose();
    }
  };

  if (!show) return null;

  return (
    <div className="leaderboard-modal-backdrop" onClick={handleBackdropClick}>
      <div className="leaderboard-modal">
        <button className="leaderboard-close-btn" onClick={onClose}>✕</button>
        
        <h2 className="leaderboard-title">🏆 Bảng Xếp Hạng 🏆</h2>
        
        {/* Winner Section */}
        {winner && (
          <div className="winner-section">
            <div className="winner-crown">👑</div>
            <div className={`winner-avatar-container ${winner.hasLeft || winner.isActive === 255 ? 'avatar-grayscale' : ''}`}>
              {winner.avatarPath && (winner.avatarPath.startsWith('/assets/') || winner.avatarPath.startsWith('/src/assets/')) ? (
                <img src={winner.avatarPath} alt="avatar" className="winner-avatar" />
              ) : (
                <span className="winner-avatar-emoji">{winner.avatarPath || '👤'}</span>
              )}
            </div>
            <div className="winner-name">{winner.username}</div>
            <div className="winner-score">{winner.score || 0} điểm</div>
          </div>
        )}

        {/* Other Players Rankings */}
        {otherPlayers.length > 0 && (
          <div className="rankings-section">
            {otherPlayers.map((player, index) => (
              <div key={player.id || index} className="ranking-item">
                <div className="ranking-number">#{index + 2}</div>
                <div className={`ranking-avatar ${player.hasLeft || player.isActive === 255 ? 'avatar-grayscale' : ''}`}>
                  {player.avatarPath && (player.avatarPath.startsWith('/assets/') || player.avatarPath.startsWith('/src/assets/')) ? (
                    <img src={player.avatarPath} alt="avatar" className="ranking-avatar-img" />
                  ) : (
                    <span className="ranking-avatar-emoji">{player.avatarPath || '👤'}</span>
                  )}
                </div>
                <div className="ranking-name">{player.username}</div>
                <div className="ranking-score">{player.score || 0} điểm</div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

