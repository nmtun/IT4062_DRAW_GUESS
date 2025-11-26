// Hiệu ứng vẽ nét khi di chuột trên background
document.addEventListener('DOMContentLoaded', function() {
    initBackgroundDraw();
});

function initBackgroundDraw() {
    // Tạo canvas overlay
    const canvas = document.createElement('canvas');
    canvas.id = 'backgroundDrawCanvas';
    canvas.style.position = 'fixed';
    canvas.style.top = '0';
    canvas.style.left = '0';
    canvas.style.width = '100%';
    canvas.style.height = '100%';
    canvas.style.pointerEvents = 'none';
    canvas.style.zIndex = '1';
    canvas.style.opacity = '0.8';
    
    document.body.appendChild(canvas);
    
    const ctx = canvas.getContext('2d');
    let isDrawing = false;
    let lastX = 0;
    let lastY = 0;
    let strokes = []; // Lưu các nét vẽ để có thể fade out
    
    // Thiết lập kích thước canvas
    function resizeCanvas() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }
    
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    
    // Cấu hình vẽ
    const drawConfig = {
        lineWidth: 8,
        color: 'rgba(255, 255, 255, 0.6)',
        fadeTime: 2000, // 2 giây
        maxStrokes: 50 // Giới hạn số nét vẽ để tránh lag
    };
    
    // Lớp Stroke để quản lý từng nét vẽ
    class Stroke {
        constructor(x, y, color, lineWidth) {
            this.points = [{x, y}];
            this.color = color;
            this.lineWidth = lineWidth;
            this.createdAt = Date.now();
            this.opacity = 0.6;
            this.fadeStartTime = null;
        }
        
        addPoint(x, y) {
            this.points.push({x, y});
        }
        
        draw(ctx) {
            if (this.points.length < 2) return;
            
            ctx.save();
            ctx.globalAlpha = this.opacity;
            ctx.strokeStyle = this.color;
            ctx.lineWidth = this.lineWidth;
            ctx.lineCap = 'round';
            ctx.lineJoin = 'round';
            
            ctx.beginPath();
            ctx.moveTo(this.points[0].x, this.points[0].y);
            
            for (let i = 1; i < this.points.length; i++) {
                ctx.lineTo(this.points[i].x, this.points[i].y);
            }
            
            ctx.stroke();
            ctx.restore();
        }
        
        update() {
            const now = Date.now();
            const age = now - this.createdAt;
            
            // Bắt đầu fade sau 2 giây
            if (age >= drawConfig.fadeTime && this.fadeStartTime === null) {
                this.fadeStartTime = now;
            }
            
            // Fade out trong 500ms
            if (this.fadeStartTime !== null) {
                const fadeAge = now - this.fadeStartTime;
                const fadeDuration = 500;
                this.opacity = Math.max(0, 0.6 * (1 - fadeAge / fadeDuration));
                
                if (this.opacity <= 0) {
                    return false; // Nét vẽ đã biến mất
                }
            }
            
            return true;
        }
    }
    
    // Xử lý mouse move
    function handleMouseMove(e) {
        const x = e.clientX;
        const y = e.clientY;
        
        // Kiểm tra xem có đang hover vào các phần tử tương tác không
        const target = e.target;
        if (target.tagName === 'BUTTON' || 
            target.tagName === 'INPUT' || 
            target.tagName === 'SELECT' ||
            target.closest('.container') ||
            target.closest('.avatar-modal') ||
            target.closest('.header') ||
            target.closest('.footer')) {
            isDrawing = false;
            return;
        }
        
        if (!isDrawing) {
            isDrawing = true;
            lastX = x;
            lastY = y;
            
            // Tạo nét vẽ mới
            if (strokes.length >= drawConfig.maxStrokes) {
                strokes.shift(); // Xóa nét cũ nhất
            }
            
            const stroke = new Stroke(x, y, drawConfig.color, drawConfig.lineWidth);
            strokes.push(stroke);
        } else {
            // Thêm điểm vào nét vẽ cuối cùng
            if (strokes.length > 0) {
                strokes[strokes.length - 1].addPoint(x, y);
            }
        }
        
        lastX = x;
        lastY = y;
    }
    
    // Xử lý mouse leave
    function handleMouseLeave() {
        isDrawing = false;
    }
    
    // Animation loop
    function animate() {
        // Xóa canvas
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        
        // Cập nhật và vẽ các nét vẽ
        strokes = strokes.filter(stroke => {
            const isAlive = stroke.update();
            if (isAlive) {
                stroke.draw(ctx);
            }
            return isAlive;
        });
        
        requestAnimationFrame(animate);
    }
    
    // Bắt đầu animation
    animate();
    
    // Event listeners
    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('mouseleave', handleMouseLeave);
    
    // Dừng vẽ khi click (để tránh vẽ khi click vào các nút)
    document.addEventListener('mousedown', function() {
        isDrawing = false;
    });
}
