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
  m_fallbackSurface.reset();
  m_rhi.reset();

  m_initialized = false;
}

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

#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
  QRhiVulkanInitParams vulkanParams;
  vulkanParams.inst = nullptr;
  m_rhi.reset(QRhi::create(QRhi::Vulkan, &vulkanParams));

#elif defined(Q_OS_MACOS)
  QRhiMetalInitParams metalParams;
  m_rhi.reset(QRhi::create(QRhi::Metal, &metalParams));
#endif

         // Fallback to OpenGL if platform-specific backend fails
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
  if (m_initialized) {
    return true;
  }
  if (m_initializationFailed) {
    return false;
  }
  if (!isExposed()) {
    if (!m_loggedNotExposedState) {
      DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): window not exposed yet");
      m_loggedNotExposedState = true;
    }
    return false;
  }

  m_loggedNotExposedState = false;

  // Ensure only one caller performs initialization work at a time.
  bool expected = false;
  if (!m_initializationInProgress.compare_exchange_strong(expected, true)) {
    DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): initialization already in progress");
    return m_initialized;
  }

  QMutexLocker initLock(&m_initMutex);

  if (m_initialized) {
    m_initializationInProgress.store(false);
    return true;
  }
  if (m_initializationFailed) {
    m_initializationInProgress.store(false);
    return false;
  }

  DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): starting RHI initialization");

  // Initialize RHI, swap chain, and pipelines.
  const bool success = initializeRhi() && initializeSwapChain() &&
                       initializePipeline() && initializeYUVPipeline();

  if (success) {
    m_initialized = true;

    // Upload initial vertex and index data.
    if (m_rhi && m_vertexBuffer && m_indexBuffer) {
      QRhiCommandBuffer* cb = nullptr;
      if (m_rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess) {
        QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
        if (batch) {
          batch->uploadStaticBuffer(m_vertexBuffer.get(), kQuadVertices);
          batch->uploadStaticBuffer(m_indexBuffer.get(), kQuadIndices);
          cb->resourceUpdate(batch);
        }
        m_rhi->endOffscreenFrame();
      }
    }

    DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): success");
    m_initializationInProgress.store(false);
    emit widgetInitialized();
    return true;
  }

  DEBUG_HDR_WINDOW_LOG("[HDR-INIT] tryInitialize(): failed");
  m_initializationFailed = true;
  m_initializationInProgress.store(false);
  emit hdrNotSupported("HDR rendering initialization failed");
  return false;
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

void HDR_RhiVideoWindow::render()
{
  if (!m_initialized || !isExposed())
    return;

         // Safety checks for required resources
  if (!m_rhi || !m_swapChain) {
    qWarning() << "[HDR-RHI] render() called but RHI or swap chain is null";
    return;
  }

  if (!m_swapChain->currentPixelSize().isValid())
    return;

         // Prepare overlay image before starting frame (uses QPainter)
  if (m_overlayEnabled) {
    paintOverlays();
  }

  QRhi::FrameOpResult result = m_rhi->beginFrame(m_swapChain.get());
  if (result == QRhi::FrameOpSwapChainOutOfDate) {
    if (!m_swapChain->createOrResize()) {
      qWarning() << "[HDR-RHI] Failed to resize swap chain";
    }
    return;
  }
  if (result != QRhi::FrameOpSuccess) {
    return;
  }

  QRhiCommandBuffer* cb = m_swapChain->currentFrameCommandBuffer();
  QRhiRenderTarget* rt = m_swapChain->currentFrameRenderTarget();

  if (!cb || !rt) {
    qWarning() << "[HDR-RHI] Command buffer or render target is null";
    m_rhi->endFrame(m_swapChain.get());
    return;
  }

  QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();

         // Clear color for the HDR pass (linear-scRGB output path)
  const QColor clearColor = m_backgroundColor;

  cb->beginPass(rt, clearColor, {1.0f, 0}, batch);

         // Render video frame if available
  renderFrame(cb);

         // Render overlay in the same pass (alpha-blended on top of video)
  if (m_overlayEnabled && !m_overlayImage.isNull()) {
    renderOverlayInPass(cb);
  }

  cb->endPass();

  m_rhi->endFrame(m_swapChain.get());
}

