/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "HDR_RhiVideoWindow.h"
#include "../yuv/PixelFormatYUV.h"

// Use modular HDR classes (same directory)
#include "HDRColorConversion.h"
#include "HDRYUVRepack.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QFileInfo>
#include <cstring>
#include <algorithm>

#ifndef DEBUG_HDR_WINDOW
#define DEBUG_HDR_WINDOW 0
#endif

#if DEBUG_HDR_WINDOW
#define DEBUG_HDR_WINDOW_LOG(...) qDebug() << "[HDR-Window]" << __VA_ARGS__
#else
#define DEBUG_HDR_WINDOW_LOG(...) ((void)0)
#endif

// Restrict: promise no aliasing through this pointer (upload hot path).
#if defined(_MSC_VER)
#define HDR_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define HDR_RESTRICT __restrict__
#else
#define HDR_RESTRICT
#endif

// Include QShaderBaker for runtime shader compilation if available
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#if __has_include(<QtShaderTools/QShaderBaker>)
#include <QtShaderTools/QShaderBaker>
#define HAVE_SHADER_BAKER 1
#elif __has_include(<rhi/qshaderbaker.h>)
#include <rhi/qshaderbaker.h>
#define HAVE_SHADER_BAKER 1
#endif
#endif

// Helper function to compile shader from GLSL source at runtime
// This is used when pre-compiled .qsb files are not available
static QShader compileShaderFromSource(const QByteArray& source, QShader::Stage stage)
{
#ifdef HAVE_SHADER_BAKER
  QShaderBaker baker;
  baker.setSourceString(source, stage);
  baker.setGeneratedShaderVariants({QShader::StandardShader});

  QList<QShaderBaker::GeneratedShader> targets;
  // SPIR-V for Vulkan
  targets.append({QShader::SpirvShader, QShaderVersion(100)});
  // HLSL for Direct3D 11/12
  targets.append({QShader::HlslShader, QShaderVersion(50)});
  // MSL for Metal
  targets.append({QShader::MslShader, QShaderVersion(12)});
  // GLSL for OpenGL
  targets.append({QShader::GlslShader, QShaderVersion(440)});
  targets.append({QShader::GlslShader, QShaderVersion(310, QShaderVersion::GlslEs)});
  baker.setGeneratedShaders(targets);

  QShader shader = baker.bake();
  if (!shader.isValid()) {
    qWarning() << "[HDR-RHI] Shader compilation failed:" << baker.errorMessage();
  }
  return shader;
#else
  Q_UNUSED(source);
  Q_UNUSED(stage);
  qWarning() << "[HDR-RHI] QShaderBaker not available, cannot compile shaders at runtime";
  return QShader();
#endif
}

// Load shader from .qsb file, or compile from source as fallback
// Non-static to allow use from HDR_RhiVideoWindow_Overlays.cpp
QShader loadOrCompileShader(const QString& qsbPath, const QString& sourcePath, QShader::Stage stage)
{
    // Prefer loading pre-compiled .qsb from Qt resources first.
  // If the resource is missing (common when shaders.qrc is not registered or .qsb files are not embedded),
  // also try loading .qsb from disk next to the executable: <appDir>/shaders/<fileName>.qsb.
  // This provides a stable path for deployments where QtShaderTools is not available at runtime.

         // First try to load pre-compiled .qsb file
  QFile qsbFile(qsbPath);
  if (qsbFile.open(QIODevice::ReadOnly)) {
    QShader shader = QShader::fromSerialized(qsbFile.readAll());
    if (shader.isValid()) {
      return shader;
    }
  }

         // Try to load the .qsb from disk (next to the executable) as a deployment fallback.
         // Example path on Windows: <YUView.exe dir>\shaders\hdr_rhi_vertex.vert.qsb
  const QString qsbFileName = QFileInfo(qsbPath).fileName();
  if (!qsbFileName.isEmpty()) {
    const QString qsbDiskPath = QCoreApplication::applicationDirPath() + QDir::separator()
    + QStringLiteral("shaders") + QDir::separator()
      + qsbFileName;
    QFile qsbDiskFile(qsbDiskPath);
    if (qsbDiskFile.open(QIODevice::ReadOnly)) {
      QShader shader = QShader::fromSerialized(qsbDiskFile.readAll());
      if (shader.isValid()) {
        return shader;
      }
    }
  }

         // Fall back to runtime compilation from source
  QFile sourceFile(sourcePath);
  if (sourceFile.open(QIODevice::ReadOnly)) {
    QByteArray source = sourceFile.readAll();
    return compileShaderFromSource(source, stage);
  }

  qWarning() << "[HDR-RHI] Failed to load shader:" << qsbPath << "or" << sourcePath
             << "(and no valid .qsb was found on disk)";
  return QShader();
}
// Vertex data for fullscreen quad: positions (x,y) and texture coords (u,v)
static const float kQuadVertices[] = {
  // Positions   // TexCoords
  -1.0f, -1.0f,  0.0f, 0.0f,
  1.0f, -1.0f,  1.0f, 0.0f,
  1.0f,  1.0f,  1.0f, 1.0f,
  -1.0f,  1.0f,  0.0f, 1.0f
};

// Index buffer for two triangles forming a quad
static const quint16 kQuadIndices[] = {
  0, 1, 2,
  2, 3, 0
};

// HDR_RhiVideoWindow：Qt QRhi HDR 窗口。
// 交换链固定为 scRGB 线性 RGBA16F。YUV 与 RGB 都先进入同一待上传队列，
// render() 再在 GUI 线程发布帧尺寸并上传，避免宽高和像素缓冲被拆开。
// 色调映射请求小于等于 0 时直通；真正写入着色器的亮度按 PQ / HLG 的范围现算。
HDR_RhiVideoWindow::HDR_RhiVideoWindow(QWindow* parent)
    : QWindow(parent)
      , m_renderMode(Mode_BT2020_PQ_10bit)
{
  // Set surface type for RHI compatibility
  // On Windows with D3D12, this will be ignored but needed for OpenGL fallback
  setSurfaceType(QSurface::OpenGLSurface);

  m_textureMatrix.setToIdentity();
  m_projectionMatrix.setToIdentity();
}

HDR_RhiVideoWindow::~HDR_RhiVideoWindow()
{
  releaseRhiResources();
}

