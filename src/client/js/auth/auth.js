document.addEventListener('DOMContentLoaded', function() {
    initAuth();
});

let currentTab = 'login'; // Theo dõi tab hiện tại

function initAuth() {
    setupTabSwitching();
    setupFormSubmission();
    setupFormValidation();
    
    // Khôi phục trạng thái tab từ localStorage (nếu có)
    const savedTab = localStorage.getItem('currentAuthTab');
    if (savedTab && ['login', 'register'].includes(savedTab)) {
        switchTab(savedTab);
    }
}

// Thiết lập chuyển đổi tab
function setupTabSwitching() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    
    tabButtons.forEach(button => {
        button.addEventListener('click', function() {
            const targetTab = this.getAttribute('data-tab');
            switchTab(targetTab);
        });
    });
}

// Chuyển đổi giữa các tab
function switchTab(tabName) {
    // Cập nhật trạng thái button
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
    
    // Cập nhật form hiển thị
    document.querySelectorAll('.auth-form').forEach(form => {
        form.classList.remove('active');
    });
    
    if (tabName === 'login') {
        document.getElementById('loginForm').classList.add('active');
    } else if (tabName === 'register') {
        document.getElementById('registerForm').classList.add('active');
    }
    
    currentTab = tabName;
    localStorage.setItem('currentAuthTab', tabName);
    
    // Xóa các thông báo lỗi khi chuyển tab
    clearMessages();
}