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

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QTextDocument>
#include <QRegion>
#include <QDir>
#include <QSettings>
#include <qfloat16.h>
#include <array>
#include <cmath>
#include <cstring>
#include <video/yuv/YUVPixelRenderer.h>

// Force inline macro for overlay performance-critical functions
#if defined(_MSC_VER)
#define OVERLAY_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define OVERLAY_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define OVERLAY_FORCE_INLINE inline
#endif

// Helper: Fast integer division by 2 using arithmetic right shift
#define FAST_DIV2(x) ((x) >> 1)
namespace
{
/**
 * @brief Format one YUV component using decimal or hexadecimal notation.
 *
 * @param value Component value.
 * @param formatBase Numeric base (10 or 16).
 * @return Formatted uppercase string.
 */
QString formatComponentValue(int value, int formatBase)
{
  if (formatBase == 16) {
    return QString::number(value, 16).toUpper();
  }
  return QString::number(value);
}

/**
 * @brief Build compact multi-line YUV text that matches SDR readability.
 *
 * @param value YUV component triplet.
 * @param formatBase Numeric base (10 or 16).
 * @return Multi-line string with one component per line.
 */
QString buildVerticalYuvText(const video::yuv::yuv_t& value, int formatBase)
{
  return QStringLiteral("Y%1\nU%2\nV%3")
  .arg(formatComponentValue(value.Y, formatBase),
       formatComponentValue(value.U, formatBase),
       formatComponentValue(value.V, formatBase));
}
} // namespace


/**
 * @brief Paint overlays on top of the rendered frame
 * 
 * Since QRhi doesn't directly support QPainter, we use a backing store approach:
 * 1. Create an overlay image with alpha channel
 * 2. Paint UI elements (grid, zoom box, rulers, etc.) using QPainter
 * 3. The overlay would be blended with the video frame
 * 
 * Note: For simplicity, this implementation creates an off-screen image and
 * draws it using the window's paint device. A more sophisticated approach
 * would use a separate RHI texture for the overlay.
 */
