# HDR_VideoWidget OpenGL渲染流程详解

## 1. 顶点数据定义

```cpp
// 全屏四边形的顶点数据（位置 + 纹理坐标）
const float HDR_VideoWidget::s_quadVertices[] = {
    // Positions   // Texture Coords (standard)
    -1.0f, -1.0f,  0.0f, 0.0f,   // Bottom Left  -> Bottom Left in texture
     1.0f, -1.0f,  1.0f, 0.0f,   // Bottom Right -> Bottom Right in texture
     1.0f,  1.0f,  1.0f, 1.0f,   // Top Right    -> Top Right in texture
    -1.0f,  1.0f,  0.0f, 1.0f    // Top Left     -> Top Left in texture
};

// 索引数据
const unsigned int HDR_VideoWidget::s_quadIndices[] = {
    0, 1, 2,   // First triangle
    2, 3, 0    // Second triangle
};
```

## 2. 着色器代码

### 顶点着色器
```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 textureMatrix;
uniform mat4 projectionMatrix;

void main()
{
    gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
}
```

### 片段着色器
```glsl
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D videoTexture;
uniform int renderMode;            // 0=SDR, 1=BT2020_PQ, 2=BT709_Linear
uniform float hdrExposure;         // HDR exposure adjustment
uniform float hdrGamma;            // Gamma correction
uniform float displayMaxLuminance; // Display peak luminance in nits
uniform float sourceMaxLuminance;  // Source content peak luminance in nits

// ST.2084 PQ constants for HDR10
const float m1 = 2610.0 / 4096.0 / 4.0;
const float m2 = 2523.0 / 4096.0 * 128.0;
const float c1 = 3424.0 / 4096.0;
const float c2 = 2413.0 / 4096.0 * 32.0;
const float c3 = 2392.0 / 4096.0 * 32.0;

// Rec.709 to Rec.2020 color space conversion matrix
const mat3 from709to2020 = mat3(
    0.6274, 0.0691, 0.0164,  // Red channel coefficients
    0.3293, 0.9195, 0.0880,  // Green channel coefficients  
    0.0433, 0.0114, 0.8956   // Blue channel coefficients
);

// Decode Rec.709 OETF to linear light
vec3 rec709ToLinear(vec3 v) {
    vec3 lo = v / 4.5;
    vec3 hi = pow((v + 0.099) / 1.099, vec3(1.0 / 0.45));
    bvec3 useHi = greaterThanEqual(v, vec3(0.081));
    return vec3(useHi.x ? hi.x : lo.x,
                useHi.y ? hi.y : lo.y,
                useHi.z ? hi.z : lo.z);
}

// Encode linear absolute luminance to ST.2084 PQ code values
vec3 applyPQ(vec3 linearScene) {
    vec3 absNits = max(linearScene, vec3(0.0)) * sourceMaxLuminance;
    vec3 normalized = clamp(absNits / 10000.0, 0.0, 1.0);
    vec3 Lp = pow(normalized, vec3(m1));
    vec3 numerator = vec3(c1) + vec3(c2) * Lp;
    vec3 denominator = vec3(1.0) + vec3(c3) * Lp;
    return pow(numerator / max(denominator, vec3(1e-6)), vec3(m2));
}

// Apply exposure adjustment for linear/scRGB mode
vec3 applyExposure(vec3 linear) {
    return linear * pow(2.0, hdrExposure);
}

// Apply gamma correction
vec3 applyGamma(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(1.0 / hdrGamma));
}

void main()
{
    vec4 color = texture(videoTexture, TexCoord);
    
    if (renderMode == 1) {
        // BT.2020 + PQ: HDR10 mode
        vec3 rgb709 = clamp(color.rgb, 0.0, 1.0);
        vec3 rgb709_linear = rec709ToLinear(rgb709);
        vec3 rgb2020_linear = from709to2020 * rgb709_linear;
        vec3 pqEncoded = applyPQ(rgb2020_linear);
        FragColor = vec4(pqEncoded, color.a);
        
    } else if (renderMode == 2) {
        // BT709_Linear mode: Apply exposure and gamma
        vec3 exposedColor = applyExposure(color.rgb);
        exposedColor = applyGamma(exposedColor);
        FragColor = vec4(exposedColor, color.a);
        
    } else {
        // SDR mode: Direct output with optional gamma correction
        vec3 sdrColor = applyGamma(color.rgb);
        FragColor = vec4(sdrColor, color.a);
    }
}
```

