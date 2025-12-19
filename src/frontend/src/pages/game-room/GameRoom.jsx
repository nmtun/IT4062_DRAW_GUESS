import React, { useState, useEffect, useMemo, useRef } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import Canvas from '../../components/Canvas';
import ChatPanel from '../../components/ChatPanel';
import PlayerList from '../../components/PlayerList';
import { useAuth } from '../../hooks/useAuth';
import { getServices } from '../../services/Services';
import './GameRoom.css';

const DEFAULT_ROUND_TIME = 30;

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
  const canvasApiRef = useRef(null);
  const myUserId = useMemo(() => {
    const n = Number(user?.id);
    return Number.isFinite(n) ? n : null;
  }, [user?.id]);
  const myUserIdRef = useRef(null);
  const userAvatarRef = useRef(null);

  useEffect(() => {
    myUserIdRef.current = myUserId;
    userAvatarRef.current = user?.avatar || null;
  }, [myUserId, user?.avatar]);

  const [isDrawing, setIsDrawing] = useState(externalIsDrawing); // current user can draw
  const [gameState, setGameState] = useState(externalGameState); // waiting|playing|finished
  const [timeLeft, setTimeLeft] = useState(externalTimeLeft);
  const [word, setWord] = useState(externalWord); // drawer sees word, others see ""
  const [wordLength, setWordLength] = useState(0);
  const [timeLimit, setTimeLimit] = useState(DEFAULT_ROUND_TIME);
  const [players, setPlayers] = useState(externalPlayers);
  const [maxPlayers, setMaxPlayers] = useState(10);
  const [chatMessages, setChatMessages] = useState(messages);
  const [roundStartMs, setRoundStartMs] = useState(0);
  const [currentDrawerId, setCurrentDrawerId] = useState(null);
  const timerRef = useRef(null);

  const isOwner = useMemo(() => {
    if (myUserId == null) return false;
    return !!players.find(p => p.id === myUserId && p.isOwner);
  }, [players, myUserId]);

  const displayWord = useMemo(() => {
    if (gameState !== 'playing') return '';
    if (isDrawing && word) return word;
    const len = wordLength || (word ? word.length : 0);
    if (!len) return '';
    return Array.from({ length: len }).map(() => '_').join(' ');
  }, [gameState, isDrawing, word, wordLength]);

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

      const meId = myUserIdRef.current;
      const myAvatar = userAvatarRef.current;
      const mapped = (data.players || []).map(p => ({
        id: typeof p.user_id === 'number' ? p.user_id : parseInt(p.user_id, 10),
        username: p.username,
        avatar: (typeof p.user_id === 'number' ? p.user_id : parseInt(p.user_id, 10)) === meId ? myAvatar || '👤' : '👤',
        score: 0,             // cập nhật từ game state khi có
        isDrawing: currentDrawerId != null && (typeof p.user_id === 'number' ? p.user_id : parseInt(p.user_id, 10)) === currentDrawerId,
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

    const handleGameStart = (data) => {
      // data: { drawer_id, word_length, time_limit, round_start_ms, word }
      if (!data) return;
      setGameState('playing');
      setWordLength(typeof data.word_length === 'number' ? data.word_length : 0);
      const tl = typeof data.time_limit === 'number' ? data.time_limit : DEFAULT_ROUND_TIME;
      const startMs = typeof data.round_start_ms === 'number' ? data.round_start_ms : Date.now();
      setRoundStartMs(startMs);
      setTimeLimit(tl);
      setTimeLeft(tl);

      const drawerId = typeof data.drawer_id === 'number' ? data.drawer_id : parseInt(data.drawer_id, 10);
      const meId = myUserIdRef.current;
      const amDrawer = meId != null && drawerId === meId;
      setCurrentDrawerId(drawerId);
      setIsDrawing(!!amDrawer);
      setWord(amDrawer ? (data.word || '') : '');

      // update player drawing badge
      setPlayers((prev) => (prev || []).map(p => ({ ...p, isDrawing: p.id === drawerId })));

      // clear canvas on each new round
      try { canvasApiRef.current?.clear?.(); } catch (_) { }
    };

    const handleDrawBroadcast = (data) => {
      // data: { action, x1, y1, x2, y2, colorHex, width }
      if (!data) return;
      try { canvasApiRef.current?.applyRemoteDraw?.(data); } catch (_) { }
    };

    const handleCorrectGuess = (data) => {
      if (!data) return;
      setChatMessages((prev) => [
        ...(prev || []),
        { type: 'system', username: '', text: `Người chơi ${data.player_id} đoán đúng! (+${data.points || 0})` }
      ]);
    };

    const handleWrongGuess = (data) => {
      if (!data) return;
      setChatMessages((prev) => [
        ...(prev || []),
        { type: 'system', username: '', text: `Người chơi ${data.player_id} đoán: "${data.guess}"` }
      ]);
    };

    const handleChatBroadcast = (data) => {
      if (!data) return;
      setChatMessages((prev) => [
        ...(prev || []),
        { type: 'chat', username: data.username, text: data.message }
      ]);
    };

    const applyScores = (scores = []) => {
      setPlayers((prev) => {
        const map = new Map(scores.map(s => [s.user_id, s.score]));
        const updated = (prev || []).map(p => ({ ...p, score: map.get(p.id) ?? p.score ?? 0 }));
        // Sort leaderboard giảm dần theo điểm
        updated.sort((a, b) => (b.score || 0) - (a.score || 0));
        return updated;
      });
    };

    const handleRoundEnd = (data) => {
      // data: { word, scores: [{user_id, score}] }
      if (!data) return;
      if (Array.isArray(data.scores)) applyScores(data.scores);
      setChatMessages((prev) => [
        ...(prev || []),
        { type: 'system', username: '', text: `Hết round! Từ đúng là: "${data.word}"` }
      ]);
      setIsDrawing(false);
      setWord('');
      setWordLength(0);
    };

    const handleGameEnd = (data) => {
      if (!data) return;
      if (Array.isArray(data.scores)) applyScores(data.scores);
      setGameState('finished');
      setChatMessages((prev) => [
        ...(prev || []),
        { type: 'system', username: '', text: `Kết thúc game! Winner: ${data.winner_id}` }
      ]);
      setIsDrawing(false);
      setWord('');
      setWordLength(0);
    };

    const subscribe = () => {
      console.log('[GameRoom] Subscribing to room events...');
      
      // Global listener để debug tất cả messages
      services.subscribe('*', (message) => {
        console.log('[GameRoom] Received any message:', message);
      });
      
      services.subscribe('room_players_update', handleRoomPlayersUpdate);
      services.subscribe('room_update', handleRoomUpdate);
      services.subscribe('game_start', handleGameStart);
      services.subscribe('draw_broadcast', handleDrawBroadcast);
      services.subscribe('correct_guess', handleCorrectGuess);
      services.subscribe('wrong_guess', handleWrongGuess);
      services.subscribe('chat_broadcast', handleChatBroadcast);
      services.subscribe('round_end', handleRoundEnd);
      services.subscribe('game_end', handleGameEnd);
      console.log('[GameRoom] Subscribed to room events');
    };

    const unsubscribe = () => {
      services.unsubscribe('*'); // Remove global listener
      services.unsubscribe('room_players_update', handleRoomPlayersUpdate);
      services.unsubscribe('room_update', handleRoomUpdate);
      services.unsubscribe('game_start', handleGameStart);
      services.unsubscribe('draw_broadcast', handleDrawBroadcast);
      services.unsubscribe('correct_guess', handleCorrectGuess);
      services.unsubscribe('wrong_guess', handleWrongGuess);
      services.unsubscribe('chat_broadcast', handleChatBroadcast);
      services.unsubscribe('round_end', handleRoundEnd);
      services.unsubscribe('game_end', handleGameEnd);
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

  // Đồng bộ countdown theo roundStartMs từ server (tất cả client sẽ hiển thị giống nhau)
  useEffect(() => {
    if (gameState !== 'playing') return;
    if (!roundStartMs) return;

    // clear old interval
    if (timerRef.current) clearInterval(timerRef.current);

    const tick = () => {
      const now = Date.now();
      const elapsedSec = Math.floor((now - roundStartMs) / 1000);
      const remain = Math.max(0, (timeLimit || DEFAULT_ROUND_TIME) - elapsedSec);
      setTimeLeft(remain);
    };

    tick();
    timerRef.current = setInterval(tick, 250);
    return () => {
      if (timerRef.current) clearInterval(timerRef.current);
      timerRef.current = null;
    };
  }, [gameState, roundStartMs, timeLimit]);

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
    const services = getServices();
    if (!drawData) return;
    if (drawData.action === 2) {
      services.sendClearCanvas();
      return;
    }
    if (drawData.action === 1) {
      services.sendDrawData(
        drawData.x1, drawData.y1, drawData.x2, drawData.y2,
        drawData.color,
        drawData.width,
        !!drawData.isEraser
      );
    }
  };

  const handleSendMessage = (text) => {
    const services = getServices();
    services.sendChatMessage(text);
    setChatMessages((prev) => [
      ...(prev || []),
      { type: 'self', username: user?.username || 'Me', text }
    ]);
  };

  const handleSendGuess = (guess) => {
    const services = getServices();
    services.guessWord(guess);
    setChatMessages((prev) => [
      ...(prev || []),
      { type: 'self', username: user?.username || 'Me', text: guess }
    ]);
  };

  const handleStartGame = () => {
    const services = getServices();
    services.startGame();
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
        </div>
        <div className="header-right">
          {gameState !== 'playing' && isOwner && players.length >= 2 && (
            <button className="back-btn" onClick={handleStartGame}>
              Bắt đầu
            </button>
          )}
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
              {gameState === 'waiting' && (
                <div className="status-banner waiting">
                  <h2>ĐANG CHỜ</h2>
                  <p>Chờ chủ phòng bắt đầu game...</p>
                </div>
              )}
              {gameState === 'playing' && isDrawing && (
                <div className="status-banner drawing">
                  <h2>BẠN ĐANG VẼ</h2>
                  <p className="word-display">{displayWord || '...'}</p>
                </div>
              )}
              {gameState === 'playing' && !isDrawing && (
                <div className="status-banner guessing">
                  <h2>ĐOÁN TỪ</h2>
                  <p className="word-display">{displayWord || '...'}</p>
                </div>
              )}
              {gameState === 'finished' && (
                <div className="status-banner waiting">
                  <h2>KẾT THÚC</h2>
                  <p>Game đã kết thúc.</p>
                </div>
              )}
            </div>
            <Canvas ref={canvasApiRef} canDraw={isDrawing && gameState === 'playing'} onDraw={handleDraw} />
          </section>

          {/* Right Panel - Chat */}
          <aside className="game-sidebar right">
            <ChatPanel
              messages={chatMessages}
              onSendMessage={handleSendMessage}
              onSendGuess={handleSendGuess}
              isWaiting={gameState !== 'playing' || isDrawing}
            />
          </aside>
        </div>
      </main>
    </div>
  );
}