void HDR_RhiVideoWindow::paintOverlays()
{
  // Early exit with combined condition check
  if (!m_overlayEnabled || width() <= 0 || height() <= 0) return;

         // Create or resize overlay image if needed
  const QSize windowSize = size();
  if (m_overlayImage.isNull() || m_overlayImage.size() != windowSize) {
    m_overlayImage = QImage(windowSize, QImage::Format_ARGB32_Premultiplied);
  }
  
  // Clear overlay with transparent background
  m_overlayImage.fill(Qt::transparent);
  
  QPainter painter(&m_overlayImage);
  if (!painter.isActive()) {
    return;
  }
  
  // Disable expensive render hints for overlay performance
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform, false);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::NoBrush);
  painter.setTransform(QTransform());

  const int viewWidth = width();
  const int viewHeight = height();
  const QPoint drawAreaBotR(viewWidth, viewHeight);
  
  // Use bit shift for division by 2 - faster than floating point multiply
  const int xSplit = static_cast<int>(viewWidth * m_overlaySplitPoint);

         // Determine centers using bit shifts for division by 2
  std::array<QPoint, 2> centers;
  const bool isSplitComparisonOrDisabled = (m_overlaySplitMode == OverlaySplitComparison || 
                                            m_overlaySplitMode == OverlaySplitDisabled);
  
  // Branchless center calculation using conditional selection
  if (isSplitComparisonOrDisabled) {
    centers[0] = QPoint(FAST_DIV2(viewWidth), FAST_DIV2(viewHeight));
    centers[1] = centers[0];
  } else {
    const int y = FAST_DIV2(viewHeight);
    centers[0] = QPoint(FAST_DIV2(xSplit), y);
    centers[1] = QPoint(xSplit + FAST_DIV2(viewWidth - xSplit), y);
  }

         // Branchless zoom factor with max pattern
  const double zoom = (m_parentZoom > 0.0) ? m_parentZoom : 1.0;
  const QPointF& offset = m_parentOffset;

         // Compute pixel-in-item flags and zoom rectangles
  std::array<bool, 2> pixelPosInItem = {false, false};
  std::array<QRect, 2> zoomPixelRect;
  if (m_zoomBoxEnabled && !m_frameSize.isEmpty()) {
    const double itemW = static_cast<double>(m_frameSize.width());
    const double itemH = static_cast<double>(m_frameSize.height());
    const int viewNum = (m_overlaySplitMode == OverlaySplitSideBySide) ? 2 : 1;
    for (int v = 0; v < viewNum; ++v) {
      const QPoint& pix = m_zoomBoxPixelPos[v];
      pixelPosInItem[v] = (pix.x() >= 0 && pix.x() < itemW && pix.y() >= 0 && pix.y() < itemH);
      if (pixelPosInItem[v]) {
        const int px = -static_cast<int>((itemW * 0.5 - static_cast<double>(pix.x())) * zoom);
        const int py = -static_cast<int>((itemH * 0.5 - static_cast<double>(pix.y())) * zoom);
        zoomPixelRect[v] = QRect(px, py, static_cast<int>(zoom), static_cast<int>(zoom));
      }
    }
  }

         // Per-view decorations
  auto paintViewDecorations = [&](int viewIndex) {
    // Clip region for split views
    if (m_overlaySplitMode == OverlaySplitSideBySide) {
      if (viewIndex == 0) {
        painter.setClipRegion(QRegion(0, 0, xSplit, drawAreaBotR.y()));
      } else {
        painter.setClipRegion(QRegion(xSplit, 0, drawAreaBotR.x() - xSplit, drawAreaBotR.y()));
      }
    }

           // Translate to item center + pan offset
    painter.translate(centers[viewIndex] + offset);

           // Grid
    if (m_gridSize > 0) {
      paintGrid(painter, centers[viewIndex], offset, viewIndex == 0 ? 0 : xSplit, viewIndex == 0 ? xSplit : drawAreaBotR.x());
    }

           // Pixel under cursor rectangle
    if (m_zoomBoxEnabled && pixelPosInItem[viewIndex]) {
      const QPoint pixelPt = m_zoomBoxPixelPos[viewIndex];
      painter.setPen(isPixelDarkAt(pixelPt) ? Qt::white : Qt::black);
      painter.drawRect(zoomPixelRect[viewIndex]);
    }

           // Reset transform for UI panels
    painter.resetTransform();

           // Zoom box
    if (m_zoomBoxEnabled) {
      paintZoomBox(painter, viewIndex, centers[viewIndex], drawAreaBotR, xSplit, 0);
    }

           // Pixel rulers
    int pxMin = 0;
    int pxMax = drawAreaBotR.x();
    int rulerXPos = 0;
    if (m_overlaySplitMode == OverlaySplitSideBySide) {
      if (viewIndex == 0) {
        pxMin = 0; pxMax = xSplit; rulerXPos = 0;
      } else {
        pxMin = xSplit; pxMax = drawAreaBotR.x(); rulerXPos = xSplit;
      }
    } else {
      pxMin = 0; pxMax = drawAreaBotR.x(); rulerXPos = 0;
    }
    paintPixelRulersX(painter, centers[viewIndex], offset, pxMin, pxMax, zoom);
    paintPixelRulersY(painter, centers[viewIndex], offset, drawAreaBotR.y(), rulerXPos, zoom);

    if (m_drawRawValues && zoom >= 128.0) {
      paintRawPixelValues(painter, centers[viewIndex], offset, pxMin, pxMax, zoom);
    }

           // Loading message
    if (!m_playing) {
      if ((viewIndex == 0 && m_loadingLeft) || (viewIndex == 1 && m_loadingRight)) {
        const QPoint center = (viewIndex == 0)
        ? QPoint(xSplit / 2, drawAreaBotR.y() / 2)
        : QPoint(xSplit + (drawAreaBotR.x() - xSplit) / 2, drawAreaBotR.y() / 2);
        drawLoadingMessage(painter, center);
      }
    }

           // Item path/name at top
    if (m_drawPathAndName) {
      if (viewIndex == 0 && !m_itemPathLeft.isEmpty()) {
        drawItemPathAndName(painter, 0, xSplit, m_itemPathLeft);
      } else if (viewIndex == 1 && !m_itemPathRight.isEmpty()) {
        drawItemPathAndName(painter, xSplit, drawAreaBotR.x() - xSplit, m_itemPathRight);
      }
    }

           // Disable clipping after view
    painter.setClipping(false);
  };

  if (m_overlaySplitMode == OverlaySplitSideBySide) {
    paintViewDecorations(0);
    paintViewDecorations(1);
  } else {
    paintViewDecorations(0);
  }

         // Split line
  if (m_overlaySplitMode == OverlaySplitSideBySide) {
    QLine line(xSplit, 0, xSplit, drawAreaBotR.y());
    painter.setPen(Qt::white);
    painter.drawLine(line);
  }

         // Zoom factor text - display as "1X HDR" format with power-of-2 multipliers
  {
    const double displayZoom = (m_displayZoom <= 0.0 ? 1.0 : m_displayZoom);
    
    // Format zoom value: prefer integer display for clean power-of-2 values
    // Avoid scientific notation by using fixed-point format
    QString zoomValue;
    if (displayZoom >= 1.0) {
      // For zoom >= 1, show as integer if it's a whole number
      if (displayZoom == std::floor(displayZoom)) {
        zoomValue = QString::number(static_cast<int>(displayZoom));
      } else {
        // Show with minimal decimal places
        zoomValue = QString::number(displayZoom, 'f', 1);
        // Remove trailing zeros
        while (zoomValue.endsWith(QLatin1Char('0')))
          zoomValue.chop(1);
        if (zoomValue.endsWith(QLatin1Char('.')))
          zoomValue.chop(1);
      }
    } else {
      // For zoom < 1, show as fraction (e.g., 0.5, 0.25)
      zoomValue = QString::number(displayZoom, 'f', 2);
      // Remove trailing zeros
      while (zoomValue.endsWith(QLatin1Char('0')))
        zoomValue.chop(1);
      if (zoomValue.endsWith(QLatin1Char('.')))
        zoomValue.chop(1);
    }
    
    // Build zoom string: "1X HDR" format
    QString zoomString = zoomValue + QStringLiteral("X");
    
    // Append HDR mode tag
    if (m_renderMode == Mode_BT2020_PQ_10bit) {
      zoomString += QStringLiteral(" HDR");
    } else if (m_renderMode == Mode_BT2020_HLG_10bit) {
      zoomString += QStringLiteral(" HLG");
    }
    
    // Draw zoom text with outline effect for seamless integration with video
    painter.save();
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    
    QFont zoomFont = m_zoomFactorFont;
    zoomFont.setKerning(false);
    zoomFont.setBold(true);
    painter.setFont(zoomFont);
    QFontMetrics fm(zoomFont);
    
    const qreal dpr = devicePixelRatio();
    auto snap = [&](qreal v) -> qreal { return qreal(qRound(v * dpr)) / dpr; };
    
    const int marginX = 10;
    const int marginY = 10;
    QPointF textPos(snap(marginX), snap(qreal(marginY + fm.ascent())));
    
    // Draw text outline (black stroke) for visibility on any background
    // Use path-based outline for smooth edges
    QPainterPath textPath;
    textPath.addText(textPos, zoomFont, zoomString);
    
    // Draw black outline stroke
    painter.setPen(QPen(QColor(0, 0, 0, 255), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(textPath);
    
    // Fill with high-contrast white text to improve readability in HDR highlights.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 255));
    painter.drawPath(textPath);
    
    painter.restore();
  }

         // Caching indicator
  if (m_waitingForCaching && !m_cachingPixmap.isNull()) {
    QPoint pos = QPoint(10, drawAreaBotR.y() - 10 - m_cachingPixmap.height());
    painter.drawPixmap(pos, m_cachingPixmap);
  }
  
  painter.end();
  
  // Overlay image is now ready; it will be rendered by renderOverlayPass() in render()
}

bool HDR_RhiVideoWindow::initializeOverlayPipeline()
{
  if (!m_rhi || m_overlayResourcesInitialized)
    return m_overlayResourcesInitialized;

  // Create overlay uniform buffer (just for projection matrix)
  m_overlayUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic,
                                                QRhiBuffer::UniformBuffer,
                                                sizeof(UniformData)));
  if (!m_overlayUniformBuffer->create())
    return false;

  // Create overlay texture (will be resized as needed)
  m_overlayTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA16F, QSize(64, 64)));
  if (!m_overlayTexture->create())
    return false;

  // Create sampler for overlay texture
  m_overlaySampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest,
                                           QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
  if (!m_overlaySampler->create())
    return false;

  // Load shaders for overlay - using simple RGBA texture shaders
  // We need to declare this function from HDR_RhiVideoWindow.cpp
  extern QShader loadOrCompileShader(const QString& qsbPath, const QString& sourcePath, QShader::Stage stage);
  
  QShader overlayVs = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_vertex.vert.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_vertex.vert"),
    QShader::VertexStage);
  QShader overlayFs = loadOrCompileShader(
    QStringLiteral(":/shaders/hdr_rhi_fragment.frag.qsb"),
    QStringLiteral(":/shaders/hdr_rhi_fragment.frag"),
    QShader::FragmentStage);

  if (!overlayVs.isValid() || !overlayFs.isValid()) {
    qWarning() << "[HDR-RHI] Failed to load overlay shaders";
    return false;
  }
  
  // Create shader resource bindings for overlay
  m_overlaySrb.reset(m_rhi->newShaderResourceBindings());
  m_overlaySrb->setBindings({
    QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_overlayUniformBuffer.get()),
    QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_overlayTexture.get(), m_overlaySampler.get())
  });
  if (!m_overlaySrb->create())
    return false;

  // Create overlay graphics pipeline with alpha blending
  m_overlayPipeline.reset(m_rhi->newGraphicsPipeline());
  
  // Enable alpha blending for overlay
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
  blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  blend.srcAlpha = QRhiGraphicsPipeline::One;
  blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
  m_overlayPipeline->setTargetBlends({blend});
  
  m_overlayPipeline->setShaderStages({
    { QRhiShaderStage::Vertex, overlayVs },
    { QRhiShaderStage::Fragment, overlayFs }
  });
  
  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings({
    { 4 * sizeof(float) }  // position + texcoord
  });
  inputLayout.setAttributes({
    { 0, 0, QRhiVertexInputAttribute::Float2, 0 },              // position
    { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) } // texcoord
  });
  m_overlayPipeline->setVertexInputLayout(inputLayout);
  
  m_overlayPipeline->setShaderResourceBindings(m_overlaySrb.get());
  m_overlayPipeline->setRenderPassDescriptor(m_renderPassDesc.get());
  
  if (!m_overlayPipeline->create())
    return false;

  m_overlayResourcesInitialized = true;
  return true;
}