void HDR_RhiVideoWindow::renderFrame(QRhiCommandBuffer* cb)
{
  if (!m_initialized)
    return;

  // Take ownership of pending YUV via swap/move — no full-frame memcpy.
  // A newer producer frame that arrives while we upload keeps its own sequence
  // and remains pending for the next render tick.
  QByteArray pendingYUVDataLocal;
  video::yuv::PixelFormatYUV pendingYUVFormatLocal;
  video::yuv::ColorConversion pendingColorConversionLocal = m_currentColorConversion;
  uint64_t pendingSequenceLocal = 0;
  bool tookPendingFrame = false;
  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    if (m_useYUVMode && m_yuvDataPending && !m_pendingYUVData.isEmpty()) {
      pendingYUVDataLocal = std::move(m_pendingYUVData);
      m_pendingYUVData.clear();
      pendingYUVFormatLocal = m_pendingYUVFormat;
      pendingColorConversionLocal = m_pendingColorConversion;
      pendingSequenceLocal = m_pendingYUVSequence;
      m_yuvDataPending = false;
      tookPendingFrame = true;
    }
  }

  const bool needsYUVUpload =
      tookPendingFrame && !pendingYUVDataLocal.isEmpty() &&
      pendingSequenceLocal >= m_lastUploadedYUVSequence;
  bool hasUploadedYUVFrame =
      m_useYUVMode && m_lastUploadedYUVSequence != 0 && m_yuvWidth > 0 && m_yuvHeight > 0;
  const bool canRenderRGB = m_videoTexture && !m_currentFrame.isNull();

  if (!needsYUVUpload && !hasUploadedYUVFrame && !canRenderRGB)
    return;

  QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
  if (!batch)
    return;

  if (needsYUVUpload) {
    if (uploadYUVTextureData(pendingYUVDataLocal,
                             m_frameSize.width(),
                             m_frameSize.height(),
                             pendingYUVFormatLocal,
                             batch)) {
      m_lastUploadedYUVSequence = pendingSequenceLocal;
      m_currentColorConversion = pendingColorConversionLocal;
      hasUploadedYUVFrame = true;
      // Keep pendingYUVDataLocal alive until cb->resourceUpdate(batch) below:
      // LE planar uploads reference this buffer directly; P010/BE uses m_repackBuffer*.
    } else {
      // Upload failed: put the frame back if no newer pending frame arrived.
      {
        QMutexLocker pendingLock(&m_pendingYUVMutex);
        if (m_pendingYUVSequence == pendingSequenceLocal && m_pendingYUVData.isEmpty()) {
          m_pendingYUVData = std::move(pendingYUVDataLocal);
          m_pendingYUVFormat = pendingYUVFormatLocal;
          m_pendingColorConversion = pendingColorConversionLocal;
          m_yuvDataPending = true;
        }
      }
      if (!hasUploadedYUVFrame) {
        cb->resourceUpdate(batch);
        return;
      }
    }
  }

         // Choose pipeline based on mode
  if (m_useYUVMode && hasUploadedYUVFrame) {
    // YUV rendering path - GPU-based BT.2020 YUV->RGB conversion
    // Output is RGBA16F linear-scRGB from unified HDR shader path

           // Update uniform buffer with matrices and color conversion
    YUVUniformData uniformData;
    std::memcpy(uniformData.projectionMatrix, m_projectionMatrix.constData(), 16 * sizeof(float));
    std::memcpy(uniformData.textureMatrix, m_textureMatrix.constData(), 16 * sizeof(float));

    ensureCachedColorMatrix(m_currentColorConversion);
    std::memcpy(uniformData.colorMatrix, m_cachedColorMatrix, 16 * sizeof(float));
    std::memcpy(uniformData.offsetVec, m_cachedOffsetVec, 4 * sizeof(float));

           // hdrParams.x = target exposure luminance in nits
           // hdrParams.y = display max luminance (nits) for Linear mode scRGB scaling
           // hdrParams.z = render mode (1=PQ, 2=HLG, 3=Linear)
           // hdrParams.w = exposure enabled flag (0=disabled, 1=enabled)
    const auto renderMode = static_cast<video::hdr::RenderMode>(m_renderMode);
    // Keep exposure activation aligned with the shader contract:
    // PQ uses the BT.2390 tone-mapping range, HLG uses the 350-1000 nit exposure range,
    // and Linear mode always bypasses exposure scaling.
    const bool isPQExposureActive =
        renderMode == video::hdr::RenderMode::PQ &&
        m_toneMapTargetNits >= video::hdr::ToneMapping::MIN_TARGET_NITS &&
        m_toneMapTargetNits <= video::hdr::ToneMapping::MAX_TARGET_NITS;
    const bool isHLGExposureActive =
        renderMode == video::hdr::RenderMode::HLG &&
        m_toneMapTargetNits >= video::hdr::constants::DISPLAY_MAX_NITS &&
        m_toneMapTargetNits <= 1000.0f;
    const bool exposureEnabled = isPQExposureActive || isHLGExposureActive;

    uniformData.hdrParams[0] = exposureEnabled ? m_toneMapTargetNits : 0.0f;
    // Linear mode scales BT.2020 linear light into scRGB using display peak nits.
    uniformData.hdrParams[1] = m_displayMaxLuminance;
    uniformData.hdrParams[2] = static_cast<float>(m_renderMode);
    uniformData.hdrParams[3] = exposureEnabled ? 1.0f : 0.0f;

    batch->updateDynamicBuffer(m_yuvUniformBuffer.get(), 0, sizeof(YUVUniformData), &uniformData);
    cb->resourceUpdate(batch);
    // Staging pointers were consumed by resourceUpdate; release the moved frame early.
    pendingYUVDataLocal.clear();

    cb->setGraphicsPipeline(m_yuvPipeline.get());
    cb->setShaderResources(m_yuvSrb.get());

  } else if (canRenderRGB) {
    // RGB rendering path
    UniformData uniformData;
    std::memcpy(uniformData.projectionMatrix, m_projectionMatrix.constData(), 16 * sizeof(float));
    std::memcpy(uniformData.textureMatrix, m_textureMatrix.constData(), 16 * sizeof(float));
    batch->updateDynamicBuffer(m_uniformBuffer.get(), 0, sizeof(UniformData), &uniformData);
    cb->resourceUpdate(batch);

    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources(m_srb.get());
  } else {
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
  const int widgetWidth = width();
  const int widgetHeight = height();
  const int frameWidth = m_frameSize.width();
  const int frameHeight = m_frameSize.height();

         // Early exit with single branch - likely to be predicted correctly
  if ((widgetWidth | widgetHeight | frameWidth | frameHeight) <= 0) {
    m_projectionMatrix.setToIdentity();
    return;
  }

         // Use reciprocals to convert divisions to multiplications
         // Division is typically 10-20x slower than multiplication on modern CPUs
  const float invWidgetWidth = 1.0f / static_cast<float>(widgetWidth);
  const float invWidgetHeight = 1.0f / static_cast<float>(widgetHeight);

         // Branchless zoom factor with max() pattern
  const float zoomFactor = static_cast<float>(m_parentZoom > 0.0 ? m_parentZoom : 1.0);
  const QPointF& offset = m_parentOffset;

         // Combined scaling computation using pre-computed reciprocals
  const float scaleX = static_cast<float>(frameWidth) * zoomFactor * invWidgetWidth;
  const float scaleY = static_cast<float>(frameHeight) * zoomFactor * invWidgetHeight;

  // Offset computation using multiplication instead of division
  // Factor of 2.0 is constant and can be folded with reciprocal
  const float offsetX = static_cast<float>(offset.x()) * 2.0f * invWidgetWidth;
  const float offsetY = -static_cast<float>(offset.y()) * 2.0f * invWidgetHeight;

  m_projectionMatrix.setToIdentity();
  m_projectionMatrix.translate(offsetX, offsetY, 0.0f);
  m_projectionMatrix.scale(scaleX, scaleY, 1.0f);
}

