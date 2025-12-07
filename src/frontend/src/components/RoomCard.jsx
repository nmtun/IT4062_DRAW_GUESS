import React from 'react';
import './RoomCard.css';

export default function RoomCard({ room, onJoin }) {
  const handleClick = () => {
    if (onJoin && room.canJoin) {
      // Chuyển đổi ID thành số nguyên
      const roomId = typeof room.id === 'string' ? parseInt(room.id) : room.id;
      onJoin(roomId);
    }
  };

  const getStateColor = (state) => {
    switch (state) {
      case 0: return '#28a745'; // Xanh lá - chờ
      case 1: return '#ffc107'; // Vàng - đang chơi  
      case 2: return '#6c757d'; // Xám - kết thúc
      default: return '#dc3545'; // Đỏ - lỗi
    }
  };

  return (
    <div 
      className={`room-card ${!room.canJoin ? 'disabled' : ''}`} 
      onClick={handleClick}
      style={{ cursor: room.canJoin ? 'pointer' : 'not-allowed' }}
    >
      <div className="room-icon">
        <span className="icon">🎮</span>
      </div>
      <div className="room-info">
        <h3 className="room-name">{room.name || `Phòng #${room.id}`}</h3>
        <div className="room-details">
          <span className="detail-item">
            <span className="icon">👥</span>
            {room.currentPlayers || 0}/{room.maxPlayers || 8}
          </span>
          <span className="detail-item">
            <span className="icon">📍</span>
            <span style={{ color: getStateColor(room.state) }}>
              {room.stateText || 'Không xác định'}
            </span>
          </span>
          <span className="detail-item">
            <span className="icon">👑</span>
            {room.isOfficial ? 'Hệ thống' : `User ${room.ownerId}`}
          </span>
        </div>
      </div>
      {room.isOfficial && (
        <div className="official-badge">✓</div>
      )}
    </div>
  );
}