void HDR_RhiVideoWindow::renderOverlayInPass(QRhiCommandBuffer* cb)
{
  if (!m_initialized || !m_rhi || !m_swapChain || !cb)
    return;

  if (m_overlayImage.isNull())
    return;

  // Initialize overlay pipeline if needed
  if (!m_overlayResourcesInitialized) {
    if (!initializeOverlayPipeline())
      return;
  }
  
  // Resize overlay texture if needed
  const QSize overlaySize = m_overlayImage.size();
  if (m_overlayTexture->pixelSize() != overlaySize) {
    m_overlayTexture->setPixelSize(overlaySize);
    m_overlayTexture->create();
    
    // Recreate SRB with new texture
    m_overlaySrb->setBindings({
      QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_overlayUniformBuffer.get()),
      QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_overlayTexture.get(), m_overlaySampler.get())
    });
    m_overlaySrb->create();
  }
  
  QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();
  if (!batch)
    return;
  
 // Convert overlay from premultiplied ARGB32 to RGBA16F with HDR brightness boost.
  // In scRGB extended linear, 1.0 = 80 nits (SDR reference white).
  // Overlay UI elements need RGB values > 1.0 to match the display's SDR white level,
  // otherwise text and numbers appear extremely dim on HDR displays.
  const int w = m_overlayImage.width();
  const int h = m_overlayImage.height();

  const float sdrWhiteNits = std::clamp(m_displayMaxLuminance * 0.25f, 200.0f, 400.0f);
  const float hdrBoost = sdrWhiteNits / 80.0f;

  constexpr int kBytesPerPixel = 8; // 4 x qfloat16
  const int dstStride = w * kBytesPerPixel;
  QByteArray float16Data(h * dstStride, '\0');

  for (int y = 0; y < h; ++y) {
    const QRgb* src = reinterpret_cast<const QRgb*>(m_overlayImage.constScanLine(y));
    qfloat16* dst = reinterpret_cast<qfloat16*>(float16Data.data() + y * dstStride);
    for (int x = 0; x < w; ++x) {
      const QRgb pixel = src[x];
      const int a = qAlpha(pixel);
      if (a > 0) {
        const float rgbScale = hdrBoost / static_cast<float>(a);
        dst[x * 4 + 0] = qfloat16(qRed(pixel) * rgbScale);
        dst[x * 4 + 1] = qfloat16(qGreen(pixel) * rgbScale);
        dst[x * 4 + 2] = qfloat16(qBlue(pixel) * rgbScale);
        dst[x * 4 + 3] = qfloat16(a * (1.0f / 255.0f));
      }
    }
  }

  QRhiTextureSubresourceUploadDescription subDesc;
  subDesc.setData(float16Data);
  subDesc.setSourceSize(QSize(w, h));
  subDesc.setDataStride(static_cast<quint32>(dstStride));
  batch->uploadTexture(m_overlayTexture.get(),
                       QRhiTextureUploadDescription({QRhiTextureUploadEntry(0, 0, subDesc)}));
  
  // Update uniform buffer with identity projection and texture matrices.
  UniformData overlayUniformData;
  QMatrix4x4 identityMatrix;
  identityMatrix.setToIdentity();
  std::memcpy(overlayUniformData.projectionMatrix, identityMatrix.constData(), 16 * sizeof(float));
  std::memcpy(overlayUniformData.textureMatrix, identityMatrix.constData(), 16 * sizeof(float));
  batch->updateDynamicBuffer(m_overlayUniformBuffer.get(),
                             0,
                             sizeof(UniformData),
                             &overlayUniformData);
  
  // Submit resource updates before rendering
  cb->resourceUpdate(batch);
  
  // Render overlay quad (within current render pass, alpha-blended on video)
  cb->setGraphicsPipeline(m_overlayPipeline.get());
  cb->setShaderResources(m_overlaySrb.get());
  
  const QSize outputSize = m_swapChain->currentPixelSize();
  cb->setViewport({ 0, 0, float(outputSize.width()), float(outputSize.height()) });
  
  const QRhiCommandBuffer::VertexInput vbufBindings[] = {
    { m_vertexBuffer.get(), 0 }
  };
  cb->setVertexInput(0, 1, vbufBindings, m_indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);
  cb->drawIndexed(6);
}

