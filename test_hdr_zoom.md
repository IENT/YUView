# HDR缩放功能测试指南

## 测试目标
验证HDR模式下的缩放功能已经修复，并与SDR模式的行为保持一致。

## 已修复的问题

### 1. 缩放比例异常
- **问题描述**：HDR模式下使用鼠标滚轮缩放时，画布长度不变而宽度却在变化
- **修复方案**：在`HDR_VideoWidget::updateProjectionMatrix()`中添加了对父widget缩放因子的获取和应用
- **关键代码**：
  ```cpp
  // Get zoom factor from parent split view widget
  double zoomFactor = 1.0;
  splitViewWidget* parentView = qobject_cast<splitViewWidget*>(parentWidget());
  if (parentView) {
      QPointF offset; double zoom = 1.0; double splitPoint = 0.5; int mode = 0;
      parentView->getViewState(offset, zoom, splitPoint, mode);
      zoomFactor = zoom;
  }
  
  // Apply zoom factor to the projection bounds
  float zoomScale = 1.0f / static_cast<float>(zoomFactor);
  ```

### 2. 倍率数字显示消失
- **问题描述**：HDR模式下左上角的倍率数字不显示
- **修复方案**：改进了`HDR_VideoWidget::paintEvent()`中的绘制逻辑，确保在OpenGL渲染完成后正确绘制倍率数字
- **关键改进**：
  - 添加`glFinish()`确保OpenGL渲染完成
  - 使用白色文字+黑色描边提高可见性
  - 调整绘制位置避免被遮挡

### 3. 缩放事件响应
- **问题描述**：HDR widget的缩放不能实时响应父widget的缩放变化
- **修复方案**：在`splitViewWidget::setZoomFactor()`中添加了对HDR widget的通知
- **关键代码**：
  ```cpp
  // Notify HDR widget of zoom change if it exists
  if (m_hdrOverlayWidget) {
      m_hdrOverlayWidget->onParentZoomChanged();
  }
  ```

## 测试步骤

### 1. 基本缩放测试
1. 启动YUView并加载一个视频文件
2. 切换到HDR模式（如果支持）
3. 使用鼠标滚轮进行缩放
4. **验证点**：
   - 视频应该均匀缩放（长宽比保持不变）
   - 缩放行为应与SDR模式一致
   - 左上角应显示当前倍率（如x2, x0.5等）

### 2. 缩放比例测试
1. 在HDR模式下，使用以下快捷键测试固定倍率：
   - `Ctrl+0`: 缩放到100%
   - `Ctrl+1`: 缩放到适应窗口
   - `Ctrl+2`: 缩放到200%
2. **验证点**：
   - 每个倍率下视频的宽高比应保持正确
   - 倍率数字应正确显示

### 3. 拖拽和缩放组合测试
1. 在HDR模式下放大视频到200%
2. 使用鼠标拖拽移动视频
3. 再次使用滚轮缩放
4. **验证点**：
   - 拖拽后缩放应该正常工作
   - 缩放中心点应该正确（鼠标位置为中心）

### 4. HDR/SDR切换测试
1. 在SDR模式下设置一个特定的缩放倍率（如150%）
2. 切换到HDR模式
3. 再切换回SDR模式
4. **验证点**：
   - 切换过程中倍率应保持不变
   - UI交互体验应保持一致

### 5. 窗口调整测试
1. 在HDR模式下设置缩放到适应窗口
2. 调整窗口大小
3. **验证点**：
   - 视频应自动调整以适应新的窗口大小
   - 宽高比应保持正确

## 预期结果
- HDR模式下的所有缩放操作应与SDR模式完全一致
- 倍率数字应始终可见（除非倍率为1.0）
- 视频的宽高比在任何缩放级别下都应保持正确
- 用户体验应该流畅，没有明显的差异

## 注意事项
- 测试需要在支持HDR的显示器上进行才能启用HDR模式
- 如果系统不支持HDR，程序应该自动回退到SDR模式
- 测试时注意观察控制台输出，查看是否有错误信息