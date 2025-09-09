# HDR投影矩阵最终修复方案

## 问题描述
1. 视频显示为正方形而不是保持原始宽高比
2. 缩放方向相反（放大变成缩小，缩小变成放大）

## 根本原因
之前的投影矩阵计算过于复杂，没有正确模拟SDR模式的渲染行为。

## 解决方案

### SDR模式渲染原理
在SDR模式下，QPainter的渲染流程是：
1. `painter.translate(centerPoint + offset)` - 移动到中心点+偏移
2. `item->drawItem(&painter, frame, zoom, ...)` - 以zoom倍率绘制

### HDR模式的等效实现
```cpp
// 计算缩放后的视频尺寸（像素）
float scaledVideoWidth = frameWidth * zoomFactor;
float scaledVideoHeight = frameHeight * zoomFactor;

// 转换为NDC坐标（-1到1）
float scaleX = scaledVideoWidth / widgetWidth;
float scaleY = scaledVideoHeight / widgetHeight;

// 偏移量也转换为NDC
float offsetX = (viewOffset.x() * 2.0f) / widgetWidth;
float offsetY = -(viewOffset.y() * 2.0f) / widgetHeight;

// 构建投影矩阵
m_projectionMatrix.setToIdentity();
m_projectionMatrix.translate(offsetX, offsetY, 0.0f);
m_projectionMatrix.scale(scaleX, scaleY, 1.0f);
```

### 关键改进
1. **简化计算**：直接使用scale和translate，而不是复杂的ortho投影
2. **匹配SDR行为**：确保缩放和平移的行为与SDR模式完全一致
3. **正确的宽高比**：视频按原始宽高比缩放，不会变形

## 效果
- 视频保持正确的宽高比（矩形而不是正方形）
- 缩放方向正确（滚轮向上放大，向下缩小）
- 平移功能正常工作
- 整体行为与SDR模式一致

## 技术细节
- OpenGL的NDC坐标范围是-1到1
- 需要将像素坐标转换为NDC坐标
- Y轴需要取反，因为OpenGL的Y轴方向与Qt相反
- 使用translate和scale组合来实现投影变换