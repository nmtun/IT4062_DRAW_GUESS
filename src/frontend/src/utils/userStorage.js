/**
 * Utility functions để quản lý user data và avatar trong localStorage
 */

const USER_DATA_KEY = 'user_data';
const AVATAR_KEY = 'user_avatar';

/**
 * Lưu user data vào localStorage
 */
export const saveUserData = (userData) => {
  try {
    localStorage.setItem(USER_DATA_KEY, JSON.stringify(userData));
    return true;
  } catch (error) {
    console.error('Error saving user data:', error);
    return false;
  }
};

/**
 * Lấy user data từ localStorage
 */
export const getUserData = () => {
  try {
    const data = localStorage.getItem(USER_DATA_KEY);
    return data ? JSON.parse(data) : null;
  } catch (error) {
    console.error('Error getting user data:', error);
    return null;
  }
};

/**
 * Xóa user data khỏi localStorage
 */
export const clearUserData = () => {
  try {
    localStorage.removeItem(USER_DATA_KEY);
    return true;
  } catch (error) {
    console.error('Error clearing user data:', error);
    return false;
  }
};

/**
 * Lưu avatar vào localStorage
 */
export const saveAvatar = (avatar) => {
  try {
    localStorage.setItem(AVATAR_KEY, avatar);
    return true;
  } catch (error) {
    console.error('Error saving avatar:', error);
    return false;
  }
};

/**
 * Lấy avatar từ localStorage
 */
export const getAvatar = () => {
  try {
    return localStorage.getItem(AVATAR_KEY) || 'avt1.jpg';
  } catch (error) {
    console.error('Error getting avatar:', error);
    return 'avt1.jpg';
  }
};

/**
 * Kiểm tra xem user đã đăng nhập chưa
 */
export const isUserLoggedIn = () => {
  const userData = getUserData();
  return userData && userData.username && userData.id;
};

/**
 * Lấy thông tin user hiện tại
 */
export const getCurrentUser = () => {
  const userData = getUserData();
  if (!userData) return null;
  
  return {
    ...userData,
    avatar: userData.avatar || getAvatar()
  };
};