void HDR_RhiVideoWindow::paintGrid(QPainter& painter, const QPoint& /*center*/, const QPointF& /*offset*/, int /*xMinClip*/, int /*xMaxClip*/)
{
  // Combined early exit check
  if (m_gridSize == 0 || m_frameSize.width() <= 0 || m_frameSize.height() <= 0) return;

         // Branchless zoom factor with max pattern
  const double zoomFactor = (m_parentZoom > 0.0) ? m_parentZoom : 1.0;
  const int scaledWidth = static_cast<int>(m_frameSize.width() * zoomFactor);
  const int scaledHeight = static_cast<int>(m_frameSize.height() * zoomFactor);
  painter.setPen(m_gridColor);

         // Use bit shift for division by 2
  const int halfWidth = FAST_DIV2(scaledWidth);
  const int halfHeight = FAST_DIV2(scaledHeight);
  const int gridZoom = static_cast<int>(m_gridSize * zoomFactor);
  
  if (gridZoom <= 0) return;
  
  // Pre-compute negated half dimensions to avoid repeated negation in loop
  const int negHalfWidth = -halfWidth;
  const int negHalfHeight = -halfHeight;
  
  // Horizontal lines - compute loop bounds once
  const int numHLines = (scaledHeight - 1) / gridZoom;
  for (int y = 1; y <= numHLines; ++y) {
    const int yPos = negHalfHeight + y * gridZoom;
    painter.drawLine(negHalfWidth, yPos, halfWidth, yPos);
  }
  
  // Vertical lines - compute loop bounds once
  const int numVLines = (scaledWidth - 1) / gridZoom;
  for (int x = 1; x <= numVLines; ++x) {
    const int xPos = negHalfWidth + x * gridZoom;
    painter.drawLine(xPos, negHalfHeight, xPos, halfHeight);
  }
}