void HDR_RhiVideoWindow::releaseRhiResources()
{
  // Release resources in reverse order of creation

  // YUV pipeline resources
  m_yuvPipeline.reset();
  m_yuvSrb.reset();
  m_yuvUniformBuffer.reset();
  m_texV.reset();
  m_texU.reset();
  m_texY.reset();

         // RGB pipeline resources
  m_pipeline.reset();
  m_srb.reset();
  m_sampler.reset();
  m_videoTexture.reset();
  m_uniformBuffer.reset();
  m_indexBuffer.reset();
  m_vertexBuffer.reset();

  // Overlay pipeline resources (must be released before m_rhi)
  m_overlayPipeline.reset();
  m_overlaySrb.reset();
  m_overlayTexture.reset();
  m_overlaySampler.reset();
  m_overlayUniformBuffer.reset();
  m_overlayResourcesInitialized = false;

  m_renderPassDesc.reset();
  m_swapChain.reset();
  m_rhi.reset();
  m_fallbackSurface.reset();

  m_initialized = false;
}

/*
 * 详尽中文注释：
 * 函数名称：HDR_RhiVideoWindow::releaseRhiResources
 * 功能描述：显式释放该窗口关联的所有 Qt RHI (Rendering Hardware Interface) 图形渲染资源与实例。
 * 算法原理与生命周期管理：
 * 1. 在图形渲染 API (如 Vulkan, Metal, Direct3D, OpenGL) 中，所有由 RHI 实例分配的子资源 (如 Buffer, Texture, Sampler, Pipeline, SwapChain 等)
 *    其生命周期都严格绑定并依赖于底层的 QRhi 实例。如果在 QRhi 实例销毁后依然尝试析构或访问子资源，将导致极其严重的悬空指针访问 (Dangling Pointer) 或驱动级别的内存破坏。
 * 2. 因此，本函数严格遵循“逆序销毁”原则：优先销毁顶层的渲染管线 (Pipeline)、着色器资源绑定 (SRB)，再销毁具体的数据缓冲区 (Buffer) 与图像纹理 (Texture)，
 *    其次销毁渲染通道描述符 (RenderPassDesc) 与交换链 (SwapChain)。
 * 3. 针对 OpenGL (GLES2) 回退路径，RHI 的释放需要在一个有效的绘图表面 (Surface) 上将 OpenGL 上下文设置为当前 (Make Current) 才能安全销毁底层的 OpenGL 资源。
 *    为此，必须首先显式重置并销毁 QRhi 实例 (m_rhi.reset())，在此期间 RHI 会利用 fallbackSurface 完成最后一次上下文清理；在这之后，
 *    才能安全地销毁并释放回退表面实例 (m_fallbackSurface.reset())。若将其顺序颠倒，会导致 RHI 析构时无法找到有效表面来绑定 OpenGL 上下文，进而导致 GPU 内存泄漏或程序崩溃。
 */

bool HDR_RhiVideoWindow::initializeRhi()
{
#ifdef Q_OS_WIN
  // Try D3D12 backend first (Qt 6.5+), then D3D11
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  QRhiD3D12InitParams d3d12Params;
  d3d12Params.enableDebugLayer = false;
  m_rhi.reset(QRhi::create(QRhi::D3D12, &d3d12Params));
#endif

  if (!m_rhi) {
    QRhiD3D11InitParams d3d11Params;
    m_rhi.reset(QRhi::create(QRhi::D3D11, &d3d11Params));
  }

#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  // Checked before any UNIX/Linux branch: macOS/iOS also define Q_OS_UNIX.
  QRhiMetalInitParams metalParams;
  m_rhi.reset(QRhi::create(QRhi::Metal, &metalParams));

#elif defined(Q_OS_LINUX)
#if QT_CONFIG(vulkan)
  QRhiVulkanInitParams vulkanParams;
  vulkanParams.inst = nullptr;
  m_rhi.reset(QRhi::create(QRhi::Vulkan, &vulkanParams));
#endif
#endif

  // Fallback to OpenGL if platform-specific backend fails / is unavailable
  if (!m_rhi) {
    QRhiGles2InitParams glParams;
    m_fallbackSurface.reset(QRhiGles2InitParams::newFallbackSurface());
    glParams.fallbackSurface = m_fallbackSurface.get();
    m_rhi.reset(QRhi::create(QRhi::OpenGLES2, &glParams));
  }

  if (!m_rhi) {
    qCritical() << "[HDR-RHI] Failed to create QRhi instance!";
    emit hdrNotSupported("Failed to initialize rendering backend");
    return false;
  }

  return true;
}

bool HDR_RhiVideoWindow::initializeSwapChain()
{
  m_swapChain.reset(m_rhi->newSwapChain());
  if (!m_swapChain) {
    qCritical() << "[HDR-RHI] Failed to create swap chain!";
    return false;
  }

  m_swapChain->setWindow(this);
  m_swapChain->setFlags(QRhiSwapChain::UsedAsTransferSource);

  // UNIFIED HDR MODE: Use HDRExtendedSrgbLinear (RGBA16F + scRGB linear light) for all modes
  // This format provides:
  // - 16-bit float precision per channel (no banding artifacts)
  // - scRGB linear light color space (values > 1.0 represent HDR luminance)
  // - PQ/HLG/Linear processing applied in the fragment shader
  // - Maximum flexibility for different HDR transfer functions
  m_swapChain->setFormat(QRhiSwapChain::HDRExtendedSrgbLinear);

  m_renderPassDesc.reset(m_swapChain->newCompatibleRenderPassDescriptor());
  if (!m_renderPassDesc) {
    qCritical() << "[HDR-RHI] Failed to create render pass descriptor!";
    return false;
  }

  m_swapChain->setRenderPassDescriptor(m_renderPassDesc.get());

  if (!m_swapChain->createOrResize()) {
    qCritical() << "[HDR-RHI] Failed to create/resize swap chain!";
    return false;
  }

  // Query peak luminance from the display (used by overlays)
  QRhiSwapChainHdrInfo hdrInfo = m_swapChain->hdrInfo();
  m_displayMaxLuminance = 1000.0f;
  if (hdrInfo.limitsType == QRhiSwapChainHdrInfo::LuminanceInNits) {
    m_displayMaxLuminance = hdrInfo.limits.luminanceInNits.maxLuminance;
  } else if (hdrInfo.limitsType == QRhiSwapChainHdrInfo::ColorComponentValue) {
    // scRGB: 1.0 = 80 nits (SDR reference white)
    m_displayMaxLuminance = hdrInfo.limits.colorComponentValue.maxColorComponentValue * 80.0f;
  }
  m_displayMaxLuminance = std::clamp(m_displayMaxLuminance, 100.0f, 10000.0f);


  return true;
}

