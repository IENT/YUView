# HDR视频宽高比修复方案

## 问题描述
当HDR widget覆盖整个窗口以解决灰色边缘问题后，视频的宽高比出现了变形，变成了正方形。

## 解决方案

### 1. 保持HDR Widget覆盖整个窗口
```cpp
// updateHDROverlayGeometry()函数
m_hdrOverlayWidget->setGeometry(rect());
```
这确保了不会有灰色边缘出现。

### 2. 修改投影矩阵计算
修改了`HDR_VideoWidget::updateProjectionMatrix()`函数，使其：
- 获取视频的原始尺寸和缩放因子
- 计算视频在窗口中的实际像素大小
- 根据视频的实际大小设置投影矩阵，而不是强制填满整个窗口
- 考虑平移偏移量，确保拖动功能正常工作

### 3. 关键代码改动
```cpp
// 计算缩放后的视频尺寸
float videoWidth = frameWidth * zoomFactor;
float videoHeight = frameHeight * zoomFactor;

// 计算归一化的偏移量
float offsetX = viewOffset.x() / (widgetWidth / 2.0f);
float offsetY = -viewOffset.y() / (widgetHeight / 2.0f);

// 设置投影边界
float halfVideoWidth = videoWidth / widgetWidth;
float halfVideoHeight = videoHeight / widgetHeight;

float left = -halfVideoWidth + offsetX;
float right = halfVideoWidth + offsetX;
float top = halfVideoHeight + offsetY;
float bottom = -halfVideoHeight + offsetY;

m_projectionMatrix.ortho(left, right, bottom, top, -1.0f, 1.0f);
```

### 4. 平移支持
在`splitViewWidget::setMoveOffset()`中添加了对HDR widget的通知：
```cpp
if (m_hdrOverlayWidget) {
    m_hdrOverlayWidget->onParentZoomChanged(); // 更新投影矩阵
}
```

## 效果
- HDR widget覆盖整个窗口，避免灰色边缘
- 视频以正确的宽高比在正确的位置渲染
- 缩放和平移功能正常工作
- 背景色与主窗口一致

## 工作原理
1. HDR widget作为一个OpenGL画布覆盖整个窗口
2. OpenGL的clear color设置为与主窗口一致的背景色
3. 视频通过投影矩阵渲染在正确的位置，保持原始宽高比
4. 未被视频覆盖的区域显示背景色