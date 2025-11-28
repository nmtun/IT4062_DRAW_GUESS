import { useState, useEffect, useRef } from 'react';
import { getAuthService } from '../services/AuthService';

/**
 * Custom hook để quản lý authentication state
 */
export const useAuth = () => {
    const [isConnected, setIsConnected] = useState(false);
    const [isLoading, setIsLoading] = useState(false);
    const [user, setUser] = useState(null);
    const [error, setError] = useState(null);
    const authService = useRef(getAuthService());

    useEffect(() => {
        const service = authService.current;

        // Kiểm tra user đã login từ localStorage
        const savedUser = localStorage.getItem('user');
        if (savedUser) {
            try {
                setUser(JSON.parse(savedUser));
            } catch (err) {
                console.error('Error parsing saved user:', err);
                localStorage.removeItem('user');
            }
        }

        // Kết nối đến gateway
        setIsLoading(true);
        service.connect()
            .then(() => {
                setIsConnected(true);
                setError(null);
            })
            .catch((err) => {
                console.error('Connection failed:', err);
                setIsConnected(false);
                setError('Không thể kết nối đến server');
            })
            .finally(() => {
                setIsLoading(false);
            });

        // Subscribe to responses
        service.subscribe('login_response', (data) => {
            setIsLoading(false);
            if (data.status === 'success') {
                const userData = {
                    id: data.userId,
                    username: data.username,
                    avatar: data.avatar || 'avt1.jpg'
                };
                setUser(userData);
                localStorage.setItem('user', JSON.stringify(userData));
                setError(null);
            } else {
                setError('Tài khoản hoặc mật khẩu không đúng');
                setUser(null);
                localStorage.removeItem('user');
            }
        });

        service.subscribe('register_response', (data) => {
            setIsLoading(false);
            if (data.status === 'success') {
                setError(null);
            } else {
                setError(data.message || 'Đăng ký thất bại');
            }
        });

        service.subscribe('error', (data) => {
            setIsLoading(false);
            setError(data.message || 'Có lỗi xảy ra');
        });

        service.subscribe('connection_failed', () => {
            setIsConnected(false);
            setError('Mất kết nối với server');
        });

        // Cleanup
        return () => {
            service.disconnect();
        };
    }, []);

    const login = async (username, password, avatar) => {
        if (!isConnected) {
            setError('Chưa kết nối đến server');
            return false;
        }

        setIsLoading(true);
        setError(null);

        const success = authService.current.login(username, password, avatar);
        if (!success) {
            setIsLoading(false);
            setError('Không thể gửi yêu cầu đăng nhập');
            return false;
        }

        return true;
    };

    const register = async (username, password) => {
        if (!isConnected) {
            setError('Chưa kết nối đến server');
            return false;
        }

        setIsLoading(true);
        setError(null);

        const success = authService.current.register(username, password);
        if (!success) {
            setIsLoading(false);
            setError('Không thể gửi yêu cầu đăng ký');
            return false;
        }

        return true;
    };

    const logout = () => {
        authService.current.logout();
        setUser(null);
        localStorage.removeItem('user');
        setError(null);
    };

    const clearError = () => {
        setError(null);
    };

    return {
        // State
        isConnected,
        isLoading,
        user,
        error,
        isAuthenticated: !!user,

        // Actions
        login,
        register,
        logout,
        clearError,

        // Utils
        connectionInfo: authService.current.getConnectionInfo()
    };
};

export default useAuth;