bool HDR_RhiVideoWindow::initializePipeline()
{
  // Create vertex buffer for quad
  m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
                                        QRhiBuffer::VertexBuffer,
                                        sizeof(kQuadVertices)));
  if (!m_vertexBuffer->create()) {
    qCritical() << "[HDR-RHI] Failed to create vertex buffer!";
    return false;
  }

         // Create index buffer
  m_indexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
                                       QRhiBuffer::IndexBuffer,
                                       sizeof(kQuadIndices)));
  if (!m_indexBuffer->create()) {
    qCritical() << "[HDR-RHI] Failed to create index buffer!";
    return false;
  }

         // Create uniform buffer
  m_uniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::UniformBuffer,
                                         sizeof(UniformData)));
  if (!m_uniformBuffer->create()) {
    qCritical() << "[HDR-RHI] Failed to create uniform buffer!";
    return false;
  }

         // Create placeholder texture (RGBA16F for HDR linear light output)
  m_videoTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA16F, QSize(64, 64)));
  if (!m_videoTexture->create()) {
    qCritical() << "[HDR-RHI] Failed to create video texture!";
    return false;
  }

  // Create sampler with nearest-neighbor filtering for pixel-accurate display
  // CRITICAL: Use Nearest filtering to preserve sharp pixel boundaries
  // Linear filtering causes "pixel smoothing" which hides compression artifacts
  // (mosquito noise, ringing effects) that video analysts need to see
  m_sampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                    QRhiSampler::None,
                                    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
  if (!m_sampler->create()) {
    qCritical() << "[HDR-RHI] Failed to create sampler!";
    return false;
  }

         // Create shader resource bindings
  m_srb.reset(m_rhi->newShaderResourceBindings());
  m_srb->setBindings({
    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer.get()),
    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_videoTexture.get(), m_sampler.get())
  });
  if (!m_srb->create()) {
    qCritical() << "[HDR-RHI] Failed to create shader resource bindings!";
    return false;
  }

         // Load shaders - try pre-compiled .qsb first, fall back to runtime compilation
  QShader vertexShader = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_vertex.vert.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_vertex.vert"),
    QShader::VertexStage);

  QShader fragmentShader = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_fragment.frag.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_fragment.frag"),
    QShader::FragmentStage);

  if (!vertexShader.isValid() || !fragmentShader.isValid()) {
    qCritical() << "[HDR-RHI] Failed to load RGB shaders!";
    qCritical() << "[HDR-RHI] Vertex shader valid:" << vertexShader.isValid();
    qCritical() << "[HDR-RHI] Fragment shader valid:" << fragmentShader.isValid();
    return false;
  }

         // Create graphics pipeline
  m_pipeline.reset(m_rhi->newGraphicsPipeline());
  m_pipeline->setShaderStages({
    { QRhiShaderStage::Vertex, vertexShader },
    { QRhiShaderStage::Fragment, fragmentShader }
  });

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({
    { 4 * sizeof(float) }  // stride: 2 floats pos + 2 floats texcoord
  });
  inputLayout.setAttributes({
    { 0, 0, QRhiVertexInputAttribute::Float2, 0 },                    // position
    { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }     // texcoord
  });
  m_pipeline->setVertexInputLayout(inputLayout);
  m_pipeline->setShaderResourceBindings(m_srb.get());
  m_pipeline->setRenderPassDescriptor(m_renderPassDesc.get());

  if (!m_pipeline->create()) {
    qCritical() << "[HDR-RHI] Failed to create graphics pipeline!";
    return false;
  }

  return true;
}

bool HDR_RhiVideoWindow::initializeYUVPipeline()
{

  // Create YUV uniform buffer (larger to hold color matrix)
  m_yuvUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                            QRhiBuffer::UniformBuffer,
                                            sizeof(YUVUniformData)));
  if (!m_yuvUniformBuffer->create()) {
    qCritical() << "[HDR-RHI] Failed to create YUV uniform buffer!";
    return false;
  }

         // Create Y, U, V textures (R16 format for 10-bit data)
  m_texY.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(64, 64)));
  m_texU.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(32, 32)));
  m_texV.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(32, 32)));

  if (!m_texY->create() || !m_texU->create() || !m_texV->create()) {
    qCritical() << "[HDR-RHI] Failed to create YUV textures!";
    return false;
  }

         // Create shader resource bindings for YUV pipeline
  m_yuvSrb.reset(m_rhi->newShaderResourceBindings());
  m_yuvSrb->setBindings({
    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_yuvUniformBuffer.get()),
    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_texY.get(), m_sampler.get()),
    QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_texU.get(), m_sampler.get()),
    QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, m_texV.get(), m_sampler.get())
  });
  if (!m_yuvSrb->create()) {
    qCritical() << "[HDR-RHI] Failed to create YUV shader resource bindings!";
    return false;
  }

         // Load YUV shaders - try pre-compiled .qsb first, fall back to runtime compilation
  QShader yuvVertexShader = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_yuv_vertex.vert.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_yuv_vertex.vert"),
    QShader::VertexStage);

  QShader yuvFragmentShader = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_yuv_fragment.frag.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_yuv_fragment.frag"),
    QShader::FragmentStage);

  if (!yuvVertexShader.isValid() || !yuvFragmentShader.isValid()) {
    qCritical() << "[HDR-RHI] Failed to load YUV shaders!";
    qCritical() << "[HDR-RHI] YUV vertex shader valid:" << yuvVertexShader.isValid();
    qCritical() << "[HDR-RHI] YUV fragment shader valid:" << yuvFragmentShader.isValid();
    return false;
  }

         // Create YUV graphics pipeline
  m_yuvPipeline.reset(m_rhi->newGraphicsPipeline());
  m_yuvPipeline->setShaderStages({
    { QRhiShaderStage::Vertex, yuvVertexShader },
    { QRhiShaderStage::Fragment, yuvFragmentShader }
  });

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({
    { 4 * sizeof(float) }
  });
  inputLayout.setAttributes({
    { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
    { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) }
  });
  m_yuvPipeline->setVertexInputLayout(inputLayout);
  m_yuvPipeline->setShaderResourceBindings(m_yuvSrb.get());
  m_yuvPipeline->setRenderPassDescriptor(m_renderPassDesc.get());

  if (!m_yuvPipeline->create()) {
    qCritical() << "[HDR-RHI] Failed to create YUV graphics pipeline!";
    return false;
  }

  return true;
}

