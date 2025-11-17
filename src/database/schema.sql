-- Bảng lưu thông tin người dùng
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Bảng lưu thông tin phòng chơi
CREATE TABLE rooms (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_code VARCHAR(10) NOT NULL UNIQUE,
    host_id INT NOT NULL,
    max_players INT NOT NULL,
    total_rounds INT NOT NULL,
    status ENUM('waiting', 'in_progress', 'finished') DEFAULT 'waiting',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (host_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Bảng lưu thông tin người chơi trong phòng
CREATE TABLE room_players (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id INT NOT NULL,
    user_id INT NOT NULL,
    join_order INT NOT NULL,
    score INT DEFAULT 0,
    is_ready BOOLEAN DEFAULT FALSE,
    connected BOOLEAN DEFAULT TRUE,
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Bảng lưu thông tin các vòng chơi trong phòng
CREATE TABLE game_rounds (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id INT NOT NULL,
    round_number INT NOT NULL,
    turn_index INT NOT NULL,
    draw_id INT NOT NULL, 
    word VARCHAR(100) NOT NULL,
    started_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ended_at TIMESTAMP NULL,
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (draw_id) REFERENCES room_players(id) ON DELETE CASCADE
);

-- Bảng lưu thông tin các lượt đoán trong mỗi vòng chơi
CREATE TABLE guesses (
    id INT AUTO_INCREMENT PRIMARY KEY,
    round_id INT NOT NULL,
    player_id INT NOT NULL,
    guess_text VARCHAR(100) NOT NULL,
    is_correct BOOLEAN NOT NULL,
    guessed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (round_id) REFERENCES game_rounds(id) ON DELETE CASCADE,
    FOREIGN KEY (player_id) REFERENCES room_players(id) ON DELETE CASCADE
);

-- Bảng lưu điểm số chi tiết cho mỗi lượt đoán đúng
CREATE TABLE score_details (
    id INT AUTO_INCREMENT PRIMARY KEY,
    round_id INT NOT NULL,
    player_id INT NOT NULL,
    score INT NOT NULL,
    awarded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (round_id) REFERENCES game_rounds(id) ON DELETE CASCADE,
    FOREIGN KEY (player_id) REFERENCES room_players(id) ON DELETE CASCADE
);

-- Bảng chat trong phòng chơi
CREATE TABLE chat_messages (
    id INT AUTO_INCREMENT PRIMARY KEY,
    room_id INT NOT NULL,
    player_id INT NOT NULL,
    message_text VARCHAR(500) NOT NULL,
    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (room_id) REFERENCES rooms(id) ON DELETE CASCADE,
    FOREIGN KEY (player_id) REFERENCES room_players(id) ON DELETE CASCADE
);

-- Bảng danh sách từ vựng
CREATE TABLE words (
    id INT AUTO_INCREMENT PRIMARY KEY,
    word VARCHAR(100) NOT NULL UNIQUE,
    category VARCHAR(50) NOT NULL
);