## 3. 初始化流程

### initializeGL()
```cpp
void HDR_VideoWidget::initializeGL()
{
    // 初始化OpenGL函数
    if (!initializeOpenGLFunctions()) {
        qWarning() << "Failed to initialize OpenGL functions";
        return;
    }
    
    // 设置OpenGL状态
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // 设置清除颜色为父窗口背景色
    QColor bgColor = palette().color(QPalette::Window);
    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
    
    // 初始化组件
    bool shadersOk = initializeShaders();
    bool geometryOk = initializeGeometry();
    bool textureOk = initializeTexture();
}
```

### 几何体初始化
```cpp
bool HDR_VideoWidget::initializeGeometry()
{
    // 创建VAO
    m_vertexArrayObject = new QOpenGLVertexArrayObject(this);
    m_vertexArrayObject->create();
    m_vertexArrayObject->bind();
    
    // 创建VBO
    m_vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vertexBuffer->create();
    m_vertexBuffer->bind();
    m_vertexBuffer->allocate(s_quadVertices, sizeof(s_quadVertices));
    
    // 创建EBO
    m_indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    m_indexBuffer->create();
    m_indexBuffer->bind();
    m_indexBuffer->allocate(s_quadIndices, sizeof(s_quadIndices));
    
    // 设置顶点属性
    // 位置属性 (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // 纹理坐标属性 (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 
                         (void*)(2 * sizeof(float)));
    
    m_vertexArrayObject->release();
    return true;
}
```

## 4. 渲染流程

### paintGL()
```cpp
void HDR_VideoWidget::paintGL()
{
    // 更新清除颜色以匹配父窗口背景
    QColor bgColor;
    if (parentWidget()) {
        bgColor = parentWidget()->palette().color(QPalette::Window);
    } else {
        bgColor = palette().color(QPalette::Window);
    }
    glClearColor(bgColor.redF(), bgColor.greenF(), bgColor.blueF(), 1.0f);
    
    // 清除颜色缓冲区
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 如果未初始化，直接返回
    if (!m_initialized) {
        return;
    }
    
    // 如果有当前帧，则渲染
    if (!m_currentFrame.isNull() && m_videoTexture && m_videoTexture->isCreated()) {
        renderFrame();
        m_frameCount++;
    }
}
```

### renderFrame()
```cpp
void HDR_VideoWidget::renderFrame()
{
    if (!m_shaderProgram || !m_vertexArrayObject || !m_videoTexture) {
        return;
    }
    
    // 绑定着色器程序
    m_shaderProgram->bind();
    
    // 更新uniform变量
    updateShaderUniforms();
    
    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    m_videoTexture->bind();
    m_shaderProgram->setUniformValue(m_textureLocation, 0);
    
    // 绑定VAO并绘制
    m_vertexArrayObject->bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    m_vertexArrayObject->release();
    
    // 清理
    m_videoTexture->release();
    m_shaderProgram->release();
}
```

### 更新Uniform变量
```cpp
void HDR_VideoWidget::updateShaderUniforms()
{
    if (!m_shaderProgram) {
        return;
    }
    
    m_shaderProgram->setUniformValue(m_renderModeLocation, static_cast<int>(m_renderMode));
    m_shaderProgram->setUniformValue(m_hdrExposureLocation, m_hdrExposure);
    m_shaderProgram->setUniformValue(m_hdrGammaLocation, m_hdrGamma);
    m_shaderProgram->setUniformValue(m_displayMaxLuminanceLocation, m_displayMaxLuminance);
    m_shaderProgram->setUniformValue(m_sourceMaxLuminanceLocation, m_sourceMaxLuminance);
    m_shaderProgram->setUniformValue(m_textureMatrixLocation, m_textureMatrix);
    m_shaderProgram->setUniformValue(m_projectionMatrixLocation, m_projectionMatrix);
}
```

