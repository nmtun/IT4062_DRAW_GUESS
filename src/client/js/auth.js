// Xử lý chuyển đổi giữa form đăng nhập và đăng ký

// ============================================
// CONSTANTS
// ============================================

const STATUS = {
    SUCCESS: 0x00,
    ERROR: 0x01,
    INVALID_USERNAME: 0x02,
    INVALID_PASSWORD: 0x03,
    USER_EXISTS: 0x04,
    AUTH_FAILED: 0x05
};

const VALIDATION = {
    USERNAME_MIN_LENGTH: 3,
    USERNAME_MAX_LENGTH: 32,
    PASSWORD_MIN_LENGTH: 6,
    PASSWORD_MAX_LENGTH: 64,
    USERNAME_PATTERN: /^[a-zA-Z_][a-zA-Z0-9_]*$/, // Bắt đầu bằng chữ cái hoặc _, chỉ chứa chữ cái, số và _
    PASSWORD_REQUIRE_LETTER: true,
    PASSWORD_REQUIRE_NUMBER: true
};

const MESSAGES = {
    PROCESSING: 'ĐANG XỬ LÝ...',
    LOGIN: 'ĐĂNG NHẬP',
    REGISTER: 'ĐĂNG KÝ',
    CONNECTING: 'Đang kết nối đến server...',
    LOGIN_SUCCESS: 'Đăng nhập thành công!',
    REGISTER_SUCCESS: 'Đăng ký thành công!',
    REGISTER_SUCCESS_PROMPT: 'Đăng ký thành công! Vui lòng đăng nhập.',
    ERRORS: {
        USERNAME_REQUIRED: 'Vui lòng nhập tài khoản!',
        PASSWORD_REQUIRED: 'Vui lòng nhập mật khẩu!',
        USERNAME_MIN_LENGTH: `Tài khoản phải có từ ${VALIDATION.USERNAME_MIN_LENGTH}-${VALIDATION.USERNAME_MAX_LENGTH} ký tự!`,
        USERNAME_INVALID_FORMAT: 'Tài khoản chỉ được chứa chữ cái, số và dấu gạch dưới (_), và phải bắt đầu bằng chữ cái hoặc dấu gạch dưới!',
        PASSWORD_MIN_LENGTH: `Mật khẩu phải có từ ${VALIDATION.PASSWORD_MIN_LENGTH}-${VALIDATION.PASSWORD_MAX_LENGTH} ký tự!`,
        PASSWORD_REQUIRE_LETTER: 'Mật khẩu phải chứa ít nhất một chữ cái!',
        PASSWORD_REQUIRE_NUMBER: 'Mật khẩu phải chứa ít nhất một số!',
        PASSWORD_REQUIRE_BOTH: 'Mật khẩu phải chứa ít nhất một chữ cái và một số!',
        PASSWORD_MISMATCH: 'Mật khẩu xác nhận không khớp!',
        AUTH_FAILED: 'Tài khoản hoặc mật khẩu không đúng!',
        USER_EXISTS: 'Tài khoản đã tồn tại!',
        ALREADY_LOGGED_IN: 'Bạn đã đăng nhập rồi! Vui lòng đăng xuất trước khi đăng nhập lại.',
        INVALID_USERNAME: 'Tên đăng nhập không hợp lệ! Tài khoản phải có từ 3-32 ký tự, chỉ chứa chữ cái, số và dấu gạch dưới, và phải bắt đầu bằng chữ cái hoặc dấu gạch dưới.',
        INVALID_PASSWORD: 'Mật khẩu không hợp lệ! Mật khẩu phải có từ 6-64 ký tự và chứa ít nhất một chữ cái và một số.',
        LOGIN_FAILED: 'Đăng nhập thất bại. Vui lòng thử lại!',
        REGISTER_FAILED: 'Đăng ký thất bại. Vui lòng thử lại!',
        CONNECTION_ERROR: 'Không thể kết nối đến server. Vui lòng thử lại!'
    }
};

const NAV = {
    LOBBY: 'lobby.html'
};

const TIMEOUTS = {
    REDIRECT: 1000,
    MESSAGE_AUTO_HIDE: 3000
};

const STORAGE_KEYS = {
    USER_ID: 'userId',
    USERNAME: 'username',
    IS_LOGGED_IN: 'isLoggedIn'
};

const DEBUG = true; // Set to false để tắt debug logs

// ============================================
// DOM CACHE
// ============================================

let domCache = {
    loginForm: null,
    registerForm: null,
    loginBtn: null,
    registerBtn: null,
    loginUsername: null,
    loginPassword: null,
    registerUsername: null,
    registerPassword: null,
    confirmPassword: null
};