bool HDR_RhiVideoWindow::tryInitialize()
{
  // 窗口尚未暴露或已经失败时直接返回。这两条在进入临界区之前判断，
  // 避免把“还不能初始化”和“正在初始化”混成同一种失败。
  if (m_initialized)
    return true;
  if (m_initializationFailed)
    return false;
  if (!isExposed())
  {
    if (!m_loggedNotExposedState)
    {
      DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): window not exposed yet");
      m_loggedNotExposedState = true;
    }
    return false;
  }

  m_loggedNotExposedState = false;

  // compare_exchange 已经是非阻塞门闩：同一时刻只有一个调用能进入初始化。
  // 第二个调用（含同线程重入）在这里返回。门闩后面再加互斥锁没有第二个进入者，
  // 属于死代码，因此初始化只保留这个原子标志。失败时尚未完成，必须返回 false。
  bool expected = false;
  if (!m_initializationInProgress.compare_exchange_strong(expected, true))
  {
    DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): initialization already in progress");
    return false;
  }

  // 无论成功、失败还是异常，离开函数时都要放下门闩，否则窗口会永久无法再初始化。
  struct InitializationGuard
  {
    std::atomic<bool>& inProgress;
    ~InitializationGuard() { inProgress.store(false, std::memory_order_release); }
  } guard{m_initializationInProgress};

  DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): starting RHI initialization");

  const bool success = initializeRhi() && initializeSwapChain() &&
                       initializePipeline() && initializeYUVPipeline();
  if (!success)
  {
    DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): failed");
    m_initializationFailed = true;
    emit hdrNotSupported("HDR rendering initialization failed");
    return false;
  }

  m_initialized = true;

  // 顶点缓冲只上传一次。此时门闩仍为 true，信号回调里再次进入 tryInitialize()
  // 会先看到 m_initialized 并返回，不会重入这段上传。
  if (m_rhi && m_vertexBuffer && m_indexBuffer)
  {
    QRhiCommandBuffer* cb = nullptr;
    if (m_rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess)
    {
      QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
      if (batch)
      {
        batch->uploadStaticBuffer(m_vertexBuffer.get(), kQuadVertices);
        batch->uploadStaticBuffer(m_indexBuffer.get(), kQuadIndices);
        cb->resourceUpdate(batch);
      }
      m_rhi->endOffscreenFrame();
    }
  }

  DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): success");
  emit widgetInitialized();
  return true;
}

void HDR_RhiVideoWindow::exposeEvent(QExposeEvent* event)
{
  Q_UNUSED(event);

  if (isExposed()) {
    // Keep exposeEvent lightweight and delegate initialization
    // to a single shared implementation (also used by explicit tryInitialize()).
    (void)tryInitialize();

    if (m_initialized) {
      render();
    }
  }
}

void HDR_RhiVideoWindow::resizeEvent(QResizeEvent* event)
{
  Q_UNUSED(event);

  if (m_swapChain && m_initialized) {
    m_swapChain->createOrResize();
    updateProjectionMatrix();
  }
}

bool HDR_RhiVideoWindow::event(QEvent* event)
{
  switch (event->type()) {
  case QEvent::UpdateRequest:
    render();
    return true;
  default:
    return QWindow::event(event);
  }
}

HDR_RhiVideoWindow::PendingUpload HDR_RhiVideoWindow::takePendingUpload()
{
  // 在锁内把“这一帧的字节、格式、宽高、色彩矩阵”一次取走。
  // 上传阶段不再读 m_frameSize，避免生产者已经写下新分辨率时，旧缓冲按新尺寸解释。
  PendingUpload upload;
  QMutexLocker pendingLock(&m_pendingYUVMutex);
  const bool pendingYUVUsable = m_yuvDataPending && !m_pendingYUVData.isEmpty() &&
                                m_pendingYUVWidth > 0 && m_pendingYUVHeight > 0;
  if (m_yuvDataPending && !pendingYUVUsable)
  {
    // 宽高和字节必须成套。不成套的记录直接丢掉，避免后面把 0 尺寸发布成投影矩阵。
    m_yuvDataPending = false;
    m_pendingYUVData.clear();
    m_pendingYUVWidth = 0;
    m_pendingYUVHeight = 0;
  }

  if (pendingYUVUsable)
  {
    upload.hasYUV = true;
    upload.yuvData = std::move(m_pendingYUVData);
    m_pendingYUVData.clear();
    upload.yuvFormat = m_pendingYUVFormat;
    upload.colorConversion = m_pendingColorConversion;
    upload.yuvWidth = m_pendingYUVWidth;
    upload.yuvHeight = m_pendingYUVHeight;
    upload.sequence = m_pendingUploadSequence;
    m_yuvDataPending = false;
  }
  else if (m_rgbFramePending && !m_pendingRGBFrame.isNull())
  {
    upload.hasRGB = true;
    upload.rgbFrame = std::move(m_pendingRGBFrame);
    m_pendingRGBFrame = QImage();
    upload.sequence = m_pendingUploadSequence;
    m_rgbFramePending = false;
  }
  return upload;
}

void HDR_RhiVideoWindow::restorePendingUpload(PendingUpload& pendingUpload)
{
  // 只有世代号仍相等、且队列里没有更新的帧时才放回。
  // 生产者在上传期间递交的新帧会先递增世代号，这里不会把它盖掉。
  if (!pendingUpload.hasYUV && !pendingUpload.hasRGB)
    return;

  QMutexLocker pendingLock(&m_pendingYUVMutex);
  if (m_pendingUploadSequence != pendingUpload.sequence)
  {
    pendingUpload.hasYUV = false;
    pendingUpload.hasRGB = false;
    return;
  }

  if (pendingUpload.hasYUV && !m_yuvDataPending && m_pendingYUVData.isEmpty())
  {
    m_pendingYUVData = std::move(pendingUpload.yuvData);
    m_pendingYUVFormat = pendingUpload.yuvFormat;
    m_pendingColorConversion = pendingUpload.colorConversion;
    m_pendingYUVWidth = pendingUpload.yuvWidth;
    m_pendingYUVHeight = pendingUpload.yuvHeight;
    m_yuvDataPending = true;
  }
  if (pendingUpload.hasRGB && !m_rgbFramePending && m_pendingRGBFrame.isNull())
  {
    m_pendingRGBFrame = std::move(pendingUpload.rgbFrame);
    m_rgbFramePending = true;
  }
  pendingUpload.hasYUV = false;
  pendingUpload.hasRGB = false;
}

