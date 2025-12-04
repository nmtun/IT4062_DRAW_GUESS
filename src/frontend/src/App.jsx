import { useState } from 'react'
import { Routes, Route } from 'react-router-dom'
import LoginRegister from './pages/login-register/LoginRegister'
import Lobby from './pages/lobby/Lobby'
import GameRoom from './pages/game-room/GameRoom'

export default function App() {
  return (
    <Routes>
      <Route path="/" element={<LoginRegister />} />
      <Route path="/lobby" element={<Lobby />} />
      <Route path="/game/:roomId" element={<GameRoom />} />
    </Routes>
  )
}

