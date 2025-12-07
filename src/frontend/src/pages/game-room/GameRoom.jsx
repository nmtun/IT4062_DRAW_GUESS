import React, { useState, useEffect } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import Canvas from '../../components/Canvas';
import ChatPanel from '../../components/ChatPanel';
import PlayerList from '../../components/PlayerList';
import { useAuth } from '../../hooks/useAuth';
import { getServices } from '../../services/Services';
import './GameRoom.css';

const DEFAULT_ROUND_TIME = 90;

export default function GameRoom({
  onLeaveRoom,
  players: externalPlayers = [],
  messages = [],
  gameState: externalGameState = 'waiting',
  timeLeft: externalTimeLeft = DEFAULT_ROUND_TIME,
  word: externalWord = '',
  isDrawing: externalIsDrawing = false
}) {
  const { roomId } = useParams();
  const navigate = useNavigate();
  const { user } = useAuth();

  const [isDrawing, setIsDrawing] = useState(externalIsDrawing);
  const [gameState, setGameState] = useState(externalGameState);
  const [timeLeft, setTimeLeft] = useState(externalTimeLeft);
  const [word, setWord] = useState(externalWord);
  const [players, setPlayers] = useState(externalPlayers);
  const [maxPlayers, setMaxPlayers] = useState(10);

  // Kết nối + join room và subscribe events
  useEffect(() => {
    if (!roomId) return;
    const services = getServices();

    const handleRoomPlayersUpdate = (data) => {
      console.log('Received room_players_update:', data);
      if (!data || data.room_id?.toString() !== roomId?.toString()) {
        return;
      }

      // Cập nhật maxPlayers nếu có
      if (typeof data.max_players === 'number') {
        setMaxPlayers(data.max_players);
      }

      const mapped = (data.players || []).map(p => ({
        id: p.user_id,
        username: p.username,
        avatar: p.user_id === user?.id ? user?.avatar || '👤' : '👤',  // Sử dụng avatar của user hiện tại nếu có
        score: 0,             // cập nhật từ game state khi có
        isDrawing: false,     // cập nhật theo game events khi có
        isOwner: p.is_owner === 1
      }));

      console.log('Updated players:', mapped);
      setPlayers(mapped);
    };

    const handleRoomUpdate = (data) => {
      // Có thể dùng để cập nhật max_players hoặc state phòng
      if (!data || data.room_id?.toString() !== roomId?.toString()) return;
      // Ví dụ: setGameState theo data.state nếu có mapping
      // state: 0=waiting,1=playing,2=finished
      const states = { 0: 'waiting', 1: 'playing', 2: 'finished' };
      setGameState(states[data.state] || 'waiting');

      if (typeof data.max_players === 'number') {
        setMaxPlayers(data.max_players);
      }
    };

    const subscribe = () => {
      console.log('[GameRoom] Subscribing to room events...');
      
      // Global listener để debug tất cả messages
      services.subscribe('*', (message) => {
        console.log('[GameRoom] Received any message:', message);
      });
      
      services.subscribe('room_players_update', handleRoomPlayersUpdate);
      services.subscribe('room_update', handleRoomUpdate);
      console.log('[GameRoom] Subscribed to room events');
    };

    const unsubscribe = () => {
      services.unsubscribe('*'); // Remove global listener
      services.unsubscribe('room_players_update', handleRoomPlayersUpdate);
      services.unsubscribe('room_update', handleRoomUpdate);
    };

    // Đảm bảo dùng cùng kết nối đã đăng nhập
    console.log('[GameRoom] Connecting to services for room:', roomId);
    services.connect()
      .then(() => {
        console.log('[GameRoom] Connected to services successfully');
        // Đăng ký lắng nghe NGAY để không bỏ lỡ broadcast
        subscribe();
        
        const id = parseInt(roomId, 10);
        const current = services.currentRoomId;
        console.log('[GameRoom] Current room ID:', current, 'Target room ID:', id);
        
        // Kiểm tra cache trước tiên
        const cached = services.getCachedRoomUpdate(id);
        if (cached) {
          console.log('[GameRoom] Using cached room data:', cached);
          handleRoomPlayersUpdate(cached);
        }
        
        if (current !== id) {
          console.log('[GameRoom] Joining room:', id);
          // Join room - server sẽ broadcast room_players_update sau khi join thành công
          services.joinRoom(id).then((response) => {
            console.log('[GameRoom] Join room response:', response);
            // room_players_update sẽ được broadcast và handle bởi subscription
          }).catch((err) => {
            console.error('[GameRoom] Join room error:', err);
          });
        } else {
          console.log('[GameRoom] Already in room. Waiting for server broadcasts...');
          // Server sẽ tự broadcast room_players_update khi có thay đổi
          // Client chỉ cần chờ, không cần request thêm
        }
      })
      .catch((err) => {
        console.error('Connect error:', err);
      });

    return () => {
      // Chỉ cleanup subscriptions, KHÔNG tự động rời phòng
      // Việc rời phòng sẽ được xử lý explicit trong handleLeaveRoom
      console.log('[GameRoom] Cleaning up subscriptions only, not leaving room');
      unsubscribe();
    };
  }, [roomId]);

  const handleLeaveRoom = () => {
    console.log('[GameRoom] User explicitly leaving room:', roomId);
    const services = getServices();
    if (roomId) {
      services.leaveRoom(parseInt(roomId, 10)).then(() => {
        console.log('[GameRoom] Successfully left room');
      }).catch((err) => {
        console.warn('[GameRoom] Error leaving room:', err);
      });
    }
    if (onLeaveRoom) {
      onLeaveRoom();
    } else {
      navigate('/lobby');
    }
  };

  // const handleDraw = (drawData) => {
  //   console.log('Draw data:', drawData);
  // };

  // const handleSendMessage = (message) => {
  //   console.log('Send message:', message);
  // };

  // const handleSendGuess = (guess) => {
  //   console.log('Send guess:', guess);
  // };

  return (
    <div className="game-room-page">
      {/* Header */}
      <header className="game-header">
        <div className="header-left">
          <button className="back-btn" onClick={handleLeaveRoom}>
            Quay lại
          </button>
        </div>
        <div className="header-center">
          <div className="timer">
            <span className="timer-icon">⏱️</span>
            <span className="timer-text">{timeLeft}s</span>
          </div>
        </div>
        <div className="header-right">
        </div>
      </header>

      {/* Main Game Area */}
      <main className="game-main">
        <div className="game-layout">
          {/* Left Panel - Player List */}
          <aside className="game-sidebar left">
            <PlayerList players={players} currentUserId={user?.id} maxPlayers={maxPlayers} />
          </aside>

          {/* Center Panel - Canvas */}
          {/* <section className="game-center">
            <div className="game-status">
              {gameState === 'playing' && isDrawing && (
                <div className="status-banner drawing">
                  <h2>BẠN ĐANG VẼ</h2>
                  <p className="word-display">{word || 'Từ bí mật: ???'}</p>
                </div>
              )}
              {gameState === 'playing' && !isDrawing && (
                <div className="status-banner guessing">
                  <h2>ĐOÁN TỪ</h2>
                  <p>Hãy đoán từ mà người chơi đang vẽ!</p>
                </div>
              )}
            </div>
            <Canvas isDrawing={isDrawing && gameState === 'playing'} onDraw={handleDraw} />
          </section> */}

          {/* Right Panel - Chat */}
          {/* <aside className="game-sidebar right">
            <ChatPanel
              messages={messages}
              onSendMessage={handleSendMessage}
              onSendGuess={handleSendGuess}
            />
          </aside> */}
        </div>
      </main>
    </div>
  );
}