// ============================================
// UTILITY FUNCTIONS
// ============================================

function log(...args) {
    if (DEBUG) {
        console.log(...args);
    }
}

function logError(...args) {
    console.error(...args);
}

function cacheDOM() {
    domCache.loginForm = document.getElementById('loginForm');
    domCache.registerForm = document.getElementById('registerForm');
    domCache.loginBtn = document.getElementById('loginBtn');
    domCache.registerBtn = document.getElementById('registerBtn');
    domCache.loginUsername = document.getElementById('loginUsername');
    domCache.loginPassword = document.getElementById('loginPassword');
    domCache.registerUsername = document.getElementById('registerUsername');
    domCache.registerPassword = document.getElementById('registerPassword');
    domCache.confirmPassword = document.getElementById('confirmPassword');
}

function setButtonState(button, { disabled = false, text = '' } = {}) {
    if (!button) return;
    button.disabled = disabled;
    if (text) {
        button.textContent = text;
    }
}

function resetButton(button, role) {
    if (!button) return;
    button.disabled = false;
    button.textContent = role === 'login' ? MESSAGES.LOGIN : MESSAGES.REGISTER;
}

function showMessage(type, message, autoHide = true) {
    const activeForm = document.querySelector('.auth-form.active');
    if (!activeForm) return;

    // Xóa thông báo cũ
    const existingMessages = activeForm.querySelectorAll('.error-message, .success-message');
    existingMessages.forEach(msg => msg.remove());

    // Tạo thông báo mới
    const messageDiv = document.createElement('div');
    messageDiv.className = type === 'error' ? 'error-message' : 'success-message';
    messageDiv.setAttribute('role', 'alert');
    messageDiv.setAttribute('aria-live', 'polite');
    messageDiv.textContent = message;

    activeForm.insertBefore(messageDiv, activeForm.firstChild);

    // Tự động xóa sau timeout
    if (autoHide) {
        setTimeout(() => {
            messageDiv.remove();
        }, TIMEOUTS.MESSAGE_AUTO_HIDE);
    }
}

function showError(message) {
    showMessage('error', message);
}

function showSuccess(message) {
    showMessage('success', message);
}

function saveUserSession(userId, username) {
    sessionStorage.setItem(STORAGE_KEYS.USER_ID, userId.toString());
    sessionStorage.setItem(STORAGE_KEYS.USERNAME, username);
    sessionStorage.setItem(STORAGE_KEYS.IS_LOGGED_IN, 'true');
}

function clearRegisterForm() {
    if (domCache.registerUsername) domCache.registerUsername.value = '';
    if (domCache.registerPassword) domCache.registerPassword.value = '';
    if (domCache.confirmPassword) domCache.confirmPassword.value = '';
}

function switchToLoginTab() {
    const loginTab = document.querySelector('[data-tab="login"]');
    if (loginTab) {
        loginTab.click();
    }
}

function redirectToLobby() {
    setTimeout(() => {
        window.location.href = NAV.LOBBY;
    }, TIMEOUTS.REDIRECT);
}

function checkNetworkDependency() {
    if (!window.Network) {
        logError('Network module chưa được load!');
        showError(MESSAGES.ERRORS.CONNECTION_ERROR);
        return false;
    }
    return true;
}

// ============================================
// VALIDATION FUNCTIONS
// ============================================

function validateLoginForm() {
    const username = domCache.loginUsername?.value.trim() || '';
    const password = domCache.loginPassword?.value || '';

    if (!username) {
        showError(MESSAGES.ERRORS.USERNAME_REQUIRED);
        return null;
    }

    if (!password) {
        showError(MESSAGES.ERRORS.PASSWORD_REQUIRED);
        return null;
    }

    return { username, password };
}