void HDR_RhiVideoWindow::paintPixelRulersX(QPainter& painter, const QPoint& center, const QPointF& offset,
                                           int xPixMin, int xPixMax, double zoom)
{
  // Early exit for low zoom levels
  if (zoom < 32.0 || m_frameSize.isEmpty()) return;
  
  // Static font initialization (done once)
  static const QFont valueFont = []() {
    QFont font(QStringLiteral("helvetica"), 10);
    font.setKerning(false);
    return font;
  }();
  painter.setFont(valueFont);

  const qreal dpr = devicePixelRatio();
  const qreal invDpr = 1.0 / dpr;  // Pre-compute inverse for division-to-multiplication
  
  // Lambda for pixel snapping using pre-computed inverse
  auto snap = [dpr, invDpr](qreal v) -> qreal { 
    return qreal(qRound(v * dpr)) * invDpr; 
  };

  const int frameWidth = m_frameSize.width();
  const qreal videoWidth = frameWidth * zoom;
  const qreal halfVideoWidth = videoWidth * 0.5;  // Use multiplication instead of division
  
  const QPoint worldTransform = center + offset.toPoint();
  
  // Compute visible pixel range
  int xMin = static_cast<int>((halfVideoWidth - worldTransform.x() - xPixMin) / zoom);
  int xMax = static_cast<int>((halfVideoWidth - (worldTransform.x() - xPixMax)) / zoom);
  xMin = clipValue(xMin, 0, frameWidth);
  xMax = clipValue(xMax, 0, frameWidth);

         // Pre-create pens outside loop to avoid repeated construction
  QPen whitePen(Qt::white);
  whitePen.setWidth(0);
  QPen blackPen(Qt::black);
  blackPen.setWidth(0);
  
  // Pre-compute constants for loop
  const qreal halfZoom = zoom * 0.5;
  const bool showAllNumbers = (zoom >= 128.0);
  static const QFontMetrics metrics(valueFont);
  
  for (int x = xMin; x <= xMax; ++x) {
    const qreal xPos = snap(static_cast<qreal>(x) * zoom - halfVideoWidth + worldTransform.x());
    
    // Draw tick marks
    painter.setPen(whitePen);
    painter.drawLine(QLineF(xPos, 0.0, xPos, 5.0));
    painter.setPen(blackPen);
    painter.drawLine(QLineF(xPos + invDpr, 0.0, xPos + invDpr, 5.0));
    
    // Draw numbers (every 5th or all if zoom >= 128)
    // Use bitwise AND for modulo 5 check when possible, but x % 5 is still needed
    if ((showAllNumbers || (x % 5 == 0)) && x != frameWidth) {
      const QString numberText = QString::number(x);
      const QSize rectSize = metrics.size(0, numberText) + QSize(4, 0);
      const qreal textX = snap(xPos + halfZoom - rectSize.width() * 0.5);
      const QRectF textRect(QPointF(textX, snap(2.0)), QSizeF(rectSize));
      
      painter.fillRect(textRect, Qt::white);
      painter.setPen(blackPen);
      painter.setRenderHint(QPainter::TextAntialiasing, false);
      painter.drawText(textRect, Qt::AlignCenter, numberText);
    }
  }
}

