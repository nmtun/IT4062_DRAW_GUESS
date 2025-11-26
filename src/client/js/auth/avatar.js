// Danh sách các avatar có sẵn
const AVATAR_LIST = [
    './assets/avt/avt1.jpg',
    './assets/avt/avt2.jpg',
    './assets/avt/avt3.jpg',
    './assets/avt/avt4.jpg',
    './assets/avt/avt5.jpg',
    './assets/avt/avt6.jpg',
    './assets/avt/avt7.jpg',
    './assets/avt/avt8.jpg',
];

// Lưu avatar hiện tại vào localStorage
const AVATAR_STORAGE_KEY = 'selectedAvatar';

// Khởi tạo avatar khi trang load
document.addEventListener('DOMContentLoaded', function() {
    initAvatar();
    setupAvatarEvents();
});

// Khởi tạo avatar từ localStorage hoặc dùng mặc định
function initAvatar() {
    const savedAvatar = localStorage.getItem(AVATAR_STORAGE_KEY);
    const defaultAvatar = AVATAR_LIST[0];
    const currentAvatar = savedAvatar || defaultAvatar;
    
    setAvatar(currentAvatar);
}

// Thiết lập các event listeners
function setupAvatarEvents() {
    const editAvatarBtn = document.querySelector('.edit-avatar');
    const avatarFace = document.querySelector('.avatar-face');
    const avatar = document.querySelector('.avatar');
    
    // Click vào nút edit avatar
    if (editAvatarBtn) {
        editAvatarBtn.addEventListener('click', function(e) {
            e.stopPropagation();
            openAvatarModal();
        });
    }
    
    // Click vào avatar để mở modal
    if (avatar) {
        avatar.addEventListener('click', function() {
            openAvatarModal();
        });
    }
    
    // Đóng modal khi click vào overlay
    const modal = document.getElementById('avatarModal');
    if (modal) {
        modal.addEventListener('click', function(e) {
            if (e.target === modal) {
                closeAvatarModal();
            }
        });
    }
    
    // Đóng modal khi click vào nút đóng
    const closeBtn = document.querySelector('.modal-close');
    if (closeBtn) {
        closeBtn.addEventListener('click', function() {
            closeAvatarModal();
        });
    }
    
    // Xử lý phím ESC để đóng modal
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            closeAvatarModal();
        }
    });
}

// Mở modal chọn avatar
function openAvatarModal() {
    const modal = document.getElementById('avatarModal');
    if (modal) {
        modal.style.display = 'flex';
        loadAvatarGrid();
    }
}

// Đóng modal
function closeAvatarModal() {
    const modal = document.getElementById('avatarModal');
    if (modal) {
        modal.style.display = 'none';
    }
}

// Tải danh sách avatar vào grid
function loadAvatarGrid() {
    const avatarGrid = document.getElementById('avatarGrid');
    if (!avatarGrid) return;
    
    // Xóa nội dung cũ
    avatarGrid.innerHTML = '';
    
    // Lấy avatar hiện tại
    const currentAvatar = getCurrentAvatar();
    
    // Tạo các item avatar
    AVATAR_LIST.forEach((avatarPath, index) => {
        const avatarItem = document.createElement('div');
        avatarItem.className = 'avatar-item';
        
        // Kiểm tra nếu là avatar đang được chọn
        if (avatarPath === currentAvatar) {
            avatarItem.classList.add('selected');
        }
        
        // Tạo hình ảnh
        const img = document.createElement('div');
        img.className = 'avatar-item-img';
        img.style.backgroundImage = `url('${avatarPath}')`;
        
        // Thêm event listener để chọn avatar
        avatarItem.addEventListener('click', function() {
            selectAvatar(avatarPath);
        });
        
        avatarItem.appendChild(img);
        avatarGrid.appendChild(avatarItem);
    });
}

// Chọn avatar mới
function selectAvatar(avatarPath) {
    setAvatar(avatarPath);
    localStorage.setItem(AVATAR_STORAGE_KEY, avatarPath);
    closeAvatarModal();
    
    // Cập nhật selected state trong grid
    updateSelectedState(avatarPath);
}

// Đặt avatar mới
function setAvatar(avatarPath) {
    const avatarFace = document.querySelector('.avatar-face');
    if (avatarFace) {
        avatarFace.style.backgroundImage = `url('${avatarPath}')`;
    }
}

// Lấy avatar hiện tại
function getCurrentAvatar() {
    const avatarFace = document.querySelector('.avatar-face');
    if (avatarFace) {
        const bgImage = avatarFace.style.backgroundImage;
        if (bgImage && bgImage !== 'none') {
            // Extract path from url('css/assets/avatar/...')
            const match = bgImage.match(/url\(['"]?([^'"]+)['"]?\)/);
            if (match) {
                return match[1];
            }
        }
    }
    
    // Fallback to localStorage or default
    return localStorage.getItem(AVATAR_STORAGE_KEY) || AVATAR_LIST[0];
}

// Cập nhật trạng thái selected trong grid
function updateSelectedState(selectedPath) {
    const avatarItems = document.querySelectorAll('.avatar-item');
    avatarItems.forEach(item => {
        item.classList.remove('selected');
    });
    
    // Tìm và đánh dấu selected
    avatarItems.forEach(item => {
        const img = item.querySelector('.avatar-item-img');
        if (img) {
            const bgImage = img.style.backgroundImage;
            const match = bgImage.match(/url\(['"]?([^'"]+)['"]?\)/);
            if (match && match[1] === selectedPath) {
                item.classList.add('selected');
            }
        }
    });
}

// Export functions nếu cần
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        setAvatar,
        getCurrentAvatar,
        AVATAR_LIST
    };
}
