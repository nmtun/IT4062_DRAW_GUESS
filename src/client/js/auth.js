// Xử lý chuyển đổi giữa form đăng nhập và đăng ký
document.addEventListener('DOMContentLoaded', function() {
    initAuthTabs();
    initAuthForms();
});

// Khởi tạo tab navigation
function initAuthTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    const loginForm = document.getElementById('loginForm');
    const registerForm = document.getElementById('registerForm');
    
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

// Khởi tạo xử lý form
function initAuthForms() {
    // Xử lý đăng nhập
    const loginBtn = document.getElementById('loginBtn');
    if (loginBtn) {
        loginBtn.addEventListener('click', handleLogin);
    }
    
    // Xử lý đăng ký
    const registerBtn = document.getElementById('registerBtn');
    if (registerBtn) {
        registerBtn.addEventListener('click', handleRegister);
    }
    
    // Xử lý Enter key
    const loginForm = document.getElementById('loginForm');
    const registerForm = document.getElementById('registerForm');
    
    if (loginForm) {
        loginForm.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                handleLogin();
            }
        });
    }
    
    if (registerForm) {
        registerForm.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                handleRegister();
            }
        });
    }
}

// Xử lý đăng nhập
function handleLogin() {
    const username = document.getElementById('loginUsername').value.trim();
    const password = document.getElementById('loginPassword').value;
    
    // Validation
    if (!username) {
        showError('Vui lòng nhập tài khoản!');
        return;
    }
    
    if (!password) {
        showError('Vui lòng nhập mật khẩu!');
        return;
    }
    
    // TODO: Gửi request đăng nhập đến server
    console.log('Đăng nhập:', { username, password });
    
    // Tạm thời hiển thị thông báo
    showSuccess('Đang xử lý đăng nhập...');
    
    // Sau khi đăng nhập thành công, có thể redirect hoặc chuyển trang
    // window.location.href = 'lobby.html';
}

// Xử lý đăng ký
function handleRegister() {
    const username = document.getElementById('registerUsername').value.trim();
    const email = document.getElementById('registerEmail').value.trim();
    const password = document.getElementById('registerPassword').value;
    const confirmPassword = document.getElementById('confirmPassword').value;
    
    // Validation
    if (!username) {
        showError('Vui lòng nhập tài khoản!');
        return;
    }
    
    if (username.length < 3) {
        showError('Tài khoản phải có ít nhất 3 ký tự!');
        return;
    }
    
    if (!email) {
        showError('Vui lòng nhập email!');
        return;
    }
    
    if (!isValidEmail(email)) {
        showError('Email không hợp lệ!');
        return;
    }
    
    if (!password) {
        showError('Vui lòng nhập mật khẩu!');
        return;
    }
    
    if (password.length < 6) {
        showError('Mật khẩu phải có ít nhất 6 ký tự!');
        return;
    }
    
    if (password !== confirmPassword) {
        showError('Mật khẩu xác nhận không khớp!');
        return;
    }
    
    // TODO: Gửi request đăng ký đến server
    console.log('Đăng ký:', { username, email, password });
    
    // Tạm thời hiển thị thông báo
    showSuccess('Đang xử lý đăng ký...');
    
    // Sau khi đăng ký thành công, có thể chuyển sang tab đăng nhập
    // setTimeout(() => {
    //     document.querySelector('[data-tab="login"]').click();
    //     showSuccess('Đăng ký thành công! Vui lòng đăng nhập.');
    // }, 1000);
}

// Kiểm tra email hợp lệ
function isValidEmail(email) {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return emailRegex.test(email);
}

// Hiển thị thông báo lỗi
function showError(message) {
    // Xóa thông báo cũ nếu có
    const existingError = document.querySelector('.error-message');
    if (existingError) {
        existingError.remove();
    }
    
    const existingSuccess = document.querySelector('.success-message');
    if (existingSuccess) {
        existingSuccess.remove();
    }
    
    // Tạo thông báo mới
    const errorDiv = document.createElement('div');
    errorDiv.className = 'error-message';
    errorDiv.textContent = message;
    
    const activeForm = document.querySelector('.auth-form.active');
    if (activeForm) {
        activeForm.insertBefore(errorDiv, activeForm.firstChild);
        
        // Tự động xóa sau 3 giây
        setTimeout(() => {
            errorDiv.remove();
        }, 3000);
    }
}

// Hiển thị thông báo thành công
function showSuccess(message) {
    // Xóa thông báo cũ nếu có
    const existingError = document.querySelector('.error-message');
    if (existingError) {
        existingError.remove();
    }
    
    const existingSuccess = document.querySelector('.success-message');
    if (existingSuccess) {
        existingSuccess.remove();
    }
    
    // Tạo thông báo mới
    const successDiv = document.createElement('div');
    successDiv.className = 'success-message';
    successDiv.textContent = message;
    
    const activeForm = document.querySelector('.auth-form.active');
    if (activeForm) {
        activeForm.insertBefore(successDiv, activeForm.firstChild);
        
        // Tự động xóa sau 3 giây
        setTimeout(() => {
            successDiv.remove();
        }, 3000);
    }
}

