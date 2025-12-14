import React, { useState, useEffect, useRef, useCallback } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import Canvas from '../../components/Canvas';
import ChatPanel from '../../components/ChatPanel';
import PlayerList from '../../components/PlayerList';
import { useAuth } from '../../hooks/useAuth';
import { getServices } from '../../services/Services';
import './GameRoom.css';

const DEFAULT_ROUND_TIME = 90;

// Mock data cho testing
const MOCK_WORDS = [
  'CON MÈO',
  'CON CHÓ',
  'NGÔI NHÀ',
  'CÂY CỐI',
  'MẶT TRỜI',
  'MẶT TRĂNG',
  'XE HƠI',
  'MÁY BAY',
  'BÀN GHẾ',
  'QUYỂN SÁCH',
  'BÚT CHÌ',
  'CỬA SỔ'
];

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
  const canvasRef = useRef(null);

  // Game state
  const [gameState, setGameState] = useState(externalGameState);
  const [timeLeft, setTimeLeft] = useState(externalTimeLeft);
  const [word, setWord] = useState(externalWord);
  const [players, setPlayers] = useState(externalPlayers);
  const [maxPlayers, setMaxPlayers] = useState(10);
  
  // Mock game data
  const [currentRound, setCurrentRound] = useState(1);
  const [totalRounds] = useState(3);
  const [currentDrawerId, setCurrentDrawerId] = useState(null);
  const [roundStartTime, setRoundStartTime] = useState(null); // Timestamp khi bắt đầu lượt
  
  // Drawing controls
  const [isDrawing, setIsDrawing] = useState(false); // Sẽ được set dựa trên currentDrawerId
  const [currentColor, setCurrentColor] = useState('#000000');
  const [brushSize, setBrushSize] = useState(5);
  const [isEraser, setIsEraser] = useState(false);

  // Hàm lấy từ theo lượt (mock data)
  const getWordForRound = (round) => {
    return MOCK_WORDS[(round - 1) % MOCK_WORDS.length];
  };

  // Hàm chọn người vẽ tiếp theo
  const selectNextDrawer = (playerList, round) => {
    if (!playerList || playerList.length === 0) return null;
    const index = (round - 1) % playerList.length;
    return playerList[index]?.id || null;
  };

  // Khởi tạo game với mock data
  const initializeMockGame = () => {
    if (players.length === 0) return;
    
    const drawerId = selectNextDrawer(players, currentRound);
    const mockWord = getWordForRound(currentRound);
    
    setCurrentDrawerId(drawerId);
    setWord(mockWord);
    setIsDrawing(drawerId === user?.id);
    setGameState('playing');
    setTimeLeft(DEFAULT_ROUND_TIME);
  };

  // Chuyển lượt tiếp theo
  const nextRound = useCallback(() => {
    setCurrentRound((prevRound) => {
      if (prevRound >= totalRounds) {
        // Game kết thúc
        setGameState('finished');
        setIsDrawing(false);
        return prevRound;
      }

      const newRound = prevRound + 1;
      
      // Lấy players hiện tại và tính toán drawer
      setPlayers((prevPlayers) => {
        const drawerId = selectNextDrawer(prevPlayers, newRound);
        const mockWord = getWordForRound(newRound);

        // Set các state khác
        const startTime = Date.now();
        setCurrentDrawerId(drawerId);
        setWord(mockWord);
        setIsDrawing(drawerId === user?.id);
        setRoundStartTime(startTime); // Reset timestamp khi chuyển lượt
        setTimeLeft(DEFAULT_ROUND_TIME);
        
        // Lưu vào localStorage để client vào sau có thể đồng bộ
        if (roomId) {
          localStorage.setItem(`roundStartTime_${roomId}_${newRound}`, startTime.toString());
        }
        
        // Xóa canvas khi chuyển lượt
        if (canvasRef.current && canvasRef.current.clearCanvas) {
          canvasRef.current.clearCanvas();
        }
        
        return prevPlayers;
      });
      
      return newRound;
    });
  }, [totalRounds, user?.id]);

  // Timer countdown - tính toán dựa trên timestamp để đồng bộ
  useEffect(() => {
    if (gameState !== 'playing' || !roundStartTime) return;

    const updateTimer = () => {
      const elapsed = Math.floor((Date.now() - roundStartTime) / 1000);
      const remaining = Math.max(0, DEFAULT_ROUND_TIME - elapsed);
      
      setTimeLeft(remaining);
      
      if (remaining <= 0) {
        // Hết thời gian, chuyển lượt
        nextRound();
      }
    };

    // Cập nhật ngay lập tức
    updateTimer();

    // Cập nhật mỗi giây
    const timer = setInterval(updateTimer, 1000);

    return () => clearInterval(timer);
  }, [gameState, roundStartTime, nextRound]);

  // Khởi tạo game khi có players
  useEffect(() => {
    // Chỉ khởi tạo khi có players và game chưa bắt đầu
    if (players.length > 0 && gameState === 'waiting' && !currentDrawerId) {
      const drawerId = selectNextDrawer(players, currentRound);
      const mockWord = getWordForRound(currentRound);
      
      const startTime = Date.now();
      setCurrentDrawerId(drawerId);
      setWord(mockWord);
      setIsDrawing(drawerId === user?.id);
      setGameState('playing');
      setRoundStartTime(startTime); // Lưu timestamp bắt đầu lượt
      setTimeLeft(DEFAULT_ROUND_TIME);
      
      // Lưu vào localStorage để client vào sau có thể đồng bộ
      if (roomId) {
        localStorage.setItem(`roundStartTime_${roomId}_${currentRound}`, startTime.toString());
      }
    }
  }, [players.length, gameState, currentDrawerId, currentRound, user?.id]);

  // Đảm bảo game được khởi tạo khi players được cập nhật từ server
  useEffect(() => {
    if (players.length > 0 && !currentDrawerId && gameState === 'waiting') {
      // Delay nhỏ để đảm bảo state đã được cập nhật
      const timer = setTimeout(() => {
        const drawerId = selectNextDrawer(players, currentRound);
        const mockWord = getWordForRound(currentRound);
        
        const startTime = Date.now();
        setCurrentDrawerId(drawerId);
        setWord(mockWord);
        setIsDrawing(drawerId === user?.id);
        setGameState('playing');
        setRoundStartTime(startTime); // Lưu timestamp bắt đầu lượt
        setTimeLeft(DEFAULT_ROUND_TIME);
        
        // Lưu vào localStorage để client vào sau có thể đồng bộ
        if (roomId) {
          localStorage.setItem(`roundStartTime_${roomId}_${currentRound}`, startTime.toString());
        }
      }, 100);
      
      return () => clearTimeout(timer);
    }
  }, [players, currentDrawerId, gameState, currentRound, user?.id]);

  // Cập nhật isDrawing khi currentDrawerId thay đổi
  useEffect(() => {
    if (currentDrawerId !== null) {
      setIsDrawing(currentDrawerId === user?.id);
    }
  }, [currentDrawerId, user?.id]);

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
      const prevPlayersCount = players.length;
      setPlayers(mapped);
      
      // Nếu client vào sau (players tăng từ 0 lên > 0) và game đã bắt đầu
      // Đồng bộ timer bằng cách lấy roundStartTime từ localStorage (nếu có)
      if (prevPlayersCount === 0 && mapped.length > 0 && gameState === 'playing' && !roundStartTime) {
        const storedStartTime = localStorage.getItem(`roundStartTime_${roomId}_${currentRound}`);
        if (storedStartTime) {
          const startTime = parseInt(storedStartTime, 10);
          const elapsed = Math.floor((Date.now() - startTime) / 1000);
          if (elapsed < DEFAULT_ROUND_TIME) {
            setRoundStartTime(startTime);
            setTimeLeft(DEFAULT_ROUND_TIME - elapsed);
          }
        }
      }
    };

    const handleRoomUpdate = (data) => {
      // Có thể dùng để cập nhật max_players hoặc state phòng
      if (!data || data.room_id?.toString() !== roomId?.toString()) return;
      // TODO: Tạm thời comment để test vẽ (sẽ bỏ comment khi có protocol start game)
      // Ví dụ: setGameState theo data.state nếu có mapping
      // state: 0=waiting,1=playing,2=finished
      // const states = { 0: 'waiting', 1: 'playing', 2: 'finished' };
      // setGameState(states[data.state] || 'waiting');

      if (typeof data.max_players === 'number') {
        setMaxPlayers(data.max_players);
      }
    };

    const handleDrawBroadcast = (data) => {
      console.log('[GameRoom] *** Received draw_broadcast ***');
      console.log('[GameRoom] draw_broadcast data:', data);
      // Render drawing từ server lên canvas để người khác thấy được
      if (canvasRef.current) {
        if (canvasRef.current.drawFromServer) {
          console.log('[GameRoom] Calling canvas.drawFromServer...');
          canvasRef.current.drawFromServer(data);
        } else {
          console.warn('[GameRoom] Canvas drawFromServer method not available');
        }
      } else {
        console.warn('[GameRoom] Canvas ref not available');
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
      
      // Subscribe draw_broadcast để nhận drawing từ người khác
      services.subscribe('draw_broadcast', handleDrawBroadcast);
      console.log('[GameRoom] Subscribed to room events including draw_broadcast');
      
      // Verify subscription
      console.log('[GameRoom] Subscription verification - draw_broadcast:', 
        services.callbacks && services.callbacks.has('draw_broadcast') ? 'YES' : 'NO');
    };

    const unsubscribe = () => {
      services.unsubscribe('*'); // Remove global listener
      services.unsubscribe('room_players_update', handleRoomPlayersUpdate);
      services.unsubscribe('room_update', handleRoomUpdate);
      services.unsubscribe('draw_broadcast', handleDrawBroadcast);
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

  const handleDraw = (drawData) => {
    if (!drawData) return;
    const services = getServices();
    console.log('[GameRoom] Sending draw data:', drawData);
    services.sendDrawData(
      drawData.x1,
      drawData.y1,
      drawData.x2,
      drawData.y2,
      drawData.color,
      drawData.width,
      drawData.isEraser
    );
  };

  const handleClearCanvas = () => {
    const services = getServices();
    services.sendClearCanvas();
    if (canvasRef.current && canvasRef.current.clearCanvas) {
      canvasRef.current.clearCanvas();
    }
  };

  const handleSendMessage = (message) => {
    console.log('Send message:', message);
  };

  const handleSendGuess = (guess) => {
    console.log('Send guess:', guess);
  };

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
          {gameState === 'playing' && (
            <div className="round-info">
              <span className="round-text">Lượt {currentRound}/{totalRounds}</span>
            </div>
          )}
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
          <section className="game-center">
            <div className="game-status">
              {gameState === 'waiting' && players.length === 0 && (
                <div className="status-banner waiting">
                  <h2>ĐANG CHỜ</h2>
                  <p>Đang chờ người chơi tham gia...</p>
                </div>
              )}
              {gameState === 'waiting' && players.length > 0 && !currentDrawerId && (
                <div className="status-banner waiting">
                  <h2>CHUẨN BỊ</h2>
                  <p>Đang chuẩn bị bắt đầu game...</p>
                </div>
              )}
              {gameState === 'playing' && isDrawing && (
                <div className="status-banner drawing">
                  <h2>BẠN ĐANG VẼ</h2>
                  <p className="word-display">{word || 'Từ bí mật: ???'}</p>
                  <p className="round-info-text">Lượt {currentRound}/{totalRounds}</p>
                </div>
              )}
              {gameState === 'playing' && !isDrawing && currentDrawerId && (
                <div className="status-banner guessing">
                  <h2>ĐOÁN TỪ</h2>
                  <p>Hãy đoán từ mà người chơi đang vẽ!</p>
                  <p className="drawer-info">
                    Người vẽ: {players.find(p => p.id === currentDrawerId)?.username || 'Đang vẽ...'}
                  </p>
                </div>
              )}
              {gameState === 'playing' && !isDrawing && !currentDrawerId && (
                <div className="status-banner waiting">
                  <h2>CHUẨN BỊ</h2>
                  <p>Đang chờ lượt tiếp theo...</p>
                </div>
              )}
              {gameState === 'finished' && (
                <div className="status-banner waiting">
                  <h2>TRÒ CHƠI KẾT THÚC</h2>
                  <p>Đã hoàn thành {totalRounds} lượt chơi!</p>
                </div>
              )}
            </div>
            <Canvas 
              ref={canvasRef}
              isDrawing={isDrawing && gameState === 'playing'}
              onDraw={handleDraw}
              color={currentColor}
              brushSize={brushSize}
              isEraser={isEraser}
              gameState={gameState}
            />
            {isDrawing && gameState === 'playing' && (
              <div className="drawing-controls">
                <div className="color-picker-group">
                  <label>Màu:</label>
                  <input 
                    type="color" 
                    value={currentColor} 
                    onChange={(e) => setCurrentColor(e.target.value)}
                    disabled={isEraser}
                  />
                </div>
                <div className="brush-size-group">
                  <label>Kích thước:</label>
                  <input 
                    type="range" 
                    min="1" 
                    max="20" 
                    value={brushSize} 
                    onChange={(e) => setBrushSize(parseInt(e.target.value))}
                  />
                  <span>{brushSize}px</span>
                </div>
                <button 
                  className={`eraser-btn ${isEraser ? 'active' : ''}`}
                  onClick={() => setIsEraser(!isEraser)}
                >
                  {isEraser ? '✏️ Bút' : '🧹 Tẩy'}
                </button>
                <button 
                  className="clear-btn"
                  onClick={handleClearCanvas}
                >
                  🗑️ Xóa
                </button>
              </div>
            )}
          </section>

          {/* Right Panel - Chat */}
          <aside className="game-sidebar right">
            <ChatPanel
              messages={messages}
              onSendMessage={handleSendMessage}
              onSendGuess={handleSendGuess}
            />
          </aside>
        </div>
      </main>
    </div>
  );
}