void HDR_RhiVideoWindow::paintPixelRulersY(QPainter& painter, const QPoint& center, const QPointF& offset,
                                           int yPixMax, int xPos, double zoom)
{
  // Early exit for low zoom levels
  if (zoom < 32.0 || m_frameSize.isEmpty()) return;
  
  // Static font initialization (done once)
  static const QFont valueFont = []() {
    QFont font(QStringLiteral("helvetica"), 10);
    font.setKerning(false);
    return font;
  }();
  painter.setFont(valueFont);

  const qreal dpr = devicePixelRatio();
  const qreal invDpr = 1.0 / dpr;  // Pre-compute inverse
  
  // Lambda for pixel snapping
  auto snap = [dpr, invDpr](qreal v) -> qreal { 
    return qreal(qRound(v * dpr)) * invDpr; 
  };

  const int frameHeight = m_frameSize.height();
  const qreal videoHeight = frameHeight * zoom;
  const qreal halfVideoHeight = videoHeight * 0.5;
  
  const QPoint worldTransform = center + offset.toPoint();
  
  // Compute visible pixel range
  int yMin = static_cast<int>((halfVideoHeight - worldTransform.y()) / zoom);
  int yMax = static_cast<int>((halfVideoHeight - (worldTransform.y() - yPixMax)) / zoom);
  yMin = clipValue(yMin, 0, frameHeight);
  yMax = clipValue(yMax, 0, frameHeight);

         // Pre-create pens outside loop
  QPen whitePen(Qt::white);
  whitePen.setWidth(0);
  QPen blackPen(Qt::black);
  blackPen.setWidth(0);
  
  // Pre-compute constants
  const qreal halfZoom = zoom * 0.5;
  const bool showAllNumbers = (zoom >= 128.0);
  const qreal xPosReal = static_cast<qreal>(xPos);
  const qreal snappedXPosPlus2 = snap(xPosReal + 2.0);
  static const QFontMetrics metrics(valueFont);

  for (int y = yMin; y <= yMax; ++y) {
    const qreal yPos = snap(static_cast<qreal>(y) * zoom - halfVideoHeight + worldTransform.y());
    
    // Draw tick marks
    painter.setPen(whitePen);
    painter.drawLine(QLineF(xPosReal, yPos, xPosReal + 5.0, yPos));
    painter.setPen(blackPen);
    painter.drawLine(QLineF(xPosReal, yPos + invDpr, xPosReal + 5.0, yPos + invDpr));
    
    // Draw numbers
    if ((showAllNumbers || (y % 5 == 0)) && y != frameHeight) {
      const QString numberText = QString::number(y);
      const QSize rectSize = metrics.size(0, numberText) + QSize(4, 0);
      const qreal textY = snap(yPos + halfZoom - rectSize.height() * 0.5);
      const QRectF textRect(QPointF(snappedXPosPlus2, textY), QSizeF(rectSize));
      
      painter.fillRect(textRect, Qt::white);
      painter.setPen(blackPen);
      painter.setRenderHint(QPainter::TextAntialiasing, false);
      painter.drawText(textRect, Qt::AlignCenter, numberText);
    }
  }
}