function validateRegisterForm() {
    const username = domCache.registerUsername?.value.trim() || '';
    const password = domCache.registerPassword?.value || '';
    const confirmPassword = domCache.confirmPassword?.value || '';

    // Validate username
    if (!username) {
        showError(MESSAGES.ERRORS.USERNAME_REQUIRED);
        return null;
    }

    if (username.length < VALIDATION.USERNAME_MIN_LENGTH || username.length > VALIDATION.USERNAME_MAX_LENGTH) {
        showError(MESSAGES.ERRORS.USERNAME_MIN_LENGTH);
        return null;
    }

    if (!VALIDATION.USERNAME_PATTERN.test(username)) {
        showError(MESSAGES.ERRORS.USERNAME_INVALID_FORMAT);
        return null;
    }

    // Validate password
    if (!password) {
        showError(MESSAGES.ERRORS.PASSWORD_REQUIRED);
        return null;
    }

    if (password.length < VALIDATION.PASSWORD_MIN_LENGTH || password.length > VALIDATION.PASSWORD_MAX_LENGTH) {
        showError(MESSAGES.ERRORS.PASSWORD_MIN_LENGTH);
        return null;
    }

    // Check password contains letter and number
    const hasLetter = /[a-zA-Z]/.test(password);
    const hasNumber = /[0-9]/.test(password);

    if (!hasLetter && !hasNumber) {
        showError(MESSAGES.ERRORS.PASSWORD_REQUIRE_BOTH);
        return null;
    }

    if (!hasLetter) {
        showError(MESSAGES.ERRORS.PASSWORD_REQUIRE_LETTER);
        return null;
    }

    if (!hasNumber) {
        showError(MESSAGES.ERRORS.PASSWORD_REQUIRE_NUMBER);
        return null;
    }

    // Validate confirm password
    if (password !== confirmPassword) {
        showError(MESSAGES.ERRORS.PASSWORD_MISMATCH);
        return null;
    }

    return { username, password };
}

// ============================================
// AUTH HANDLERS
// ============================================

/**
 * Kiểm tra xem người dùng đã đăng nhập chưa
 * @returns {boolean}
 */
function isUserLoggedIn() {
    const isLoggedIn = sessionStorage.getItem(STORAGE_KEYS.IS_LOGGED_IN);
    const userId = sessionStorage.getItem(STORAGE_KEYS.USER_ID);
    const username = sessionStorage.getItem(STORAGE_KEYS.USERNAME);
    return isLoggedIn === 'true' && userId && username;
}

async function handleLogin(e) {
    if (e) {
        e.preventDefault();
    }

    // Kiểm tra nếu đã đăng nhập rồi
    if (isUserLoggedIn()) {
        showError(MESSAGES.ERRORS.ALREADY_LOGGED_IN);
        // Tự động redirect sau 2 giây
        setTimeout(() => {
            window.location.href = NAV.LOBBY;
        }, 2000);
        return;
    }

    if (!checkNetworkDependency()) return;

    const formData = validateLoginForm();
    if (!formData) return;

    const { username, password } = formData;

    // Set button to processing state
    setButtonState(domCache.loginBtn, {
        disabled: true,
        text: MESSAGES.PROCESSING
    });

    showSuccess(MESSAGES.CONNECTING);

    try {
        log('Đang gửi login request...', { username });
        const response = await window.Network.sendLoginRequest(username, password);

        log('Nhận được login response:', response);
        const statusName = response.status === STATUS.SUCCESS ? 'SUCCESS' :
                          response.status === STATUS.AUTH_FAILED ? 'AUTH_FAILED' :
                          response.status === STATUS.ERROR ? 'ERROR' : 'OTHER';
        log('Response status:', response.status, `(0x${response.status.toString(16).padStart(2, '0')} = ${statusName})`);

        if (response.status === STATUS.SUCCESS) {
            saveUserSession(response.userId, response.username);

            log('✅ Đăng nhập thành công!');
            log('User ID:', response.userId);
            log('Username:', response.username);
            log('SessionStorage:', {
                userId: sessionStorage.getItem(STORAGE_KEYS.USER_ID),
                username: sessionStorage.getItem(STORAGE_KEYS.USERNAME),
                isLoggedIn: sessionStorage.getItem(STORAGE_KEYS.IS_LOGGED_IN)
            });

            // Socket sẽ được giữ lại, không đóng kết nối
            // Để duy trì kết nối cho các tính năng sau (chat, game, etc.)
            log('Socket connection maintained for future use');

            showSuccess(MESSAGES.LOGIN_SUCCESS);
            redirectToLobby();
        } else if (response.status === STATUS.AUTH_FAILED) {
            showError(MESSAGES.ERRORS.AUTH_FAILED);
            resetButton(domCache.loginBtn, 'login');
        } else {
            showError(MESSAGES.ERRORS.LOGIN_FAILED);
            resetButton(domCache.loginBtn, 'login');
        }
    } catch (error) {
        logError('❌ Lỗi đăng nhập:', error);
        const errorMessage = error.message || MESSAGES.ERRORS.CONNECTION_ERROR;
        showError(errorMessage);
        resetButton(domCache.loginBtn, 'login');
    }
}

