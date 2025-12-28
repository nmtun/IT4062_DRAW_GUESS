import React, { useState } from 'react';
import './ChatPanel.css';

export default function ChatPanel({ messages = [], onSendMessage, isWaiting = false }) {
  const [inputValue, setInputValue] = useState('');

  const displayMessages = messages;

  const handleSend = (e) => {
    e.preventDefault();
    if (!inputValue.trim()) return;

    // Luôn gửi chat message, server sẽ tự động xử lý như guess nếu đang chơi
    if (onSendMessage) {
      onSendMessage(inputValue);
    }
    setInputValue('');
  };

  return (
    <div className="chat-panel">
      <div className="chat-header">
        <h3>CHAT</h3>
      </div>

      <div className="chat-messages">
        {displayMessages.length > 0 ? (
          displayMessages.map((msg, index) => (
            <div key={index} className={`message ${msg.type}`}>
              {msg.username && <span className="message-username">{msg.username}:</span>}
              <span className="message-text">{msg.text}</span>
            </div>
          ))
        ) : (
          <div className="message system">
            <span className="message-text">{isWaiting ? 'Đang chờ người chơi' : 'Chưa có tin nhắn nào'}</span>
          </div>
        )}
      </div>

      <form className="chat-input-form" onSubmit={handleSend}>
        <input
          type="text"
          className="chat-input"
          placeholder={isWaiting ? 'Nhập tin nhắn...' : 'Nhập đáp án hoặc tin nhắn...'}
          value={inputValue}
          onChange={(e) => setInputValue(e.target.value)}
        />
        <button type="submit" className="chat-send-btn">
          Gửi
        </button>
      </form>
    </div>
  );
}