void HDR_RhiVideoWindow::publishPresentedFrame(const QSize& frameSize,
                                              bool useYUV,
                                              const QImage& rgbFrame)
{
  // 呈现状态只在 GUI 线程的 render() 里发布。生产者只写待上传队列，
  // 因此叠加层读取 m_frameSize / m_currentFrame 时没有数据竞争。
  if (useYUV)
  {
    if (!m_currentFrame.isNull())
      m_currentFrame = QImage();
  }
  else
  {
    m_currentFrame = rgbFrame;
  }

  const bool sizeChanged = (m_frameSize != frameSize);
  m_frameSize = frameSize;
  m_useYUVMode = useYUV;
  if (sizeChanged)
    updateProjectionMatrix();
}

float HDR_RhiVideoWindow::effectiveToneMapNits() const
{
  // 0、负数和 NaN 都表示直通。正数再按当前传递函数钳位：
  // PQ 使用 BT.2390 的 500–10000 nits，HLG 使用 350–1000 nits，Linear 不曝光。
  const float requested = m_toneMapRequestNits;
  if (!(requested > 0.0f))
    return 0.0f;

  if (m_renderMode == Mode_BT2020_PQ_10bit)
  {
    return qBound(video::hdr::ToneMapping::MIN_TARGET_NITS,
                  requested,
                  video::hdr::ToneMapping::MAX_TARGET_NITS);
  }
  if (m_renderMode == Mode_BT2020_HLG_10bit)
  {
    return qBound(video::hdr::constants::DISPLAY_MAX_NITS, requested, 1000.0f);
  }
  return 0.0f;
}

void HDR_RhiVideoWindow::render()
{
  if (!m_initialized || !isExposed())
    return;

  if (!m_rhi || !m_swapChain)
  {
    qWarning() << "[HDR-RHI] render() called but RHI or swap chain is null";
    return;
  }

  if (!m_swapChain->currentPixelSize().isValid())
    return;

  // 先取走待上传帧再画叠加层，放大镜和标尺使用的是即将显示的分辨率。
  PendingUpload pendingUpload = takePendingUpload();
  if (pendingUpload.hasYUV)
    publishPresentedFrame(QSize(pendingUpload.yuvWidth, pendingUpload.yuvHeight), true, QImage());
  else if (pendingUpload.hasRGB)
    publishPresentedFrame(pendingUpload.rgbFrame.size(), false, pendingUpload.rgbFrame);

  if (m_overlayEnabled)
    paintOverlays();

  const QRhi::FrameOpResult result = m_rhi->beginFrame(m_swapChain.get());
  if (result != QRhi::FrameOpSuccess)
  {
    restorePendingUpload(pendingUpload);
    if (result == QRhi::FrameOpSwapChainOutOfDate && !m_swapChain->createOrResize())
      qWarning() << "[HDR-RHI] Failed to resize swap chain";
    return;
  }

  QRhiCommandBuffer* cb = m_swapChain->currentFrameCommandBuffer();
  QRhiRenderTarget* rt = m_swapChain->currentFrameRenderTarget();
  if (!cb || !rt)
  {
    qWarning() << "[HDR-RHI] Command buffer or render target is null";
    restorePendingUpload(pendingUpload);
    m_rhi->endFrame(m_swapChain.get());
    return;
  }

  QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
  cb->beginPass(rt, m_backgroundColor, {1.0f, 0}, batch);
  renderFrame(cb, pendingUpload);

  if (m_overlayEnabled && !m_overlayImage.isNull())
    renderOverlayInPass(cb);

  cb->endPass();
  m_rhi->endFrame(m_swapChain.get());
}

void HDR_RhiVideoWindow::renderFrame(QRhiCommandBuffer* cb, PendingUpload& pendingUpload)
{
  // 待上传数据由 render() 在画叠加层之前取走。这里只消费这份快照，
  // 不再二次加锁读取队列，避免和 publishPresentedFrame 看到的不是同一帧。
  if (!m_initialized)
  {
    restorePendingUpload(pendingUpload);
    return;
  }

  const bool needsYUVUpload = pendingUpload.hasYUV && !pendingUpload.yuvData.isEmpty() &&
                              pendingUpload.sequence >= m_lastUploadedYUVSequence;
  const bool needsRGBUpload = pendingUpload.hasRGB && !pendingUpload.rgbFrame.isNull();
  bool hasUploadedYUVFrame =
      m_useYUVMode && m_lastUploadedYUVSequence != 0 && m_yuvWidth > 0 && m_yuvHeight > 0;

  if (!needsYUVUpload && !needsRGBUpload && !hasUploadedYUVFrame &&
      !(m_videoTexture && !m_currentFrame.isNull()))
    return;

  QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
  if (!batch)
  {
    restorePendingUpload(pendingUpload);
    return;
  }

  if (needsYUVUpload)
  {
    // 使用入队时的宽高，而不是当前 m_frameSize。后者可能已经是下一帧的尺寸。
    if (uploadYUVTextureData(pendingUpload.yuvData,
                             pendingUpload.yuvWidth,
                             pendingUpload.yuvHeight,
                             pendingUpload.yuvFormat,
                             batch))
    {
      m_lastUploadedYUVSequence = pendingUpload.sequence;
      m_currentColorConversion = pendingUpload.colorConversion;
      hasUploadedYUVFrame = true;
      // pendingUpload.yuvData 必须活到下面的 resourceUpdate()：
      // 小端平面格式直接引用这块内存，P010/大端格式引用 m_repackBuffer*。
    }
    else
    {
      restorePendingUpload(pendingUpload);
      if (!hasUploadedYUVFrame)
      {
        cb->resourceUpdate(batch);
        return;
      }
    }
  }
  else if (needsRGBUpload)
  {
    if (!uploadTextureData(pendingUpload.rgbFrame, batch))
    {
      restorePendingUpload(pendingUpload);
      if (!(m_videoTexture && !m_currentFrame.isNull()))
      {
        cb->resourceUpdate(batch);
        return;
      }
    }
  }

  if (m_useYUVMode && hasUploadedYUVFrame)
  {
    // YUV 路径在片元着色器里完成 BT.2020 矩阵和 PQ/HLG/Linear，输出 scRGB 线性光。
    YUVUniformData uniformData;
    std::memcpy(uniformData.projectionMatrix, m_projectionMatrix.constData(), 16 * sizeof(float));
    std::memcpy(uniformData.textureMatrix, m_textureMatrix.constData(), 16 * sizeof(float));

    ensureCachedColorMatrix(m_currentColorConversion);
    std::memcpy(uniformData.colorMatrix, m_cachedColorMatrix, 16 * sizeof(float));
    std::memcpy(uniformData.offsetVec, m_cachedOffsetVec, 4 * sizeof(float));

    // hdrParams.x = 按当前模式钳位后的目标亮度，0 表示关闭映射
    // hdrParams.y = 显示器峰值亮度，Linear 路径不拿它做缩放
    // hdrParams.z = 渲染模式（1=PQ，2=HLG，3=Linear）
    // hdrParams.w = 映射开关（0 关闭，1 开启）
    const float appliedToneMapNits = effectiveToneMapNits();
    const bool exposureEnabled = appliedToneMapNits > 0.0f;
    uniformData.hdrParams[0] = appliedToneMapNits;
    uniformData.hdrParams[1] = m_displayMaxLuminance;
    uniformData.hdrParams[2] = static_cast<float>(m_renderMode);
    uniformData.hdrParams[3] = exposureEnabled ? 1.0f : 0.0f;

    batch->updateDynamicBuffer(m_yuvUniformBuffer.get(), 0, sizeof(YUVUniformData), &uniformData);
    cb->resourceUpdate(batch);
    // resourceUpdate 已经读取完裸指针，这里才能释放快照，并禁止失败路径把空缓冲放回队列。
    pendingUpload.yuvData.clear();
    pendingUpload.hasYUV = false;

    cb->setGraphicsPipeline(m_yuvPipeline.get());
    cb->setShaderResources(m_yuvSrb.get());
  }
  else if (m_videoTexture && !m_currentFrame.isNull())
  {
    UniformData uniformData;
    std::memcpy(uniformData.projectionMatrix, m_projectionMatrix.constData(), 16 * sizeof(float));
    std::memcpy(uniformData.textureMatrix, m_textureMatrix.constData(), 16 * sizeof(float));
    batch->updateDynamicBuffer(m_uniformBuffer.get(), 0, sizeof(UniformData), &uniformData);
    cb->resourceUpdate(batch);
    pendingUpload.rgbFrame = QImage();
    pendingUpload.hasRGB = false;

    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources(m_srb.get());
  }
  else
  {
    cb->resourceUpdate(batch);
    return;
  }

  const QSize outputSize = m_swapChain->currentPixelSize();
  cb->setViewport({ 0, 0, float(outputSize.width()), float(outputSize.height()) });

  const QRhiCommandBuffer::VertexInput vbufBindings[] = {
    { m_vertexBuffer.get(), 0 }
  };
  cb->setVertexInput(0, 1, vbufBindings, m_indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);
  cb->drawIndexed(6);  // 6 indices for 2 triangles
}