async function handleRegister(e) {
    if (e) {
        e.preventDefault();
    }

    if (!checkNetworkDependency()) return;

    const formData = validateRegisterForm();
    if (!formData) return;

    const { username, password } = formData;

    // Set button to processing state
    setButtonState(domCache.registerBtn, {
        disabled: true,
        text: MESSAGES.PROCESSING
    });

    showSuccess(MESSAGES.CONNECTING);

    try {
        const response = await window.Network.sendRegisterRequest(username, password, '');

        log('Nhận được register response:', response);
        const statusName = response.status === STATUS.SUCCESS ? 'SUCCESS' :
                          response.status === STATUS.USER_EXISTS ? 'USER_EXISTS' :
                          response.status === STATUS.INVALID_USERNAME ? 'INVALID_USERNAME' :
                          response.status === STATUS.INVALID_PASSWORD ? 'INVALID_PASSWORD' :
                          response.status === STATUS.ERROR ? 'ERROR' : 'OTHER';
        log('Response status:', response.status, `(0x${response.status.toString(16).padStart(2, '0')} = ${statusName})`);

        if (response.status === STATUS.SUCCESS) {
            log('✅ Đăng ký thành công!');
            showSuccess(MESSAGES.REGISTER_SUCCESS);

            // Reset button ngay lập tức
            resetButton(domCache.registerBtn, 'register');

            // Chuyển sang tab đăng nhập sau timeout
            setTimeout(() => {
                switchToLoginTab();
                clearRegisterForm();
                showSuccess(MESSAGES.REGISTER_SUCCESS_PROMPT);
            }, TIMEOUTS.REDIRECT);
        } else {
            // Map status code to detailed error message
            let errorMessage = MESSAGES.ERRORS.REGISTER_FAILED;
            
            if (response.status === STATUS.USER_EXISTS) {
                errorMessage = MESSAGES.ERRORS.USER_EXISTS;
            } else if (response.status === STATUS.INVALID_USERNAME) {
                // Ưu tiên message từ server nếu có, nếu không dùng message mặc định chi tiết
                errorMessage = (response.message && response.message.trim()) 
                    ? response.message 
                    : MESSAGES.ERRORS.INVALID_USERNAME;
            } else if (response.status === STATUS.INVALID_PASSWORD) {
                // Ưu tiên message từ server nếu có, nếu không dùng message mặc định chi tiết
                errorMessage = (response.message && response.message.trim()) 
                    ? response.message 
                    : MESSAGES.ERRORS.INVALID_PASSWORD;
            } else if (response.status === STATUS.ERROR) {
                errorMessage = (response.message && response.message.trim()) 
                    ? response.message 
                    : MESSAGES.ERRORS.REGISTER_FAILED;
            }
            
            showError(errorMessage);
            resetButton(domCache.registerBtn, 'register');
        }
    } catch (error) {
        logError('❌ Lỗi đăng ký:', error);
        const errorMessage = error.message || MESSAGES.ERRORS.CONNECTION_ERROR;
        showError(errorMessage);
        resetButton(domCache.registerBtn, 'register');
    }
}

// ============================================
// INITIALIZATION
// ============================================

function initAuthTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    const loginForm = domCache.loginForm;
    const registerForm = domCache.registerForm;

    if (!loginForm || !registerForm) return;

    tabButtons.forEach(button => {
        button.addEventListener('click', function() {
            const targetTab = this.getAttribute('data-tab');

            // Cập nhật active state cho tabs
            tabButtons.forEach(btn => btn.classList.remove('active'));
            this.classList.add('active');

            // Hiển thị form tương ứng
            if (targetTab === 'login') {
                loginForm.classList.add('active');
                registerForm.classList.remove('active');
            } else if (targetTab === 'register') {
                registerForm.classList.add('active');
                loginForm.classList.remove('active');
            }
        });
    });
}

function initAuthForms() {
    // Setup form submit handlers (xử lý Enter key tự động)
    if (domCache.loginForm) {
        domCache.loginForm.addEventListener('submit', handleLogin);
    }

    if (domCache.registerForm) {
        domCache.registerForm.addEventListener('submit', handleRegister);
    }

    // Setup button click handlers (backup nếu form submit không hoạt động)
    if (domCache.loginBtn) {
        domCache.loginBtn.addEventListener('click', handleLogin);
    }

    if (domCache.registerBtn) {
        domCache.registerBtn.addEventListener('click', handleRegister);
    }
}

// ============================================
// MAIN INITIALIZATION
// ============================================

document.addEventListener('DOMContentLoaded', function() {
    cacheDOM();
    
    // Kiểm tra nếu đã đăng nhập, tự động redirect đến lobby
    if (isUserLoggedIn()) {
        log('Đã đăng nhập, chuyển hướng đến lobby...');
        window.location.href = NAV.LOBBY;
        return;
    }
    
    initAuthTabs();
    initAuthForms();
});