void HDR_RhiVideoWindow::setRenderMode(RenderMode mode)
{
  if (m_renderMode == mode)
    return;
  m_renderMode = mode;
  if (m_initialized)
    requestUpdate();
}

void HDR_RhiVideoWindow::setToneMapTargetNits(float nits)
{
  constexpr float kMinHLGExposureNits = video::hdr::constants::DISPLAY_MAX_NITS;
  constexpr float kMaxHLGExposureNits = 1000.0f;

  float clampedNits = nits;
  if (m_renderMode == Mode_BT2020_PQ_10bit) {
    clampedNits = qBound(video::hdr::ToneMapping::MIN_TARGET_NITS,
                         nits,
                         video::hdr::ToneMapping::MAX_TARGET_NITS);
  } else if (m_renderMode == Mode_BT2020_HLG_10bit) {
    clampedNits = qBound(kMinHLGExposureNits,
                         nits,
                         kMaxHLGExposureNits);
  }

  if (qFuzzyCompare(m_toneMapTargetNits, clampedNits)) return;
  m_toneMapTargetNits = clampedNits;

  if (m_initialized) {
    requestUpdate();
  }
}

void HDR_RhiVideoWindow::setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities)
{
  if (capabilities.maxLuminance > 0)
    m_displayMaxLuminance = capabilities.maxLuminance;
}