## 5. 投影矩阵计算

```cpp
void HDR_VideoWidget::updateProjectionMatrix()
{
    // 获取窗口和视频尺寸
    int widgetWidth = width();
    int widgetHeight = height();
    int frameWidth = m_frameSize.width();
    int frameHeight = m_frameSize.height();
    
    // 获取缩放和偏移
    double zoomFactor = 1.0;
    QPointF viewOffset;
    splitViewWidget* parentView = qobject_cast<splitViewWidget*>(parentWidget());
    if (parentView) {
        double splitPoint = 0.5; int mode = 0;
        parentView->getViewState(viewOffset, zoomFactor, splitPoint, mode);
    }
    
    // 计算缩放后的视频尺寸
    float videoWidth = frameWidth * zoomFactor;
    float videoHeight = frameHeight * zoomFactor;
    
    // 计算归一化的偏移量
    float offsetX = viewOffset.x() / (widgetWidth / 2.0f);
    float offsetY = -viewOffset.y() / (widgetHeight / 2.0f);
    
    // 计算投影边界
    float halfVideoWidth = videoWidth / widgetWidth;
    float halfVideoHeight = videoHeight / widgetHeight;
    
    float left = -halfVideoWidth + offsetX;
    float right = halfVideoWidth + offsetX;
    float top = halfVideoHeight + offsetY;
    float bottom = -halfVideoHeight + offsetY;
    
    // 设置正交投影
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.ortho(left, right, bottom, top, -1.0f, 1.0f);
}
```

## 6. 纹理上传

```cpp
bool HDR_VideoWidget::uploadTextureData(const QImage& image)
{
    // 垂直翻转图像以匹配OpenGL坐标系
    QImage flippedImage = image.flipped(Qt::Vertical);
    
    // 选择合适的格式
    GLenum internalFormat = GL_RGBA16F;  // 高精度内部格式
    GLenum pixelFormat = GL_RGBA;
    GLenum pixelType = GL_UNSIGNED_BYTE;
    
    // 转换图像格式
    QImage textureImage = flippedImage.convertToFormat(QImage::Format_RGBA8888);
    
    // 创建或更新纹理
    if (!m_videoTexture->isCreated()) {
        m_videoTexture->setFormat(QOpenGLTexture::RGBA32F);
        m_videoTexture->setSize(textureImage.width(), textureImage.height());
        m_videoTexture->setMinificationFilter(QOpenGLTexture::Linear);
        m_videoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
        m_videoTexture->create();
    }
    
    // 上传纹理数据
    m_videoTexture->bind();
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 textureImage.width(), textureImage.height(),
                 0, pixelFormat, pixelType, textureImage.constBits());
    m_videoTexture->release();
    
    return true;
}
```

## 渲染流程总结

1. **初始化阶段**：
   - 创建VAO、VBO、EBO
   - 编译和链接着色器
   - 设置顶点属性
   - 创建纹理对象

2. **每帧渲染**：
   - 清除颜色缓冲区（使用父窗口背景色）
   - 绑定着色器程序
   - 更新uniform变量（投影矩阵、渲染模式等）
   - 绑定纹理
   - 绘制全屏四边形
   - 片段着色器根据渲染模式处理颜色

3. **坐标变换**：
   - 顶点着色器使用投影矩阵变换顶点位置
   - 投影矩阵考虑了视频的缩放和平移
   - 保持视频的正确宽高比

4. **HDR处理**：
   - SDR模式：直接输出带gamma校正
   - HDR10模式：Rec.709转Rec.2020，应用PQ曲线
   - Linear模式：应用曝光和gamma调整