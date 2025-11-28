import React from 'react';
import './AvatarSelector.css';

export default function AvatarSelector({ 
  isOpen, 
  selectedAvatar, 
  onSelect, 
  onClose 
}) {
  const avatars = [
    'avt1.jpg', 'avt2.jpg', 'avt3.jpg', 'avt4.jpg',
    'avt5.jpg', 'avt6.jpg', 'avt7.jpg', 'avt8.jpg'
  ];

  const handleAvatarSelect = (avatar) => {
    onSelect(avatar);
  };

  if (!isOpen) return null;

  return (
    <div className="avatar-modal">
      <div className="modal-content">
        <div className="modal-header">
          <h2>Chọn Avatar</h2>
          <button 
            className="modal-close"
            onClick={onClose}
          >
            &times;
          </button>
        </div>
        <div className="modal-body">
          <div className="avatar-grid">
            {avatars.map((avatar) => (
              <div
                key={avatar}
                className={`avatar-item ${selectedAvatar === avatar ? 'selected' : ''}`}
                onClick={() => handleAvatarSelect(avatar)}
              >
                <div 
                  className="avatar-item-img"
                  style={{
                    backgroundImage: `url(/src/assets/avt/${avatar})`
                  }}
                ></div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}