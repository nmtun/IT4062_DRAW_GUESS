import React from 'react';
import './RoomCard.css';
import { IoGameController } from 'react-icons/io5';
import { FaUsers, FaCrown, FaCheck } from 'react-icons/fa';
import { MdCircle } from 'react-icons/md';

// Mảng các gradient màu đẹp
const gradientColors = [
  'linear-gradient(135deg, #667eea 0%, #764ba2 100%)', // Tím xanh
  'linear-gradient(135deg, #4facfe 0%, #00f2fe 100%)', // Xanh dương
  'linear-gradient(135deg, #fa709a 0%, #fee140 100%)', // Hồng cam
  'linear-gradient(135deg, #30cfd0 0%, #330867 100%)', // Xanh lam đậm
  'linear-gradient(135deg, #a8edea 0%, #fed6e3 100%)', // Pastel
  'linear-gradient(135deg, #ffecd2 0%, #fcb69f 100%)', // Cam nhạt
  'linear-gradient(135deg, #ff9a9e 0%, #fecfef 100%)', // Hồng nhạt
  'linear-gradient(135deg, #fbc2eb 0%, #a6c1ee 100%)', // Tím nhạt
  'linear-gradient(135deg, #fdcbf1 0%, #e6dee9 100%)', // Hồng tím
  'linear-gradient(135deg, #89f7fe 0%, #66a6ff 100%)', // Xanh da trời
];

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

  // Chọn màu gradient dựa trên ID phòng để đảm bảo nhất quán
  const getGradientColor = () => {
    const roomId = typeof room.id === 'string' ? parseInt(room.id) : room.id;
    const index = roomId % gradientColors.length;
    return gradientColors[index];
  };

  return (
    <div 
      className={`room-card ${!room.canJoin ? 'disabled' : ''}`} 
      onClick={handleClick}
      style={{ cursor: room.canJoin ? 'pointer' : 'not-allowed' }}
    >
      <div className="room-icon" style={{ background: getGradientColor() }}>
        <IoGameController className="icon" />
      </div>
      <div className="room-info">
        <h3 className="room-name">{room.name || `Phòng #${room.id}`}</h3>
        <div className="room-details">
          <span className="detail-item">
            <FaUsers className="icon" />
            {room.currentPlayers || 0}/{room.maxPlayers || 8}
          </span>
          <span className="detail-item">
            <MdCircle className="icon" style={{ color: getStateColor(room.state) }} />
            <span style={{ color: getStateColor(room.state) }}>
              {room.stateText || 'Không xác định'}
            </span>
          </span>
          <span className="detail-item">
            <FaCrown className="icon" />
            {room.isOfficial ? 'Hệ thống' : (room.ownerUsername || `User ${room.ownerId}`)}
          </span>
        </div>
      </div>
      {room.isOfficial && (
        <div className="official-badge">
          <FaCheck />
        </div>
      )}
    </div>
  );
}