void HDR_RhiVideoWindow::paintZoomBox(QPainter& painter, int viewIndex, const QPoint& /*center*/, const QPoint& drawAreaBotR, int xSplit, int /*frameIndex*/)
{
  // Combined early exit
  if (!m_zoomBoxEnabled || m_frameSize.isEmpty()) return;

         // Pre-computed constants for zoom box rendering
  constexpr int kZoomBoxWindowZoomFactor = 32;
  constexpr int kSrcSize = 5;
  constexpr int kMargin = 11;
  constexpr int kPadding = 8;
  constexpr int kHalfSrcSize = kSrcSize >> 1;  // srcSize / 2 using bit shift
  
  int zoomBoxSize = kSrcSize * kZoomBoxWindowZoomFactor;  // 5 * 32 = 160

  QRect zoomViewRect(0, 0, zoomBoxSize, zoomBoxSize);
  bool drawInfoPanel = !m_playing;
  
  // Bounds check for split view
  const int rightBoundary = drawAreaBotR.x() - kMargin - zoomBoxSize;
  if (viewIndex == 1 && xSplit > rightBoundary) {
    if (xSplit > drawAreaBotR.x() - kMargin) return;
    zoomViewRect.setWidth(drawAreaBotR.x() - xSplit - kMargin);
    drawInfoPanel = false;
  }

         // Check zoom threshold once
  const bool shouldDrawZoomView = (m_parentZoom < kZoomBoxWindowZoomFactor);
  
  if (shouldDrawZoomView) {
    // Compute bottom-right position based on view configuration
    // Reduce branches by computing target point first
    QPoint targetBottomRight;
    const bool isSideBySide = (m_overlaySplitMode == OverlaySplitSideBySide);
    
    if (isSideBySide) {
      const int xPos = (viewIndex == 0) ? (xSplit - kMargin) : (drawAreaBotR.x() - kMargin);
      targetBottomRight = QPoint(xPos, drawAreaBotR.y() - kMargin);
    } else {
      targetBottomRight = drawAreaBotR - QPoint(kMargin, kMargin);
    }
    zoomViewRect.moveBottomRight(targetBottomRight);

    painter.setPen(Qt::black);
    painter.fillRect(zoomViewRect, painter.background());

           // Source rect centered at pixel position
    const QPoint& pix = m_zoomBoxPixelPos[viewIndex];
    
    // Combined bounds check for pixel position
    if (pix.x() >= 0 && pix.y() >= 0) {
      const int maxSrcX = m_frameSize.width() - kSrcSize;
      const int maxSrcY = m_frameSize.height() - kSrcSize;

      const int srcX = clipValue(pix.x() - kHalfSrcSize, 0, maxSrcX);
      const int srcY_img = clipValue(pix.y() - kHalfSrcSize, 0, maxSrcY);

      // Prefer the lazy patch provider so the HDR playback path does not
      // pay for a full-frame CPU YUV->RGB conversion. Only the 5x5 region
      // we are about to draw is converted on-demand.
      QImage patch;
      const QRect patchRect(srcX, srcY_img, kSrcSize, kSrcSize);
      if (m_overlayPatchProvider) {
        patch = m_overlayPatchProvider(patchRect);
      }

      if (!patch.isNull()) {
        painter.drawImage(zoomViewRect, patch, patch.rect());
      } else if (!m_currentFrame.isNull()) {
        // RGB fallback path: full CPU frame from updateFrame().
        // Flip Y to match the Y-up texture layout expected by this overlay.
        const int srcY = m_frameSize.height() - srcY_img - kSrcSize;
        const QRect srcRect(srcX, srcY, kSrcSize, kSrcSize);
        painter.drawImage(zoomViewRect, m_currentFrame, srcRect);
      }
    }

    painter.drawRect(zoomViewRect);
  } else {
    zoomBoxSize = 0;
  }

  if (drawInfoPanel) {
    video::yuv::yuv_t yuvValue;
    const bool hasYuvValue = getYuvPixelValue(m_zoomBoxPixelPos[viewIndex], yuvValue);

    QString pixelInfoString = QString("<h4>Coordinates</h4>"
                                      "<table width=\"100%\">"
                                      "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
                                      "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
                                      "</table>")
                                .arg(m_zoomBoxPixelPos[viewIndex].x())
                                .arg(m_zoomBoxPixelPos[viewIndex].y());
    if (hasYuvValue) {
      pixelInfoString.append(QString("<h4>YUV</h4>"
                                     "<table width=\"100%\">"
                                     "<tr><td>Y:</td><td align=\"right\">%1</td></tr>"
                                     "<tr><td>U:</td><td align=\"right\">%2</td></tr>"
                                     "<tr><td>V:</td><td align=\"right\">%3</td></tr>"
                                     "</table>")
                               .arg(yuvValue.Y)
                               .arg(yuvValue.U)
                               .arg(yuvValue.V));
    }
    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(pixelInfoString);
    textDocument.setTextWidth(textDocument.size().width());

           // Use constexpr constants for margin and padding
    if (viewIndex == 0 && m_overlaySplitMode == OverlaySplitSideBySide) {
      painter.translate(xSplit - kMargin - zoomBoxSize - textDocument.size().width() - kPadding * 2 + 1,
                        drawAreaBotR.y() - kMargin - textDocument.size().height() - kPadding * 2 + 1);
    } else {
      painter.translate(drawAreaBotR.x() - kMargin - zoomBoxSize - textDocument.size().width() - kPadding * 2 + 1,
                        drawAreaBotR.y() - kMargin - textDocument.size().height() - kPadding * 2 + 1);
    }
    QRect rect(QPoint(0,0), textDocument.size().toSize() + QSize(2 * kPadding, 2 * kPadding));
    QBrush originalBrush;
    painter.setBrush(QColor(0, 0, 0, 210));
    painter.setPen(Qt::black);
    painter.drawRect(rect);
    painter.translate(kPadding, kPadding);
    textDocument.drawContents(&painter);
    painter.setBrush(originalBrush);
    painter.resetTransform();
  }
}

bool HDR_RhiVideoWindow::getYuvPixelValue(const QPoint& pixelPos, video::yuv::yuv_t& value) const
{
  if (pixelPos.x() < 0 || pixelPos.y() < 0 || pixelPos.x() >= m_frameSize.width() || pixelPos.y() >= m_frameSize.height()) {
    return false;
  }

  if (m_overlayYuvPixelProvider)
    return m_overlayYuvPixelProvider(pixelPos, value);

  return false;
}

