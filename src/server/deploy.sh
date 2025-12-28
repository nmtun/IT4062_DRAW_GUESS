#!/bin/bash

# Script tự động deploy - chạy trực tiếp trên VPS
# Sử dụng: ./deploy.sh

set -e  # Dừng ngay khi có lỗi

# Màu sắc cho output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Đường dẫn
REMOTE_PATH="/opt/IT4062_DRAW_GUESS/src"

# Hàm in log với màu
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Hàm thực thi lệnh với error handling
execute_cmd() {
    local cmd="$1"
    local description="$2"
    
    log_info "$description"
    
    if eval "$cmd"; then
        log_success "$description - Hoàn thành"
    else
        log_error "$description - Thất bại"
        exit 1
    fi
}

log_info "Bắt đầu quá trình deploy..."
echo ""

# Bước 1: Dừng service
log_info "=== BƯỚC 1: Dừng các service ==="
execute_cmd "systemctl stop drawguess" "Dừng drawguess service"
sleep 2

execute_cmd "pm2 delete all || true" "Xóa tất cả PM2 processes"
sleep 2

log_success "Đã dừng tất cả services"
echo ""

# Bước 2: Dừng Docker containers
log_info "=== BƯỚC 2: Dừng Docker containers ==="
execute_cmd "cd $REMOTE_PATH && docker compose down" "Dừng Docker containers"
sleep 3  # Chờ Docker cleanup

log_success "Đã dừng Docker containers"
echo ""

# Bước 3: Reload Nginx
log_info "=== BƯỚC 3: Reload Nginx ==="
execute_cmd "systemctl reload nginx" "Reload Nginx"
sleep 2

log_success "Đã reload Nginx"
echo ""

# Bước 4: Khởi động Docker containers
log_info "=== BƯỚC 4: Khởi động Docker containers ==="
execute_cmd "cd $REMOTE_PATH && docker compose up -d" "Khởi động Docker containers"
sleep 5  # Chờ MySQL khởi động hoàn toàn

log_success "Đã khởi động Docker containers"
echo ""

# Bước 5: Build server
log_info "=== BƯỚC 5: Build server ==="
execute_cmd "cd $REMOTE_PATH && make clean || true" "Clean build files"
sleep 1

execute_cmd "cd $REMOTE_PATH && make" "Build server"
sleep 2

log_success "Đã build server thành công"
echo ""

# Bước 6: Khởi động server
log_info "=== BƯỚC 6: Khởi động server ==="
execute_cmd "systemctl restart drawguess" "Khởi động drawguess service"
sleep 3  # Chờ server khởi động

execute_cmd "systemctl status drawguess --no-pager" "Kiểm tra trạng thái drawguess"
sleep 1

log_success "Đã khởi động server"
echo ""

# Bước 7: Deploy Gateway
log_info "=== BƯỚC 7: Deploy Gateway ==="
execute_cmd "cd $REMOTE_PATH/gateway && npm ci || npm install" "Cài đặt Gateway dependencies"
sleep 2

execute_cmd "pm2 start $REMOTE_PATH/gateway/index.js --name drawguess-gateway || pm2 restart drawguess-gateway" "Khởi động Gateway"
sleep 2

execute_cmd "pm2 save" "Lưu PM2 configuration"
sleep 1

log_success "Đã deploy Gateway"
echo ""

# Bước 8: Build Frontend
log_info "=== BƯỚC 8: Build Frontend ==="
execute_cmd "cd $REMOTE_PATH/frontend && npm ci || npm install" "Cài đặt Frontend dependencies"
sleep 2

execute_cmd "cd $REMOTE_PATH/frontend && npm run build" "Build Frontend"
sleep 2

log_success "Đã build Frontend"
echo ""

# Bước 9: Reload Nginx lần cuối
log_info "=== BƯỚC 9: Reload Nginx lần cuối ==="
execute_cmd "systemctl reload nginx" "Reload Nginx"
sleep 1

log_success "Đã reload Nginx"
echo ""

# Kiểm tra trạng thái cuối cùng
log_info "=== Kiểm tra trạng thái cuối cùng ==="
execute_cmd "systemctl is-active drawguess" "Kiểm tra drawguess service"
execute_cmd "pm2 list" "Kiểm tra PM2 processes"
execute_cmd "cd $REMOTE_PATH && docker compose ps" "Kiểm tra Docker containers"

echo ""
log_success "=========================================="
log_success "Deploy hoàn tất thành công!"
log_success "=========================================="