bool HDR_RhiVideoWindow::ensureYUVTextures(int width, int height, int uvWidth, int uvHeight)
{
  if (!m_rhi || !m_yuvSrb || !m_yuvUniformBuffer || !m_sampler)
  {
    return false;
  }

  const bool sizeChanged = (m_yuvWidth != width || m_yuvHeight != height);
  const bool uvSizeChanged = (m_uvWidth != uvWidth || m_uvHeight != uvHeight);
  const bool missingTextures = !m_texY || !m_texU || !m_texV;

  if (!sizeChanged && !uvSizeChanged && !missingTextures)
  {
    return true;
  }

  m_texY.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(width, height)));
  m_texU.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(uvWidth, uvHeight)));
  m_texV.reset(m_rhi->newTexture(QRhiTexture::R16, QSize(uvWidth, uvHeight)));

  if (!m_texY->create() || !m_texU->create() || !m_texV->create())
  {
    qWarning() << "[HDR-RHI] Failed to create YUV textures!";
    m_texY.reset();
    m_texU.reset();
    m_texV.reset();
    m_yuvWidth = 0;
    m_yuvHeight = 0;
    m_uvWidth = 0;
    m_uvHeight = 0;
    return false;
  }

  m_yuvSrb->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(
          0,
          QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
          m_yuvUniformBuffer.get()),
      QRhiShaderResourceBinding::sampledTexture(
          1, QRhiShaderResourceBinding::FragmentStage, m_texY.get(), m_sampler.get()),
      QRhiShaderResourceBinding::sampledTexture(
          2, QRhiShaderResourceBinding::FragmentStage, m_texU.get(), m_sampler.get()),
      QRhiShaderResourceBinding::sampledTexture(
          3, QRhiShaderResourceBinding::FragmentStage, m_texV.get(), m_sampler.get())});
  if (!m_yuvSrb->create())
  {
    qWarning() << "[HDR-RHI] Failed to recreate YUV shader resource bindings!";
    return false;
  }

  m_yuvWidth = width;
  m_yuvHeight = height;
  m_uvWidth = uvWidth;
  m_uvHeight = uvHeight;
  return true;
}

void HDR_RhiVideoWindow::updateProjectionMatrix()
{
  // 窗口刚创建或帧尺寸尚未发布时，宽高可能为 0。必须用逻辑或判断，
  // 位或做不到“任意一个 <= 0”：0 | 1920 仍然大于 0，随后 1/0 会把矩阵写成 Inf。
  const int widgetWidth = width();
  const int widgetHeight = height();
  const int frameWidth = m_frameSize.width();
  const int frameHeight = m_frameSize.height();
  if (widgetWidth <= 0 || widgetHeight <= 0 || frameWidth <= 0 || frameHeight <= 0)
  {
    m_projectionMatrix.setToIdentity();
    return;
  }

  const float invWidgetWidth = 1.0f / static_cast<float>(widgetWidth);
  const float invWidgetHeight = 1.0f / static_cast<float>(widgetHeight);
  const float zoomFactor = static_cast<float>(m_parentZoom > 0.0 ? m_parentZoom : 1.0);
  const QPointF& offset = m_parentOffset;
  const float scaleX = static_cast<float>(frameWidth) * zoomFactor * invWidgetWidth;
  const float scaleY = static_cast<float>(frameHeight) * zoomFactor * invWidgetHeight;
  const float offsetX = static_cast<float>(offset.x()) * 2.0f * invWidgetWidth;
  const float offsetY = -static_cast<float>(offset.y()) * 2.0f * invWidgetHeight;

  m_projectionMatrix.setToIdentity();
  m_projectionMatrix.translate(offsetX, offsetY, 0.0f);
  m_projectionMatrix.scale(scaleX, scaleY, 1.0f);
}

