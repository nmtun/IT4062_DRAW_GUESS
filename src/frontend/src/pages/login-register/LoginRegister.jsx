import React, { useState, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '../../hooks/useAuth';
import { validateLoginForm, validateRegisterForm } from '../../utils/validation';
import { getAvatar } from '../../utils/userStorage';
import LoginForm from '../../components/LoginForm';
import RegisterForm from '../../components/RegisterForm';
import AvatarSelector from '../../components/AvatarSelector';
import './LoginRegister.css';

export default function LoginRegister({ onLoginSuccess }) {
  const navigate = useNavigate();
  const [activeTab, setActiveTab] = useState('login');
  const [selectedAvatar, setSelectedAvatar] = useState(getAvatar()); // Load avatar từ localStorage
  const [showAvatarModal, setShowAvatarModal] = useState(false);
  
  // Form states
  const [loginForm, setLoginForm] = useState({ username: '', password: '' });
  const [registerForm, setRegisterForm] = useState({ 
    username: '', 
    password: '', 
    confirmPassword: '',
  });
  
  // Form validation states
  const [loginErrors, setLoginErrors] = useState({});
  const [registerErrors, setRegisterErrors] = useState({});
  const [message, setMessage] = useState({ text: '', type: '' });
  
  // Auth hook
  const { 
    isConnected, 
    isLoading, 
    user, 
    error: authError, 
    login, 
    register, 
    clearError,
    updateAvatar 
  } = useAuth();

  // Handle auth error
  useEffect(() => {
    if (authError) {
      setMessage({ text: authError, type: 'error' });
    }
  }, [authError]);

  // Handle successful login - navigate to lobby
  useEffect(() => {
    if (user && !isLoading) {
      setMessage({ text: 'Đăng nhập thành công!', type: 'success' });
      setTimeout(() => {
        navigate('/lobby');
        if (onLoginSuccess) {
          onLoginSuccess();
        }
      }, 500);
    }
  }, [user, isLoading, navigate, onLoginSuccess]);

  // Clear message when switching tabs
  useEffect(() => {
    setMessage({ text: '', type: '' });
    setLoginErrors({});
    setRegisterErrors({});
    if (authError) {
      clearError();
    }
  }, [activeTab]);

  // Event handlers
  const handleTabSwitch = (tab) => {
    setActiveTab(tab);
  };

  const handleAvatarSelect = (avatar) => {
    setSelectedAvatar(avatar);
    setShowAvatarModal(false);
    // Cập nhật avatar trong localStorage ngay lập tức
    updateAvatar(avatar);
  };

  const handleLoginFormChange = ({ form, errors }) => {
    setLoginForm(form);
    setLoginErrors(errors);
  };

  const handleRegisterFormChange = ({ form, errors }) => {
    setRegisterForm(form);
    setRegisterErrors(errors);
  };

  const handleLogin = async (e) => {
    e.preventDefault();
    
    // Validate form
    const validation = validateLoginForm(loginForm);
    if (!validation.isValid) {
      setLoginErrors(validation.errors);
      return;
    }

    setMessage({ text: '', type: '' });
    clearError();
    
    const success = await login(
      loginForm.username,
      loginForm.password,
      selectedAvatar
    );

    if (success) {
      // Reset form khi gửi yêu cầu thành công
      setLoginForm({ username: '', password: '' });
      setLoginErrors({});
    }
  };

  const handleRegister = async (e) => {
    e.preventDefault();
    
    // Validate form
    const validation = validateRegisterForm(registerForm);
    if (!validation.isValid) {
      setRegisterErrors(validation.errors);
      return;
    }

    setMessage({ text: '', type: '' });
    clearError();
    
    const success = await register(
      registerForm.username,
      registerForm.password
    );

    if (success) {
      // Đăng ký thành công, chuyển về tab đăng nhập
      setMessage({ text: 'Đăng ký thành công! Vui lòng đăng nhập để tiếp tục.', type: 'success' });
      setActiveTab('login');
      setRegisterForm({ username: '', password: '', confirmPassword: '' });
      setRegisterErrors({});
      
      // Pre-fill username trong form đăng nhập
      setLoginForm({ username: registerForm.username, password: '' });
    }
  };

  return (
    <div className="login-register-page">
      {/* Header */}
      <header className="header">
        <div className="header-center">
          <h1 className="logo">Draw & Guess</h1>
          <p className="tagline">DRAW, GUESS and WIN</p>
        </div>
      </header>

      {/* Main Content */}
      <main className="main-content">
        <div className="container">
          {/* Play Now Section */}
          <div className="play-section">
            <h2>CHƠI NGAY</h2>
            
            {/* Message Display */}
            {message.text && (
              <div className={`message ${message.type}`}>
                <span className="message-icon">
                  {message.type === 'success' ? '✓' : 
                   message.type === 'error' ? '⚠' : 'ℹ'}
                </span>
                <span className="message-text">{message.text}</span>
              </div>
            )}
            
            {/* Tab Navigation */}
            <div className="auth-tabs">
              <button 
                className={`tab-btn ${activeTab === 'login' ? 'active' : ''}`}
                onClick={() => handleTabSwitch('login')}
                disabled={isLoading}
              >
                Đăng Nhập
              </button>
              <button 
                className={`tab-btn ${activeTab === 'register' ? 'active' : ''}`}
                onClick={() => handleTabSwitch('register')}
                disabled={isLoading}
              >
                Đăng Ký
              </button>
            </div>

            {/* Login Form */}
            {activeTab === 'login' && (
              <LoginForm
                form={loginForm}
                errors={loginErrors}
                isLoading={isLoading}
                selectedAvatar={selectedAvatar}
                onFormChange={handleLoginFormChange}
                onSubmit={handleLogin}
                onAvatarEdit={() => setShowAvatarModal(true)}
              />
            )}

            {/* Register Form */}
            {activeTab === 'register' && (
              <RegisterForm
                form={registerForm}
                errors={registerErrors}
                isLoading={isLoading}
                onFormChange={handleRegisterFormChange}
                onSubmit={handleRegister}
              />
            )}
          </div>
        </div>
      </main>

      {/* Avatar Selection Modal */}
      <AvatarSelector
        isOpen={showAvatarModal}
        selectedAvatar={selectedAvatar}
        onSelect={handleAvatarSelect}
        onClose={() => setShowAvatarModal(false)}
      />
    </div>
  );
}