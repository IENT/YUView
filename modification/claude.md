# HDR显示问题修复总结

## 问题诊断

### 1. 图像上下颠倒
**原因**：OpenGL纹理坐标系（原点在左下角）与QImage坐标系（原点在左上角）不匹配。

### 2. 色彩错误
**原因链**：
1. YUV转RGB正确生成了16位图像（`QImage::Format_RGBA64_Premultiplied`）用于10位数据
2. 但在`videoHandlerYUV::getCurrentFrameAsImage()`中被转换成8位（`Format_ARGB32_Premultiplied`）
3. HDR Widget收到8位图像后再次转换，导致10位精度完全丢失
4. 灰度图像能正确显示是因为它们对精度损失不敏感

## 修复方案

### 修复1：纹理坐标翻转（HDR_VideoWidget.cpp）
```cpp
// 修改顶点数据，翻转Y坐标
const float HDR_VideoWidget::s_quadVertices[] = {
    // Positions   // Texture Coords (Y-flipped)
    -1.0f, -1.0f,  0.0f, 1.0f,   // Bottom Left  -> Top Left in texture
     1.0f, -1.0f,  1.0f, 1.0f,   // Bottom Right -> Top Right in texture
     1.0f,  1.0f,  1.0f, 0.0f,   // Top Right    -> Bottom Right in texture
    -1.0f,  1.0f,  0.0f, 0.0f    // Top Left     -> Bottom Left in texture
};
```

### 修复2：保持16位图像格式（videoHandlerYUV.cpp）
```cpp
QImage videoHandlerYUV::getCurrentFrameAsImage()
{
  // 关键：对于10位HDR，保持16位格式
  if (enable10BitDisplay && srcPixelFormat.getBitsPerSample() == 10) {
    if (currentImage.format() == QImage::Format_RGBA64 ||
        currentImage.format() == QImage::Format_RGBA64_Premultiplied) {
      return currentImage;  // 直接返回，不转换格式！
    }
  }
  // ... 8位处理路径
}
```

### 修复3：正确处理16位纹理上传（HDR_VideoWidget.cpp）
```cpp
bool HDR_VideoWidget::uploadTextureData(const QImage& image)
{
  // 检测并正确处理16位输入
  if (image.format() == QImage::Format_RGBA64 || 
      image.format() == QImage::Format_RGBA64_Premultiplied) {
    
    // 使用16位纹理格式
    internalFormat = GL_RGBA16F;
    pixelFormat = GL_RGBA;
    pixelType = GL_UNSIGNED_SHORT;  // 16位每通道
    
    // 上传16位数据
    const quint16* pixelData = reinterpret_cast<const quint16*>(image.constBits());
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 
                 width, height, 0, pixelFormat, pixelType, pixelData);
  }
}
```

### 修复4：增强的HDR着色器（HDR_VideoWidget.cpp）
```glsl
// Fragment shader中正确处理10位到HDR的转换
if (renderMode == 1) {  // BT2020_PQ mode
    // 输入已归一化，应用曝光调整
    vec3 exposedColor = applyExposure(color.rgb);
    
    // 应用PQ OETF用于HDR10输出
    vec3 pqColor = PQ_OETF(exposedColor * sourceMaxLuminance);
    FragColor = vec4(pqColor, color.a);
}
```

### 修复5：确保YUV转换使用16位路径（videoHandlerYUV.cpp）
```cpp
void videoHandlerYUV::loadFrame(int frameIndex)
{
  // 传递10位标志到转换函数
  convertYUVToImage(
      sourceBuffer, 
      tmpImage, 
      srcPixelFormat, 
      curFrameSize, 
      conversionSettings,
      enable10BitDisplay && srcPixelFormat.getBitsPerSample() == 10);  // 启用16位转换
}
```

## 数据流程图

```
10-bit YUV Data
     ↓
convertYUVToImage() [使用16位转换路径]
     ↓
QImage::Format_RGBA64_Premultiplied (16-bit/channel)
     ↓
getCurrentFrameAsImage() [保持16位格式，不转换！]
     ↓
HDRRenderingManager::updateHDRFrame() [传递16位图像]
     ↓
HDR_VideoWidget::uploadTextureData() [使用GL_UNSIGNED_SHORT上传]
     ↓
OpenGL Texture (GL_RGBA16F internal format)
     ↓
Fragment Shader [PQ/Linear HDR处理]
     ↓
HDR Display
```

## 测试验证点

1. **检查日志输出**：
   - 应看到 "Received 16-bit image for true 10-bit HDR rendering"
   - 应看到 "Uploaded 16-bit texture data"

2. **验证图像方向**：
   - 图像应正面向上，不再颠倒

3. **验证色彩精度**：
   - 10位灰阶应显示1024级（不是256级）
   - 彩色图像应保持正确的色彩，不再有色彩失真

4. **性能检查**：
   - 16位处理可能稍慢，但质量提升明显

## 编译注意事项

这些修改涉及多个文件的协同工作：
- `HDR_VideoWidget.cpp/h` - 纹理坐标和16位纹理处理
- `videoHandlerYUV.cpp` - 保持16位图像格式
- `HDRRenderingManager.cpp` - 正确传递16位数据

确保所有文件都更新并重新编译，以避免不一致的问题。