void HDR_RhiVideoWindow::setRenderMode(RenderMode mode)
{
  // 这里不改写亮度请求。下一帧 effectiveToneMapNits() 会按新的传递函数重新钳位，
  // 所以从 PQ 的 4000 nits 切到 HLG 时着色器得到 1000，再切回 PQ 时请求仍是 4000。
  if (m_renderMode == mode)
    return;
  m_renderMode = mode;
  if (m_initialized)
    requestUpdate();
}

void HDR_RhiVideoWindow::setToneMapTargetNits(float nits)
{
  // 接口约定：0 表示关闭映射。这里只记录请求，不按当前模式钳位。
  // 钳位放到 effectiveToneMapNits()，这样 PQ 下的 4000 nits 切到 HLG 再切回来仍然是 4000，
  // 而 HLG 着色器当帧拿到的是钳位后的 1000。qFuzzyCompare 不能直接比较 0，所以平移 1。
  const float requested = (nits > 0.0f) ? nits : 0.0f;
  if (qFuzzyCompare(m_toneMapRequestNits + 1.0f, requested + 1.0f))
    return;

  m_toneMapRequestNits = requested;
  if (m_initialized)
    requestUpdate();
}

void HDR_RhiVideoWindow::setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities)
{
  if (capabilities.maxLuminance > 0)
    m_displayMaxLuminance = capabilities.maxLuminance;
}

void HDR_RhiVideoWindow::emitFrameUpdated()
{
  // requestUpdate() 本身可以跨线程调用。frameUpdated 的槽可能直接连在界面对象上，
  // 后台解码线程如果直接 emit，就会在错误的线程进入这些对象。
  if (QThread::currentThread() == thread())
  {
    emit frameUpdated();
    return;
  }

  QMetaObject::invokeMethod(this, [this]() { emit frameUpdated(); }, Qt::QueuedConnection);
}

void HDR_RhiVideoWindow::updateFrame(const QImage& newFrame)
{
  // RGB 与 YUV 共用同一条延迟上传队列，不再在这里 beginOffscreenFrame。
  // 同步离屏提交会在 D3D12/Vulkan 上额外排队并等待 GPU，和下一帧 render() 重复。
  if (newFrame.isNull())
    return;

  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    ++m_pendingUploadSequence;
    m_pendingRGBFrame = newFrame;
    m_rgbFramePending = true;
    m_yuvDataPending = false;
    m_pendingYUVData.clear();
    m_pendingYUVWidth = 0;
    m_pendingYUVHeight = 0;
  }

  requestUpdate();
  emitFrameUpdated();
}

void HDR_RhiVideoWindow::queuePendingYUV(QByteArray&& yuvData,
                                         int width, int height,
                                         const video::yuv::PixelFormatYUV& format,
                                         video::yuv::ColorConversion colorConversion)
{
  // 宽高、格式和字节必须在同一把锁里发布。锁外写 m_frameSize 会和 render() 数据竞争，
  // 也会让上一帧的缓冲套用下一帧的分辨率。呈现用的 m_frameSize 改由 render() 发布。
  if (yuvData.isEmpty() || width <= 0 || height <= 0)
    return;

  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    ++m_pendingUploadSequence;
    m_pendingYUVData = std::move(yuvData);
    m_pendingYUVFormat = format;
    m_pendingColorConversion = colorConversion;
    m_pendingYUVWidth = width;
    m_pendingYUVHeight = height;
    m_yuvDataPending = true;
    m_rgbFramePending = false;
    m_pendingRGBFrame = QImage();
  }

  requestUpdate();
  emitFrameUpdated();
}

void HDR_RhiVideoWindow::updateFrameYUV(const QByteArray& yuvData,
                                        int width, int height,
                                        const video::yuv::PixelFormatYUV& format,
                                        video::yuv::ColorConversion colorConversion)
{
  // Copy path for callers that still need their buffer after this call.
  queuePendingYUV(QByteArray(yuvData), width, height, format, colorConversion);
}

void HDR_RhiVideoWindow::updateFrameYUVMove(QByteArray&& yuvData,
                                            int width, int height,
                                            const video::yuv::PixelFormatYUV& format,
                                            video::yuv::ColorConversion colorConversion)
{
  queuePendingYUV(std::move(yuvData), width, height, format, colorConversion);
}

bool HDR_RhiVideoWindow::uploadTextureData(const QImage& image, QRhiResourceUpdateBatch* batch)
{
  if (!m_rhi || image.isNull())
    return false;

    // For RGBA16F textures, we need to convert QImage to RGBA16FPx4 format.
  // Qt 6 provides Format_RGBA16FPx4 for 16-bit float per channel images.
  // If the source image is 8-bit RGBA, we convert it to float format.
  // SDR values (0-255) are mapped to linear light range (0.0-1.0).
  // Note: The YUV GPU path (updateFrameYUV) is preferred for HDR content.
  QImage uploadImage;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
  // Qt 6.2+ supports Format_RGBA16FPx4 natively
  uploadImage = image.convertToFormat(QImage::Format_RGBA16FPx4);
#else
  // Fallback: use RGBX8888 and let RHI handle conversion
  // Note: This path may have precision loss for HDR content
  uploadImage = image.convertToFormat(QImage::Format_RGBX8888);
  qWarning() << "[HDR-RHI] Qt < 6.2 detected, RGBA16F texture may have limited SDR compatibility";
#endif

  const QSize newSize = uploadImage.size();

         // Recreate texture if size changed
         // Use RGBA16F for HDR linear light output (16-bit float per channel)
  if (!m_videoTexture || m_videoTexture->pixelSize() != newSize) {
    m_videoTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA16F, newSize));
    if (!m_videoTexture->create()) {
      qWarning() << "[HDR-RHI] Failed to create video texture!";
      return false;
    }

           // Update shader resource bindings
    m_srb->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniformBuffer.get()),
      QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_videoTexture.get(), m_sampler.get())
    });
    m_srb->create();
  }

         // Upload texture data (RGBA16FPx4 or RGBX8888 with auto conversion)
  batch->uploadTexture(m_videoTexture.get(), uploadImage);

  return true;
}

