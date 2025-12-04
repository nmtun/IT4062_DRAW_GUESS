import React, { useState } from 'react';
import RoomCard from '../../components/RoomCard';
import { useAuth } from '../../hooks/useAuth';
import { useNavigate } from 'react-router-dom';
import { clearUserData } from '../../utils/userStorage';
import { getAuthService } from '../../services/AuthService';
import './Lobby.css';

// Mock rooms data - sẽ được thay thế bằng data từ server
const MOCK_ROOMS = [
  { id: '1pZ', name: 'Tổng quát #1pZ', currentPlayers: 3, maxPlayers: 15, score: 64, maxScore: 120, isOfficial: true },
  { id: '3SM', name: 'Tổng quát #3SM', currentPlayers: 3, maxPlayers: 15, score: 64, maxScore: 120, isOfficial: true },
  { id: 'iVs', name: 'Khác/ Tổng quát #iVs', currentPlayers: 0, maxPlayers: 5, score: 0, maxScore: 180 },
  { id: '6YN', name: 'Khác/ Tổng quát #6YN', currentPlayers: 0, maxPlayers: 5, score: 0, maxScore: 180 },
  { id: '2Rr', name: 'Minecraft #2Rr', currentPlayers: 2, maxPlayers: 8, score: 45, maxScore: 100 },
  { id: '2aw', name: 'Minecraft #2aw', currentPlayers: 1, maxPlayers: 8, score: 30, maxScore: 100 },
  { id: '2Df', name: 'Youtubers #2Df', currentPlayers: 4, maxPlayers: 10, score: 80, maxScore: 150 },
  { id: '2Zv', name: 'Thức ăn #2Zv', currentPlayers: 2, maxPlayers: 8, score: 25, maxScore: 120 },
  { id: '4Gh', name: 'Thức ăn #4Gh', currentPlayers: 5, maxPlayers: 8, score: 60, maxScore: 120 },
  { id: '5Jk', name: 'Động vật #5Jk', currentPlayers: 3, maxPlayers: 10, score: 50, maxScore: 150 },
  { id: '7Lm', name: 'Động vật #7Lm', currentPlayers: 0, maxPlayers: 10, score: 0, maxScore: 150 },
  { id: '8Np', name: 'Phương tiện giao thông #8Np', currentPlayers: 1, maxPlayers: 5, score: 15, maxScore: 80 },
];

export default function Lobby({ onJoinRoom, onCreateRoom, rooms = [] }) {
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedLanguage, setSelectedLanguage] = useState('VI');
  const [selectedTopic, setSelectedTopic] = useState('TAT CA');
  const { user } = useAuth();
  const navigate = useNavigate();

  // Sử dụng rooms từ props, nếu không có thì dùng mock data
  const displayRooms = rooms.length > 0 ? rooms : MOCK_ROOMS;

  const handleJoinRoom = (roomId) => {
    if (onJoinRoom) {
      onJoinRoom(roomId);
    }
  };

  const handleCreateRoom = () => {
    if (onCreateRoom) {
      onCreateRoom();
    }
  };

  const handleLogout = () => {
    getAuthService().logout();
    getAuthService().disconnect();
    clearUserData();
    navigate('/');
  }

  return (
    <div className="lobby-page">
      {/* Header */}
      <header className="lobby-header">
        <div className="header-left">
          <div className="user-info">
            <div className="lobby-avatar">
              <img
                src={`/src/assets/avt/${user?.avatar || 'avt1.jpg'}`}
                alt="Avatar"
                className="lobby-avatar-img"
                onError={(e) => {
                  e.target.src = '/src/assets/avt/avt1.jpg';
                }}
              />
            </div>
            <span className="username">
              {(user?.username ? user.username.replace(/^\u0001/, '') : 'Guest')}
            </span>
          </div>
        </div>
        <div className="header-center">
          <h1 className="lobby-logo">Draw & Guess</h1>
        </div>
        <div className="header-right">
          <div className="btn-logout">
            <button onClick={() => { handleLogout(); }}>
              Đăng xuất
            </button>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="lobby-main">

        {/* Rooms Title and Search */}
        <div className="rooms-header">
          <div className="rooms-title">
            <h2>Các phòng</h2>
          </div>
          <div className="search-container">
            <input
              type="text"
              className="search-input"
              placeholder="Tìm kiếm phòng ..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
            />
            <span className="search-icon">🔍</span>
          </div>
        </div>

        {/* Room List */}
        <div className="rooms-grid">
          {displayRooms.length > 0 ? (
            displayRooms.map((room) => (
              <RoomCard
                key={room.id}
                room={room}
                onJoin={handleJoinRoom}
              />
            ))
          ) : (
            <div className="no-rooms-message">
              <p>Chưa có phòng nào. Hãy tạo phòng mới!</p>
            </div>
          )}
        </div>
      </main>
      {/* Action Buttons */}
      <div className="lobby-actions">
        <button className="btn-new-room" onClick={handleCreateRoom}>
          PHÒNG MỚI
        </button>
        <button className="btn-play">
          CHƠI
        </button>
      </div>
    </div>
  );
}

