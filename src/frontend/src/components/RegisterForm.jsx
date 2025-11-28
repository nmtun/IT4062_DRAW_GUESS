import React from 'react';
import { validateField } from '../utils/validation';
import './RegisterForm.css';

export default function RegisterForm({ 
  form, 
  errors, 
  isLoading, 
  isConnected, 
  onFormChange,
  onSubmit
}) {
  const handleChange = (e) => {
    const { name, value } = e.target;
    const updatedForm = { ...form, [name]: value };
    
    // Real-time validation with current form data
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
      <div className={getFormGroupClass('username')}>
        <label htmlFor="registerUsername">Tài khoản</label>
        <input 
          type="text" 
          id="registerUsername" 
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
        <small className="field-hint">
          Tài khoản phải có 3-32 ký tự, bắt đầu bằng chữ cái hoặc _, chỉ chứa chữ cái, số và _
        </small>
      </div>
      
      <div className={getFormGroupClass('password')}>
        <label htmlFor="registerPassword">Mật khẩu</label>
        <input 
          type="password" 
          id="registerPassword" 
          name="password" 
          value={form.password}
          onChange={handleChange}
          placeholder="Nhập mật khẩu"
          className={getFieldClass('password')}
          disabled={isLoading}
          autoComplete="new-password"
          maxLength="64"
          required
        />
        {errors.password && (
          <span className="error-text">{errors.password}</span>
        )}
        <small className="field-hint">
          Mật khẩu phải có 6-64 ký tự, có ít nhất 1 chữ cái và 1 số
        </small>
      </div>
      
      <div className={getFormGroupClass('confirmPassword')}>
        <label htmlFor="confirmPassword">Xác nhận mật khẩu</label>
        <input 
          type="password" 
          id="confirmPassword" 
          name="confirmPassword" 
          value={form.confirmPassword}
          onChange={handleChange}
          placeholder="Nhập lại mật khẩu"
          className={getFieldClass('confirmPassword')}
          disabled={isLoading}
          autoComplete="new-password"
          maxLength="64"
          required  
        />
        {errors.confirmPassword && (
          <span className="error-text">{errors.confirmPassword}</span>
        )}
        <small className="field-hint">
          Nhập lại mật khẩu để xác nhận
        </small>
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
              ĐANG ĐĂNG KÝ...
            </>
          ) : (
            'ĐĂNG KÝ'
          )}
        </button>
      </div>
    </form>
  );
}