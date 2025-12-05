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

  // Kết nối + join room và subscribe events
  useEffect(() => {
    if (!roomId) return;
    const services = getServices();

    const handleRoomPlayersUpdate = (data) => {
      // data từ gateway/index.js: { room_id, players: [{user_id, username, is_owner}], player_count, ... }
      if (!data || data.room_id?.toString() !== roomId?.toString()) return;

      const mapped = (data.players || []).map(p => ({
        id: p.user_id,
        username: p.username,
        avatar: p.user_id === user?.id ? user?.avatar || '👤' : '👤',  // Sử dụng avatar của user hiện tại nếu có
        score: 0,             // cập nhật từ game state khi có
        isDrawing: false,     // cập nhật theo game events khi có
        isOwner: p.is_owner === 1
      }));
      setPlayers(mapped);
    };

    const handleRoomUpdate = (data) => {
      // Có thể dùng để cập nhật max_players hoặc state phòng
      if (!data || data.room_id?.toString() !== roomId?.toString()) return;
      // Ví dụ: setGameState theo data.state nếu có mapping
      // state: 0=waiting,1=playing,2=finished
      const states = { 0: 'waiting', 1: 'playing', 2: 'finished' };
      setGameState(states[data.state] || 'waiting');
    };

    const subscribe = () => {
      services.subscribe('room_players_update', handleRoomPlayersUpdate);
      services.subscribe('room_update', handleRoomUpdate);
    };

    const unsubscribe = () => {
      services.unsubscribe('room_players_update', handleRoomPlayersUpdate);
      services.unsubscribe('room_update', handleRoomUpdate);
    };

    // Đảm bảo dùng cùng kết nối đã đăng nhập
    services.connect()
      .then(() => {
        // Join room để server broadcast danh sách người chơi hiện tại
        services.joinRoom(parseInt(roomId, 10));
        subscribe();
      })
      .catch((err) => {
        console.error('Connect error:', err);
      });

    return () => {
      // Rời phòng và cleanup
      unsubscribe();
      services.leaveRoom(parseInt(roomId, 10));
    };
  }, [roomId]);

  const handleLeaveRoom = () => {
    const services = getServices();
    if (roomId) {
      services.leaveRoom(parseInt(roomId, 10));
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
            <PlayerList players={players} currentUserId={user?.id} />
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

