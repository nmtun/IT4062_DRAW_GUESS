import React, { useState, useEffect, useRef } from 'react';
import RoomCard from '../../components/RoomCard';
import CreateRoomDialog from '../../components/CreateRoomDialog';
import { useAuth } from '../../hooks/useAuth';
import { useNavigate } from 'react-router-dom';
import { clearUserData } from '../../utils/userStorage';
import { getServices } from '../../services/Services';
import './Lobby.css';

export default function Lobby({ onJoinRoom, onCreateRoom, rooms = [] }) {
  const [searchTerm, setSearchTerm] = useState('');
  const [roomsList, setRoomsList] = useState([]);
  const [isCreateDialogOpen, setIsCreateDialogOpen] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [isLoadingRooms, setIsLoadingRooms] = useState(true);
  const [error, setError] = useState(null);
  const timeoutRef = useRef(null); 
  const { user } = useAuth();
  const navigate = useNavigate();

  // Lọc phòng theo từ khóa tìm kiếm
  const filteredRooms = roomsList.filter(room =>
    room.name.toLowerCase().includes(searchTerm.toLowerCase())
  );

  // Hiển thị rooms từ server hoặc từ props
  const displayRooms = rooms.length > 0 ? rooms : filteredRooms;

  // Kết nối và lắng nghe events từ server
  useEffect(() => {
    const services = getServices();

    // Đăng ký lắng nghe events TRƯỚC (an toàn)
    const handleCreateRoomResponse = (data) => {
      setIsLoading(false);
      if (data.status === 'success') {
        if (data.room_id) {
          navigate(`/game/${data.room_id}`);
        }
      } else {
        alert(data.message || 'Không thể tạo phòng');
      }
    };

    const handleJoinRoomResponse = (data) => {
      if (data.status === 'success') {
        if (data.room_id) {
          navigate(`/game/${data.room_id}`);
        }
      } else {
        alert(data.message || 'Không thể tham gia phòng');
      }
    };

    const handleRoomListResponse = (data) => {
      // clear timeout khi có phản hồi
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
        timeoutRef.current = null;
      }
      setIsLoadingRooms(false);
      setError(null);
      if (data.rooms) {
        const formattedRooms = data.rooms.map(room => {
          const getStateText = (state) => {
            switch (state) {
              case 0: return 'Chờ';
              case 1: return 'Đang chơi';
              case 2: return 'Kết thúc';
              default: return 'Không xác định';
            }
          };
          return {
            id: room.room_id.toString(),
            name: room.room_name,
            currentPlayers: room.player_count,
            maxPlayers: room.max_players,
            state: room.state,
            stateText: getStateText(room.state),
            ownerId: room.owner_id,
            canJoin: room.state === 0 && room.player_count < room.max_players
          };
        });
        setRoomsList(formattedRooms);
      } else {
        setRoomsList([]);
      }
    };

    const handleError = (data) => {
      // clear timeout khi có lỗi
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
        timeoutRef.current = null;
      }
      setIsLoadingRooms(false);
      setIsLoading(false);
      setError(data.message || 'Đã xảy ra lỗi');
    };

    services.subscribe('create_room_response', handleCreateRoomResponse);
    services.subscribe('join_room_response', handleJoinRoomResponse);
    services.subscribe('room_list_response', handleRoomListResponse);
    services.subscribe('error', handleError);

    // Kết nối và CHỈ load danh sách sau khi connect xong
    (async () => {
      try {
        await services.connect();
        loadRoomList(); // kết nối xong mới gửi request
      } catch (error) {
        console.error('Connection error:', error);
        setError('Không thể kết nối đến server. Vui lòng thử lại.');
        setIsLoadingRooms(false);
      }
    })();

    return () => {
      services.unsubscribe('create_room_response', handleCreateRoomResponse);
      services.unsubscribe('join_room_response', handleJoinRoomResponse);
      services.unsubscribe('room_list_response', handleRoomListResponse);
      services.unsubscribe('error', handleError);
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
        timeoutRef.current = null;
      }
    };
  }, [navigate]);

  const loadRoomList = () => {
    const services = getServices();
    // reset timeout cũ
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
      timeoutRef.current = null;
    }

    setIsLoadingRooms(true);
    setError(null);

    // Timeout fallback: chỉ clear trong các handler khi có phản hồi
    timeoutRef.current = setTimeout(() => {
      setIsLoadingRooms(false);
      setError('Timeout: Server không phản hồi');
      timeoutRef.current = null;
    }, 10000);

    services.getRoomList();
  };

  const handleJoinRoom = (roomId) => {
    const services = getServices();
    if (onJoinRoom) {
      onJoinRoom(roomId);
      navigate(`/game/${roomId}`);
    } else {
      services.joinRoom(roomId);
    }
  };

  const handleCreateRoom = async (roomName, maxPlayers, rounds) => {
    const services = getServices();
    setIsLoading(true);

    try {
      // Gửi yêu cầu tạo phòng
      const sent = services.createRoom(roomName, maxPlayers, rounds);
      if (!sent) {
        setIsLoading(false);
        alert('Không thể kết nối đến server');
      }
    } catch (error) {
      setIsLoading(false);
      console.error('Error creating room:', error);
      alert('Có lỗi xảy ra khi tạo phòng');
    }
  };

  const openCreateDialog = () => {
    if (onCreateRoom) {
      onCreateRoom();
    } else {
      setIsCreateDialogOpen(true);
    }
  };

  const handleLogout = () => {
    getServices().logout();
    getServices().disconnect();
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
              {(user?.username ? user.username.replace(/[\x00-\x1F]/g, '') : 'Guest')}
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
          {error ? (
            <div className="error-message">
              <p>{error}</p>
              <button onClick={loadRoomList} className="retry-btn">
                Thử lại
              </button>
            </div>
          ) : isLoadingRooms ? (
            <div className="loading-message">
              <p>Đang tải danh sách phòng...</p>
            </div>
          ) : displayRooms.length > 0 ? (
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
        <button className="btn-new-room" onClick={openCreateDialog} disabled={isLoading}>
          {isLoading ? 'ĐANG TẠO...' : 'PHÒNG MỚI'}
        </button>
        <button className="btn-play" onClick={loadRoomList}>
          LÀM MỚI
        </button>
      </div>

      {/* Dialog tạo phòng */}
      <CreateRoomDialog
        isOpen={isCreateDialogOpen}
        onClose={() => setIsCreateDialogOpen(false)}
        onCreateRoom={handleCreateRoom}
      />
    </div>
  );
}