void HDR_RhiVideoWindow::updateFrame(const QImage& newFrame)
{
  if (newFrame.isNull()) return;

  m_currentFrame = newFrame;
  const QSize newSize = newFrame.size();
  const bool sizeChanged = (m_frameSize != newSize);
  m_frameSize = newSize;
  m_useYUVMode = false;

  if (sizeChanged) {
    updateProjectionMatrix();
  }

  if (m_initialized && isExposed() && m_rhi) {
           // Submit the upload only after the offscreen frame is available.
    QRhiCommandBuffer* cb = nullptr;
    if (m_rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess) {
      QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
      if (batch && uploadTextureData(newFrame, batch)) {
        cb->resourceUpdate(batch);
      }
      m_rhi->endOffscreenFrame();
    }
  }

  requestUpdate();
  emit frameUpdated();
}

void HDR_RhiVideoWindow::queuePendingYUV(QByteArray&& yuvData,
                                         int width, int height,
                                         const video::yuv::PixelFormatYUV& format,
                                         video::yuv::ColorConversion colorConversion)
{
  if (yuvData.isEmpty() || width <= 0 || height <= 0)
    return;

  m_frameSize = QSize(width, height);
  m_useYUVMode = true;
  m_currentColorConversion = colorConversion;
  updateProjectionMatrix();

  // Deferred upload in renderFrame() avoids beginOffscreenFrame stalls.
  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    ++m_pendingYUVSequence;
    m_pendingYUVData = std::move(yuvData);
    m_pendingYUVFormat = format;
    m_pendingColorConversion = colorConversion;
    m_yuvDataPending = true;
  }

  requestUpdate();
  emit frameUpdated();
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
  const int uvWidth = (subH == 2) ? (width >> 1) : ((subH == 4) ? (width >> 2) : width);
  const int uvHeight = (subV == 2) ? (height >> 1) : ((subV == 4) ? (height >> 2) : height);
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
  m_currentFrame = QImage();
  m_overlayPatchProvider = nullptr;
  m_overlayYuvPixelProvider = nullptr;
  m_frameSize = QSize();
  m_useYUVMode = false;
  m_colorMatrixCacheValid = false;
  {
    QMutexLocker pendingLock(&m_pendingYUVMutex);
    m_yuvDataPending = false;
    m_pendingYUVData.clear();
    m_repackBufferY.clear();
    m_repackBufferU.clear();
    m_repackBufferV.clear();
  }
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