void HDR_RhiVideoWindow::paintRawPixelValues(QPainter&    painter,
                                             const QPoint& center,
                                             const QPointF& offset,
                                             int xPixMin,
                                             int xPixMax,
                                             double zoom)
{
  if (m_frameSize.isEmpty()) {
    return;
  }

  QFont valueFont(QStringLiteral("helvetica"), 10);
  painter.setFont(valueFont);
  QFontMetrics metrics(valueFont);

  const QSize videoRect = m_frameSize * zoom;
  const QPoint worldTransform = center + offset.toPoint();
  int xMin = static_cast<int>((videoRect.width() / 2 - worldTransform.x() - xPixMin) / zoom);
  int xMax = static_cast<int>((videoRect.width() / 2 - (worldTransform.x() - xPixMax)) / zoom);
  int yMin = static_cast<int>((videoRect.height() / 2 - worldTransform.y()) / zoom);
  int yMax = static_cast<int>((videoRect.height() / 2 - (worldTransform.y() - height())) / zoom);
  xMin = clipValue(xMin, 0, m_frameSize.width() - 1);
  xMax = clipValue(xMax, 0, m_frameSize.width() - 1);
  yMin = clipValue(yMin, 0, m_frameSize.height() - 1);
  yMax = clipValue(yMax, 0, m_frameSize.height() - 1);

  const int formatBase = QSettings().value("ShowPixelValuesHex").toBool() ? 16 : 10;
  for (int x = xMin; x <= xMax; ++x) {
    for (int y = yMin; y <= yMax; ++y) {
      video::yuv::yuv_t value;
      if (!getYuvPixelValue(QPoint(x, y), value)) {
        continue;
      }

      const QString text = buildVerticalYuvText(value, formatBase);
      const QSize rectSize = metrics.size(0, text) + QSize(4, 2);
      const int xPos = static_cast<int>(x * zoom - videoRect.width() / 2 + worldTransform.x() + zoom / 2 - rectSize.width() / 2);
      const int yPos = static_cast<int>(y * zoom - videoRect.height() / 2 + worldTransform.y() + zoom / 2 - rectSize.height() / 2);
      const QRect textRect(QPoint(xPos, yPos), rectSize);
      painter.fillRect(textRect, QColor(0, 0, 0, 220));
      painter.setPen(Qt::white);
      painter.drawText(textRect, Qt::AlignCenter, text);
    }
  }
}

void HDR_RhiVideoWindow::drawItemPathAndName(QPainter& painter, int posX, int width, const QString& path)
{
  if (path.isEmpty()) return;
  
  static QFont valueFont = []() {
    QFont font(QStringLiteral("helvetica"), 10);
    return font;
  }();
  painter.setFont(valueFont);
  QFontMetrics metrics(valueFont);
  
  QString drawString;
  const QChar sep = QDir::separator();
  const QStringList parts = path.split(sep);

  QString currentLine;
  for (int i = 0; i < parts.size(); ++i) {
    const bool isLast = (i == parts.size() - 1);
    if (currentLine.isEmpty()) {
      currentLine = parts[i];
      if (!isLast) currentLine += sep;
    } else {
      QString lineTemp = currentLine + parts[i];
      if (!isLast) lineTemp += sep;
      const QSize textSize = metrics.size(0, lineTemp);
      if (textSize.width() > width - 20) {
        drawString += currentLine + "\n";
        currentLine = parts[i];
        if (!isLast) currentLine += sep;
      } else {
        currentLine = lineTemp;
      }
    }
  }
  if (!currentLine.isEmpty()) drawString += currentLine;

  const QSize textSize = metrics.size(0, drawString);
  QRect textRect; textRect.setSize(textSize);
  textRect.moveCenter(QPoint(posX + width / 2, 0));
  textRect.moveTop(10);
  QRect boxRect = textRect + QMargins(5,5,5,5);
  painter.setPen(QPen(Qt::black, 1));
  painter.fillRect(boxRect, Qt::white);
  painter.drawRect(boxRect);
  painter.drawText(textRect, Qt::AlignCenter, drawString);
}

void HDR_RhiVideoWindow::drawLoadingMessage(QPainter& painter, const QPoint& pos)
{
  static QFont valueFont = []() {
    QFont font(QStringLiteral("helvetica"), 10);
    return font;
  }();
  painter.setFont(valueFont);
  QFontMetrics metrics(painter.font());
  auto text = QStringLiteral("Loading...");
  const QSize textSize = metrics.size(0, text);
  QRect textRect; textRect.setSize(textSize); textRect.moveCenter(pos);
  QRect boxRect = textRect + QMargins(5,5,5,5);
  painter.setPen(QPen(Qt::black, 1));
  painter.fillRect(boxRect, Qt::white);
  painter.drawRect(boxRect);
  painter.drawText(textRect, Qt::AlignCenter, text);
}

OVERLAY_FORCE_INLINE bool HDR_RhiVideoWindow::isPixelDarkAt(const QPoint& pixel) const
{
  // Combined bounds check using unsigned comparison trick
  // If pixel.x() < 0, casting to unsigned makes it a large positive number > width
  const unsigned px = static_cast<unsigned>(pixel.x());
  const unsigned py = static_cast<unsigned>(pixel.y());
  const unsigned fw = static_cast<unsigned>(m_frameSize.width());
  const unsigned fh = static_cast<unsigned>(m_frameSize.height());

  if (px >= fw || py >= fh) return false;

  // Primary path: delegate to the handler's live raw buffer via the lazy YUV
  // pixel provider installed by SplitViewWidget. This avoids retaining a second
  // full-frame YUV copy inside the HDR window for overlay sampling.
  {
    video::yuv::yuv_t yuvValue;
    if (getYuvPixelValue(pixel, yuvValue)) {
      return yuvValue.Y < (1 << 9);
    }
  }

  // RGB fallback path: reachable when updateFrame() pushed a full CPU frame.
  if (!m_currentFrame.isNull()) {
    const int flippedY = static_cast<int>(fh) - 1 - pixel.y();
    const QRgb pixelColor = m_currentFrame.pixel(pixel.x(), flippedY);
    return qGray(pixelColor) < 128;
  }

  return false;
}
