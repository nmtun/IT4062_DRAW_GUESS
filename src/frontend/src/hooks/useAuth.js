import { useState, useEffect, useRef, useCallback } from 'react';
import { getAuthService } from '../services/AuthService';
import { saveUserData, getUserData, clearUserData, saveAvatar, getAvatar, getCurrentUser } from '../utils/userStorage';

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
        let isMounted = true; // Flag để tránh state update sau khi unmount
        
        // Khôi phục user data từ localStorage nếu có
        const savedUserData = getCurrentUser();
        if (savedUserData && isMounted) {
            setUser(savedUserData);
        }

        // Chỉ kiểm tra trạng thái kết nối hiện tại, không tự động kết nối
        const currentState = service.getConnectionState();
        if (currentState === 'connected') {
            // Đã kết nối rồi
            if (isMounted) {
                setIsConnected(true);
                setIsLoading(false);
                setError(null);
            }
        } else if (currentState === 'connecting') {
            // Đang kết nối
            if (isMounted) {
                setIsLoading(true);
            }
        } else {
            // Disconnected - sẵn sàng kết nối khi cần
            if (isMounted) {
                setIsConnected(false);
                setIsLoading(false);
                setError(null); // Không hiển thị error khi chưa thử kết nối
            }
        }

        // Subscribe to responses
        const handleLoginResponse = (data) => {
            if (!isMounted) return;
            setIsLoading(false);
            if (data.status === 'success') {
                // Lấy avatar từ localStorage hoặc dùng default
                const savedAvatar = getAvatar();
                const userData = {
                    id: data.userId,
                    username: data.username,
                    avatar: data.avatar || savedAvatar
                };
                
                // Lưu thông tin user vào localStorage để persist
                saveUserData(userData);
                
                setUser(userData);
                setError(null);
            } else {
                setError('Tài khoản hoặc mật khẩu không đúng');
                setUser(null);
            }
        };

        const handleRegisterResponse = (data) => {
            if (!isMounted) return;
            setIsLoading(false);
            if (data.status === 'success') {
                setError(null);
            } else {
                setError(data.message || 'Đăng ký thất bại');
            }
        };

        const handleError = (data) => {
            if (!isMounted) return;
            setIsLoading(false);
            setError(data.message || 'Có lỗi xảy ra');
        };

        const handleConnectionFailed = () => {
            if (!isMounted) return;
            setIsConnected(false);
            setError('Mất kết nối với server');
        };

        service.subscribe('login_response', handleLoginResponse);
        service.subscribe('register_response', handleRegisterResponse);
        service.subscribe('error', handleError);
        service.subscribe('connection_failed', handleConnectionFailed);

        // Cleanup
        return () => {
            isMounted = false; // Đánh dấu component đã unmount
            
            // Unsubscribe các callback cụ thể
            service.unsubscribe('login_response', handleLoginResponse);
            service.unsubscribe('register_response', handleRegisterResponse);
            service.unsubscribe('error', handleError);
            service.unsubscribe('connection_failed', handleConnectionFailed);
            
            // Chỉ disconnect nếu không còn subscribers nào khác
            // Điều này giúp tránh việc disconnect không cần thiết trong StrictMode
            if (service.callbacks.size === 0) {
                console.log('No more subscribers, keeping connection alive for potential reuse');
            }
        };
    }, []);

    const login = async (username, password, avatar) => {
        const service = authService.current;
        let connectionState = service.getConnectionState();
        
        // Kết nối nếu chưa kết nối
        if (connectionState !== 'connected') {
            setIsLoading(true);
            setError(null);
            
            try {
                await service.connect();
                setIsConnected(true);
            } catch (err) {
                console.error('Connection failed:', err);
                setIsLoading(false);
                setError('Không thể kết nối đến server');
                return false;
            }
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
        const service = authService.current;
        let connectionState = service.getConnectionState();
        
        // Kết nối nếu chưa kết nối
        if (connectionState !== 'connected') {
            setIsLoading(true);
            setError(null);
            
            try {
                await service.connect();
                setIsConnected(true);
            } catch (err) {
                console.error('Connection failed:', err);
                setIsLoading(false);
                setError('Không thể kết nối đến server');
                return false;
            }
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
        // Xóa user data khỏi localStorage nhưng giữ lại avatar
        clearUserData();
        setUser(null);
        setError(null);
    };

    const clearError = useCallback(() => {
        setError(null);
    }, []);

    const updateAvatar = useCallback((newAvatar) => {
        // Lưu avatar vào localStorage
        saveAvatar(newAvatar);
        
        // Cập nhật user state nếu đang đăng nhập
        if (user) {
            const updatedUser = { ...user, avatar: newAvatar };
            setUser(updatedUser);
            saveUserData(updatedUser);
        }
    }, [user]);

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
        updateAvatar,

        // Utils
        connectionInfo: authService.current.getConnectionInfo()
    };
};

export default useAuth;