bool HDR_RhiVideoWindow::uploadYUVTextureData(const QByteArray& yuvData,
                                              int width, int height,
                                              const video::yuv::PixelFormatYUV& format,
                                              QRhiResourceUpdateBatch* batch)
{
  if (!m_rhi || !batch)
    return false;

  // Only support 10-bit planar or semi-planar YUV for the GPU HDR path.
  // Semi-planar P010-style input is deinterleaved to the existing U/V shader layout.
  if (format.getBitsPerSample() != 10 || !format.isPlanar()) {
    qWarning() << "[HDR-RHI] YUV GPU path only supports 10-bit planar or semi-planar formats for HDR10";
    return false;
  }

  const int subH = format.getSubsamplingHor();
  const int subV = format.getSubsamplingVer();
  const int yPlaneSize = width * height;
  // 色度平面用向下取整，与 PixelFormatYUV::bytesPerFrame 的 width/2、width/4 一致。
  // 向上取整会和缓存里实际打包的字节数错位。subH/subV 只可能是 1、2、4。
  const int uvWidth = (subH > 1) ? (width / subH) : width;
  const int uvHeight = (subV > 1) ? (height / subV) : height;
  const int uvPlaneSize = uvWidth * uvHeight;

  const int yPlaneBytes = yPlaneSize << 1;
  const int uvPlaneBytes = uvPlaneSize << 1;
  const int totalBytes = yPlaneBytes + (uvPlaneBytes << 1);

  if (yuvData.size() < totalBytes) {
    qWarning() << "[HDR-RHI] YUV data buffer too small";
    return false;
  }

  if (!ensureYUVTextures(width, height, uvWidth, uvHeight))
    return false;

  const auto* rawWords = reinterpret_cast<const uint16_t*>(yuvData.constData());
  const bool uvInterleaved = format.isUVInterleaved();
  const bool uFirst = (format.getPlaneOrder() == video::yuv::PlaneOrder::YUV ||
                       format.getPlaneOrder() == video::yuv::PlaneOrder::YUVA);
  const bool normalizeP010 = uvInterleaved && format.getBitsPerSample() == 10;
  const bool bigEndian = format.isBigEndian();
  const bool needsRepacking = bigEndian || uvInterleaved;

  const uint16_t* HDR_RESTRICT srcY = rawWords;
  const uint16_t* HDR_RESTRICT srcU = nullptr;
  const uint16_t* HDR_RESTRICT srcV = nullptr;

  if (needsRepacking) {
    // Retain capacity across frames to avoid per-frame heap churn.
    if (m_repackBufferY.size() < yPlaneBytes)
      m_repackBufferY.resize(yPlaneBytes);
    if (m_repackBufferU.size() < uvPlaneBytes)
      m_repackBufferU.resize(uvPlaneBytes);
    if (m_repackBufferV.size() < uvPlaneBytes)
      m_repackBufferV.resize(uvPlaneBytes);

    auto* repackY = reinterpret_cast<uint16_t*>(m_repackBufferY.data());
    auto* repackU = reinterpret_cast<uint16_t*>(m_repackBufferU.data());
    auto* repackV = reinterpret_cast<uint16_t*>(m_repackBufferV.data());

    // Y plane: BE swap and/or P010 >>6 via SSE2 when available.
    video::hdr::HDRYUVRepack::convertPlane16(
        rawWords, repackY, yPlaneSize, bigEndian, normalizeP010);

    if (uvInterleaved) {
      video::hdr::HDRYUVRepack::deinterleaveUV(rawWords + yPlaneSize,
                                               repackU,
                                               repackV,
                                               uvPlaneSize,
                                               bigEndian,
                                               normalizeP010,
                                               uFirst);
    } else {
      const uint16_t* firstPlane = rawWords + yPlaneSize;
      const uint16_t* secondPlane = firstPlane + uvPlaneSize;
      video::hdr::HDRYUVRepack::convertPlane16(
          firstPlane, uFirst ? repackU : repackV, uvPlaneSize, bigEndian, false);
      video::hdr::HDRYUVRepack::convertPlane16(
          secondPlane, uFirst ? repackV : repackU, uvPlaneSize, bigEndian, false);
    }

    srcY = repackY;
    srcU = repackU;
    srcV = repackV;
  } else {
    // Keep staging capacity for the next format that needs it, but do not clear
    // every LE planar frame (would thrash the allocator on format toggles).
    const uint16_t* planeAfterY = srcY + yPlaneSize;
    srcU = uFirst ? planeAfterY : (planeAfterY + uvPlaneSize);
    srcV = uFirst ? (planeAfterY + uvPlaneSize) : planeAfterY;
  }

  QRhiTextureSubresourceUploadDescription yDesc(srcY, yPlaneBytes);
  batch->uploadTexture(m_texY.get(), QRhiTextureUploadDescription({{0, 0, yDesc}}));

  QRhiTextureSubresourceUploadDescription uDesc(srcU, uvPlaneBytes);
  batch->uploadTexture(m_texU.get(), QRhiTextureUploadDescription({{0, 0, uDesc}}));

  QRhiTextureSubresourceUploadDescription vDesc(srcV, uvPlaneBytes);
  batch->uploadTexture(m_texV.get(), QRhiTextureUploadDescription({{0, 0, vDesc}}));

  return true;
}

void HDR_RhiVideoWindow::clearFrame()
{
  // 递增世代号，使正在上传、准备放回队列的那一帧失效，避免 clear 之后被放回。
  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    ++m_pendingUploadSequence;
    m_yuvDataPending = false;
    m_pendingYUVData.clear();
    m_pendingYUVWidth = 0;
    m_pendingYUVHeight = 0;
    m_rgbFramePending = false;
    m_pendingRGBFrame = QImage();
    m_repackBufferY.clear();
    m_repackBufferU.clear();
    m_repackBufferV.clear();
  }
  m_currentFrame = QImage();
  m_overlayPatchProvider = nullptr;
  m_overlayYuvPixelProvider = nullptr;
  m_frameSize = QSize();
  m_useYUVMode = false;
  m_colorMatrixCacheValid = false;
  updateProjectionMatrix();
  requestUpdate();
}

void HDR_RhiVideoWindow::updateTransform(double renderZoom, const QPointF& offset, double displayZoom)
{
  const double actualDisplayZoom = (displayZoom < 0.0) ? renderZoom : displayZoom;

  if (m_parentZoom != renderZoom || m_parentOffset != offset || m_displayZoom != actualDisplayZoom) {
    m_parentZoom = renderZoom;
    m_displayZoom = actualDisplayZoom;
    m_parentOffset = offset;
    updateProjectionMatrix();
    requestUpdate();
  }
}


// Overlay painting implementation is in HDR_RhiVideoWindow_Overlays.cpp
