import React, { useState } from 'react';
import './CreateRoomDialog.css';

export default function CreateRoomDialog({ isOpen, onClose, onCreateRoom }) {
  const [roomName, setRoomName] = useState('');
  const [maxPlayers, setMaxPlayers] = useState(8);
  const [rounds, setRounds] = useState(3);
  const [isCreating, setIsCreating] = useState(false);

  const handleSubmit = async (e) => {
    e.preventDefault();
    
    if (!roomName.trim()) {
      alert('Vui lòng nhập tên phòng');
      return;
    }

    if (maxPlayers < 2 || maxPlayers > 10) {
      alert('Số người chơi phải từ 2 đến 10');
      return;
    }

    if (rounds < 1 || rounds > 10) {
      alert('Số vòng chơi phải từ 1 đến 10');
      return;
    }

    setIsCreating(true);
    try {
      await onCreateRoom(roomName.trim(), maxPlayers, rounds);
      // Reset form
      setRoomName('');
      setMaxPlayers(10);
      setRounds(3);
      onClose();
    } catch (error) {
      alert('Không thể tạo phòng. Vui lòng thử lại.');
    } finally {
      setIsCreating(false);
    }
  };

  const handleClose = () => {
    if (!isCreating) {
      setRoomName('');
      setMaxPlayers(10);
      setRounds(3);
      onClose();
    }
  };

  if (!isOpen) return null;

  return (
    <div className="create-room-overlay" onClick={handleClose}>
      <div className="create-room-dialog" onClick={(e) => e.stopPropagation()}>
        <div className="dialog-header">
          <h3>Tạo phòng mới</h3>
          <button 
            className="close-btn" 
            onClick={handleClose}
            disabled={isCreating}
          >
            ✕
          </button>
        </div>
        
        <form onSubmit={handleSubmit} className="dialog-form">
          <div className="form-group">
            <label htmlFor="roomName">Tên phòng:</label>
            <input
              type="text"
              id="roomName"
              value={roomName}
              onChange={(e) => setRoomName(e.target.value)}
              placeholder="Nhập tên phòng..."
              maxLength={31}
              disabled={isCreating}
              required
            />
          </div>

          <div className="form-group">
            <label htmlFor="maxPlayers">Số người chơi tối đa:</label>
            <select
              id="maxPlayers"
              value={maxPlayers}
              onChange={(e) => setMaxPlayers(parseInt(e.target.value))}
              disabled={isCreating}
            >
              {Array.from({length: 9}, (_, i) => i + 2).map(num => (
                <option key={num} value={num}>{num} người</option>
              ))}
            </select>
          </div>

          <div className="form-group">
            <label htmlFor="rounds">Số vòng chơi:</label>
            <select
              id="rounds"
              value={rounds}
              onChange={(e) => setRounds(parseInt(e.target.value))}
              disabled={isCreating}
            >
              {Array.from({length: 10}, (_, i) => i + 1).map(num => (
                <option key={num} value={num}>{num} vòng</option>
              ))}
            </select>
          </div>

          <div className="dialog-actions">
            <button
              type="button"
              onClick={handleClose}
              className="btn-cancel"
              disabled={isCreating}
            >
              Hủy
            </button>
            <button
              type="submit"
              className="btn-create"
              disabled={isCreating}
            >
              {isCreating ? 'Đang tạo...' : 'Tạo phòng'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}