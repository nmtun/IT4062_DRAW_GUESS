import React, { forwardRef, useEffect, useImperativeHandle, useRef, useState } from 'react';
import './Canvas.css';

const Canvas = forwardRef(function Canvas(
  { canDraw = false, onDraw },
  ref
) {
  const canvasElRef = useRef(null);
  const lastPosRef = useRef(null);
  const [isDrawingState, setIsDrawingState] = useState(false);
  const [currentColor, setCurrentColor] = useState('#000000');
  const [brushSize, setBrushSize] = useState(5);
  const [isEraser, setIsEraser] = useState(false);

  const initCanvasSize = () => {
    const canvas = canvasElRef.current;
    if (!canvas) return;
    canvas.width = canvas.offsetWidth;
    canvas.height = canvas.offsetHeight;
  };

  useEffect(() => {
    initCanvasSize();
    const onResize = () => initCanvasSize();
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, []);

  useImperativeHandle(ref, () => ({
    clear() {
      const canvas = canvasElRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext('2d');
      ctx.clearRect(0, 0, canvas.width, canvas.height);
    },
    applyRemoteDraw(data) {
      const canvas = canvasElRef.current;
      if (!canvas || !data) return;
      const ctx = canvas.getContext('2d');

      if (data.action === 2) {
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        return;
      }

      if (data.action === 1 || data.action === 3) {
        const colorHex = data.action === 3 ? '#FFFFFF' : (data.colorHex || '#000000');
        ctx.strokeStyle = colorHex;
        ctx.lineWidth = data.width || 5;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';
        ctx.beginPath();
        ctx.moveTo(data.x1 || 0, data.y1 || 0);
        ctx.lineTo(data.x2 || 0, data.y2 || 0);
        ctx.stroke();
      }
    }
  }));

  const getPoint = (e) => {
    const canvas = canvasElRef.current;
    const rect = canvas.getBoundingClientRect();
    return { x: e.clientX - rect.left, y: e.clientY - rect.top };
  };

  const startDrawing = (e) => {
    if (!canDraw) return;
    setIsDrawingState(true);
    lastPosRef.current = getPoint(e);
  };

  const draw = (e) => {
    if (!canDraw || !isDrawingState) return;
    const canvas = canvasElRef.current;
    const ctx = canvas.getContext('2d');

    const p2 = getPoint(e);
    const p1 = lastPosRef.current;
    if (!p1) {
      lastPosRef.current = p2;
      return;
    }

    ctx.strokeStyle = isEraser ? '#FFFFFF' : currentColor;
    ctx.lineWidth = brushSize;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();

    if (onDraw) {
      onDraw({
        action: 1,
        x1: p1.x,
        y1: p1.y,
        x2: p2.x,
        y2: p2.y,
        color: currentColor,
        width: brushSize,
        isEraser
      });
    }

    lastPosRef.current = p2;
  };

  const stopDrawing = () => {
    setIsDrawingState(false);
    lastPosRef.current = null;
  };

  const clearCanvas = () => {
    if (!canDraw) return;
    const canvas = canvasElRef.current;
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (onDraw) onDraw({ action: 2 });
  };

  return (
    <div className="canvas-container">
      <div className="canvas-tools">
        <div className="tool-group">
          <label>Màu:</label>
          <input
            type="color"
            value={currentColor}
            onChange={(e) => setCurrentColor(e.target.value)}
            disabled={!canDraw || isEraser}
          />
        </div>
        <div className="tool-group">
          <label>Size:</label>
          <input
            type="range"
            min="1"
            max="20"
            value={brushSize}
            onChange={(e) => setBrushSize(parseInt(e.target.value, 10))}
            disabled={!canDraw}
          />
          <span>{brushSize}</span>
        </div>
        <div className="tool-group">
          <label>
            <input
              type="checkbox"
              checked={isEraser}
              onChange={(e) => setIsEraser(e.target.checked)}
              disabled={!canDraw}
            />
            Tẩy
          </label>
        </div>
        <button className="clear-btn" onClick={clearCanvas} disabled={!canDraw}>
          Xóa
        </button>
      </div>

      {!canDraw && (
        <div className="canvas-overlay">
          <div className="waiting-message">
            <div className="waiting-icon">⏳</div>
            <h3>ĐANG CHỜ</h3>
            <p>Chỉ người vẽ mới thao tác được</p>
          </div>
        </div>
      )}

      <canvas
        ref={canvasElRef}
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

