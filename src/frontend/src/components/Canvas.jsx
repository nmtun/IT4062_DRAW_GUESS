import React, { useRef, useEffect, useState, forwardRef, useImperativeHandle } from 'react';
import './Canvas.css';

const Canvas = forwardRef(function Canvas({ isDrawing = false, onDraw, color = '#000000', brushSize = 5, isEraser = false, gameState = 'waiting' }, ref) {
  const canvasRef = useRef(null);
  const [isDrawingState, setIsDrawingState] = useState(false);
  const lastPointRef = useRef(null);

  // Chỉ resize canvas khi component mount hoặc khi kích thước container thay đổi
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const resizeCanvas = () => {
      const ctx = canvas.getContext('2d');
      const currentWidth = canvas.offsetWidth;
      const currentHeight = canvas.offsetHeight;
      
      // Chỉ resize nếu kích thước thực sự thay đổi
      if (canvas.width !== currentWidth || canvas.height !== currentHeight) {
        canvas.width = currentWidth;
        canvas.height = currentHeight;
      }
    };

    resizeCanvas();

    // Chỉ lắng nghe resize window, không resize khi brushSize thay đổi
    const handleResize = () => {
      resizeCanvas();
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []); // Chỉ chạy một lần khi mount

  useImperativeHandle(ref, () => ({
    clearCanvas: () => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext('2d');
      ctx.clearRect(0, 0, canvas.width, canvas.height);
    },
    drawFromServer: (data) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext('2d');

      if (!data || typeof data.action === 'undefined') {
        console.warn('[Canvas] drawFromServer: invalid data', data);
        return;
      }

      if (data.action === 2) {
        // CLEAR action
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        return;
      }

      // Validate coordinates
      if (typeof data.x1 === 'undefined' || typeof data.y1 === 'undefined' || 
          typeof data.x2 === 'undefined' || typeof data.y2 === 'undefined') {
        console.warn('[Canvas] drawFromServer: missing coordinates', data);
        return;
      }

      const isErase = data.action === 3;
      ctx.globalCompositeOperation = isErase ? 'destination-out' : 'source-over';
      
      if (!isErase && data.color) {
        // Convert color integer to RGB
        const r = (data.color >>> 24) & 0xFF;
        const g = (data.color >>> 16) & 0xFF;
        const b = (data.color >>> 8) & 0xFF;
        ctx.strokeStyle = `rgb(${r}, ${g}, ${b})`;
      } else {
        ctx.strokeStyle = '#FFFFFF';
      }

      ctx.lineWidth = data.width || 5;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';

      // Vẽ đường thẳng từ (x1, y1) đến (x2, y2)
      ctx.beginPath();
      ctx.moveTo(data.x1, data.y1);
      ctx.lineTo(data.x2, data.y2);
      ctx.stroke();
    }
  }));

  const getCoordinates = (e) => {
    const canvas = canvasRef.current;
    if (!canvas) return null;
    const rect = canvas.getBoundingClientRect();
    return {
      x: e.clientX - rect.left,
      y: e.clientY - rect.top
    };
  };

  const startDrawing = (e) => {
    if (!isDrawing) return;
    setIsDrawingState(true);
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const coords = getCoordinates(e);
    if (!coords) return;

    // Cập nhật style dựa trên props hiện tại
    ctx.strokeStyle = isEraser ? '#FFFFFF' : color;
    ctx.lineWidth = brushSize;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.globalCompositeOperation = isEraser ? 'destination-out' : 'source-over';

    ctx.beginPath();
    ctx.moveTo(coords.x, coords.y);
    lastPointRef.current = coords;
  };

  const draw = (e) => {
    if (!isDrawingState || !isDrawing) return;
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const coords = getCoordinates(e);
    if (!coords) return;

    const lastPoint = lastPointRef.current;
    if (!lastPoint) {
      lastPointRef.current = coords;
      return;
    }

    // Cập nhật style dựa trên props hiện tại trước khi vẽ
    ctx.strokeStyle = isEraser ? '#FFFFFF' : color;
    ctx.lineWidth = brushSize;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.globalCompositeOperation = isEraser ? 'destination-out' : 'source-over';

    ctx.lineTo(coords.x, coords.y);
    ctx.stroke();

    // Gửi dữ liệu vẽ nếu có callback
    if (onDraw) {
      onDraw({
        x1: lastPoint.x,
        y1: lastPoint.y,
        x2: coords.x,
        y2: coords.y,
        color: color,
        width: brushSize,
        isEraser: isEraser
      });
    }

    lastPointRef.current = coords;
  };

  const stopDrawing = () => {
    setIsDrawingState(false);
    lastPointRef.current = null;
  };

  return (
    <div className="canvas-container">
      {!isDrawing && gameState !== 'playing' && (
        <div className="canvas-overlay">
          <div className="waiting-message">
            <div className="waiting-icon">⏳</div>
            <h3>ĐANG CHỜ</h3>
            <p>Đang chờ người chơi</p>
          </div>
        </div>
      )}

      <canvas
        ref={canvasRef}
        className="drawing-canvas"
        onMouseDown={startDrawing}
        onMouseMove={draw}
        onMouseUp={stopDrawing}
        onMouseLeave={stopDrawing}
        onTouchStart={(e) => {
          e.preventDefault();
          startDrawing(e.touches[0]);
        }}
        onTouchMove={(e) => {
          e.preventDefault();
          draw(e.touches[0]);
        }}
        onTouchEnd={stopDrawing}
      />
    </div>
  );
});

export default Canvas;

