import React from 'react';
import { validateField } from '../utils/validation';
import './LoginForm.css';

export default function LoginForm({ 
  form, 
  errors, 
  isLoading, 
  isConnected, 
  selectedAvatar,
  onFormChange,
  onSubmit,
  onAvatarEdit
}) {
  const handleChange = (e) => {
    const { name, value } = e.target;
    const updatedForm = { ...form, [name]: value };
    
    // Real-time validation
    const error = validateField(name, value, updatedForm);
    
    onFormChange({
      form: updatedForm,
      errors: { ...errors, [name]: error }
    });
  };

  const getFieldClass = (fieldName) => {
    const value = form[fieldName];
    const error = errors[fieldName];
    
    if (error) return 'error';
    if (value && !error) return 'valid';
    return '';
  };

  const getFormGroupClass = (fieldName) => {
    const value = form[fieldName];
    const error = errors[fieldName];
    
    let classes = 'form-group';
    if (error) classes += ' error';
    if (value && !error) classes += ' valid';
    
    return classes;
  };

  return (
    <form className="auth-form active" onSubmit={onSubmit}>
      <div className="avatar-container">
        <div className="avatar">
          <div 
            className="avatar-face"
            style={{
              backgroundImage: `url(/src/assets/avt/${selectedAvatar})`
            }}
          ></div>
          <button 
            type="button"
            className="edit-avatar"
            onClick={onAvatarEdit}
            disabled={isLoading}
          >
            ✏️
          </button>
        </div>
      </div>
      
      <div className={getFormGroupClass('username')}>
        <label htmlFor="loginUsername">Tài khoản</label>
        <input 
          type="text" 
          id="loginUsername" 
          name="username" 
          value={form.username}
          onChange={handleChange}
          placeholder="Nhập tài khoản"
          className={getFieldClass('username')}
          disabled={isLoading}
          autoComplete="username"
          maxLength="32"
          required
        />
        {errors.username && (
          <span className="error-text">{errors.username}</span>
        )}
      </div>
      
      <div className={getFormGroupClass('password')}>
        <label htmlFor="loginPassword">Mật khẩu</label>
        <input 
          type="password" 
          id="loginPassword" 
          name="password" 
          value={form.password}
          onChange={handleChange}
          placeholder="Nhập mật khẩu"
          className={getFieldClass('password')}
          disabled={isLoading}
          autoComplete="current-password"
          maxLength="64"
          required
        />
        {errors.password && (
          <span className="error-text">{errors.password}</span>
        )}
      </div>
      
      <div className="action-buttons">
        <button 
          type="submit" 
          className="btn btn-play"
          disabled={isLoading || !isConnected}
        >
          {isLoading ? (
            <>
              <span className="loading-spinner"></span>
              ĐANG ĐĂNG NHẬP...
            </>
          ) : (
            'ĐĂNG NHẬP'
          )}
        </button>
      </div>
    </form>
  );
}