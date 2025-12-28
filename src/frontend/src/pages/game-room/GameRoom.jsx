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
  const [category, setCategory] = useState(''); // Category của từ hiện tại
  const [timeLimit, setTimeLimit] = useState(DEFAULT_ROUND_TIME);
  const [players, setPlayers] = useState(externalPlayers);
  const [maxPlayers, setMaxPlayers] = useState(10);
  const [chatMessages, setChatMessages] = useState(messages);
  const [roundStartMs, setRoundStartMs] = useState(0);
  const [currentDrawerId, setCurrentDrawerId] = useState(null);
  const [currentRound, setCurrentRound] = useState(0);
  const [playerCount, setPlayerCount] = useState(0);
  const [totalRounds, setTotalRounds] = useState(0);
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

  // Tính vòng hiện tại: vòng = Math.floor((current_round - 1) / player_count) + 1
  const currentCycle = useMemo(() => {
    if (currentRound <= 0 || playerCount <= 0) {
      return 0;
    }
    const cycle = Math.floor((currentRound - 1) / playerCount) + 1;
    return cycle;
  }, [currentRound, playerCount]);

  // Tính tổng số vòng: total_rounds từ room (số vòng gốc)
  const totalCycles = useMemo(() => {
    return totalRounds > 0 ? totalRounds : 0;
  }, [totalRounds]);

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

      // Cập nhật player_count
      if (typeof data.player_count === 'number') {
        setPlayerCount(data.player_count);
      }

      const meId = myUserIdRef.current;
      const myAvatar = userAvatarRef.current;
      const mapped = (data.players || []).map(p => {
        const playerId = typeof p.user_id === 'number' ? p.user_id : parseInt(p.user_id, 10);
        // Sử dụng avatar từ server, nếu là current user thì ưu tiên myAvatar từ localStorage
        let playerAvatar = p.avatar || 'avt1.jpg';
        if (playerId === meId && myAvatar) {
          playerAvatar = myAvatar;
        }
        
        // Tìm player hiện tại để giữ lại score (không reset về 0)
        const existingPlayer = players.find(ep => ep.id === playerId);
        const currentScore = existingPlayer ? (existingPlayer.score || 0) : 0;
        
        return {
          id: playerId,
          username: p.username,
          avatar: playerAvatar,
          score: currentScore,  // Giữ lại score hiện tại, không reset về 0
          isDrawing: currentDrawerId != null && playerId === currentDrawerId,
          isOwner: p.is_owner === 1
        };
      });

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
      // data: { drawer_id, word_length, time_limit, round_start_ms, current_round, player_count, total_rounds, word, category }
      if (!data) return;
      setGameState('playing');
      setWordLength(typeof data.word_length === 'number' ? data.word_length : 0);
      const tl = typeof data.time_limit === 'number' ? data.time_limit : DEFAULT_ROUND_TIME;
      const startMs = typeof data.round_start_ms === 'number' ? data.round_start_ms : Date.now();
      setRoundStartMs(startMs);
      setTimeLimit(tl);
      setTimeLeft(tl);

      // Cập nhật thông tin vòng
      const cr = typeof data.current_round === 'number' && !isNaN(data.current_round) ? data.current_round : 0;
      const pc = typeof data.player_count === 'number' && !isNaN(data.player_count) ? data.player_count : (players.length > 0 ? players.length : 0);
      const tr = typeof data.total_rounds === 'number' && !isNaN(data.total_rounds) ? data.total_rounds : 0;
      
      setCurrentRound(cr);
      setPlayerCount(pc);
      setTotalRounds(tr);

      const drawerId = typeof data.drawer_id === 'number' ? data.drawer_id : parseInt(data.drawer_id, 10);
      const meId = myUserIdRef.current;
      const amDrawer = meId != null && drawerId === meId;
      setCurrentDrawerId(drawerId);
      setIsDrawing(!!amDrawer);
      setWord(amDrawer ? (data.word || '') : '');
      
      // Set category (tất cả người chơi đều nhận category)
      setCategory(data.category || '');


      // update player drawing badge
      setPlayers((prev) => (prev || []).map(p => ({ ...p, isDrawing: p.id === drawerId })));

      // clear canvas on each new round
      try { 
        canvasApiRef.current?.clear?.(); 
      } catch {
        // Ignore errors when clearing canvas
      }
    };

    const handleDrawBroadcast = (data) => {
      // data: { action, x1, y1, x2, y2, colorHex, width }
      if (!data) return;
      try { 
        canvasApiRef.current?.applyRemoteDraw?.(data); 
      } catch (err) {
        // Ignore errors when applying remote draw
        console.warn('Error applying remote draw:', err);
      }
    };

    const handleCorrectGuess = (data) => {
      if (!data) return;
      console.log('[GameRoom] handleCorrectGuess received:', data);
      const meId = myUserIdRef.current;
      const isMe = meId != null && data.player_id === meId;
      const guesserPoints = data.guesser_points || data.points || 0;
      const drawerPoints = data.drawer_points || 0;
      
      // Cập nhật điểm: người đoán đúng và người vẽ (mỗi lần có người đoán đúng)
      setPlayers((prev) => {
        // Lấy username từ data (server gửi) hoặc từ players state hiện tại
        let guesserUsername = data.username;
        console.log('[GameRoom] Username from data:', guesserUsername);
        if (!guesserUsername || guesserUsername.trim() === '') {
          // Fallback: tìm từ players state hiện tại
          const guesserPlayer = prev.find(p => p.id === data.player_id);
          guesserUsername = guesserPlayer?.username || `Người chơi ${data.player_id}`;
          console.log('[GameRoom] Username from players state:', guesserPlayer?.username, 'Final:', guesserUsername);
        }
        
        // Hiển thị thông báo
        if (isMe) {
          setChatMessages((chatPrev) => [
            ...(chatPrev || []),
            { type: 'system', username: '', text: '🎉 Bạn đã đoán đúng! (+' + guesserPoints + ' điểm)' }
          ]);
        } else {
          setChatMessages((chatPrev) => [
            ...(chatPrev || []),
            { type: 'system', username: '', text: `${guesserUsername} đã đoán đúng! (+${guesserPoints} điểm)` }
          ]);
        }
        
        const updated = (prev || []).map(p => {
          if (p.id === data.player_id) {
            // Cộng điểm cho người đoán đúng
            return { ...p, score: (p.score || 0) + guesserPoints };
          } else if (p.isDrawing) {
            // Cộng điểm cho người vẽ (mỗi lần có người đoán đúng)
            return { ...p, score: (p.score || 0) + drawerPoints };
          }
          return p;
        });
        
        // Sort leaderboard giảm dần theo điểm
        updated.sort((a, b) => (b.score || 0) - (a.score || 0));
        return updated;
      });
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
      setCategory('');
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
      setCategory('');
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
    // Luôn gửi chat message, server sẽ tự động xử lý như guess nếu đang chơi và không phải drawer
    services.sendChatMessage(text);
    // Không thêm vào chatMessages ngay vì server sẽ broadcast lại
    // Chỉ thêm nếu là tin nhắn chat thông thường (không phải guess)
  };

  const handleStartGame = () => {
    const services = getServices();
    services.startGame();
  };

  return (
    <div className="game-room-page">
      {/* Main Game Area */}
      <main className="game-main">
        {/* Header thông tin chung */}
        <div className="game-info-bar">
          <button className="leave-btn" onClick={handleLeaveRoom}>
            Rời phòng
          </button>
          <div className="word-display-area">
            {gameState === 'playing' && (
              <div className="word-display-box">
                {category && (
                  <div className="category-display">Chủ đề: {category}</div>
                )}
                {isDrawing ? (
                  <>
                    <span className="word-label">Từ của bạn:</span>
                    <span className="word-text">{displayWord || '...'}</span>
                  </>
                ) : (
                  <>
                    <span className="word-label">Đoán từ:</span>
                    <span className="word-text">{displayWord || '...'}</span>
                  </>
                )}
              </div>
            )}
            {gameState === 'waiting' && (
              <div className="word-display-box waiting-text">
                Chờ bắt đầu...
              </div>
            )}
            {gameState === 'finished' && (
              <div className="word-display-box finished-text">
                Kết thúc
              </div>
            )}
          </div>
          <div className="game-info-right">
            <div className="round-info">
              <span className="round-label">Vòng:</span>
              <span className="round-value">
                {currentCycle > 0 ? currentCycle : (currentRound > 0 && playerCount > 0 ? Math.floor((currentRound - 1) / playerCount) + 1 : '?')}
              </span>
              {totalCycles > 0 && (
                <span className="round-total">/ {totalCycles}</span>
              )}
            </div>
            <div className="timer">
              <span className="timer-icon">⏱️</span>
              <span className="timer-text">{timeLeft}s</span>
            </div>
          </div>
        </div>

        <div className="game-layout">
          {/* Left Panel - Player List */}
          <aside className="game-sidebar left">
            <div className="player-list-wrapper">
              <PlayerList players={players} currentUserId={user?.id} maxPlayers={maxPlayers} />
              {gameState !== 'playing' && isOwner && players.length >= 2 && (
                <button className="start-game-btn" onClick={handleStartGame}>
                  Bắt đầu
                </button>
              )}
            </div>
          </aside>

          {/* Center Panel - Canvas */}
          <section className="game-center">
            <Canvas 
              ref={canvasApiRef} 
              canDraw={isDrawing && gameState === 'playing'} 
              onDraw={handleDraw}
              isWaiting={gameState === 'waiting'}
            />
          </section>

          {/* Right Panel - Chat */}
          <aside className="game-sidebar right">
            <ChatPanel
              messages={chatMessages}
              onSendMessage={handleSendMessage}
              isWaiting={gameState !== 'playing' || isDrawing}
            />
          </aside>
        </div>
      </main>
    </div>
  );
}

