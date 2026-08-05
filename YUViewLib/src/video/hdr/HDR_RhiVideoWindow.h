/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDR_RHIVIDEOWINDOW_H
#define HDR_RHIVIDEOWINDOW_H

#include <QWindow>
#include <QMatrix4x4>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QPoint>
#include <QString>
#include <QPixmap>
#include <QFont>
#include <QByteArray>
#include <QOffscreenSurface>
#include <QMutex>
#include <QRect>
#include <memory>
#include <atomic>
#include <cstdint>
#include <functional>

// Include HDR module headers for modular architecture
#include "HDRTypes.h"
#include "HDRColorConversion.h"


// Qt RHI API (requires Qt 6.6+ for public rhi module, or 6.4+ with private API)
// Qt 6.6+ provides public <rhi/qrhi.h>, earlier versions need QtGui private headers
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
// Qt 6.6+ public API - all RHI types are in rhi/qrhi.h
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
// Note: QRhiD3D11InitParams, QRhiD3D12InitParams, QRhiGles2InitParams, etc.
// are all declared in rhi/qrhi.h in Qt 6.6+
#define YUVIEW_HAVE_QRHI 1
#elif QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
// Qt 6.4/6.5 private API path - need to include platform-specific headers
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qshader_p.h>
// Platform-specific RHI backends
#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QtGui/private/qrhid3d12_p.h>
#endif
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
#include <QtGui/private/qrhivulkan_p.h>
#endif
#ifdef Q_OS_MACOS
#include <QtGui/private/qrhimetal_p.h>
#endif
#include <QtGui/private/qrhigles2_p.h>
#define YUVIEW_HAVE_QRHI 1
#define YUVIEW_QRHI_PRIVATE_API 1
#else
// Fallback for older Qt versions - RHI HDR window will not be available
#define YUVIEW_HAVE_QRHI 0
#warning "QRhi HDR support requires Qt 6.4 or later"
#endif

#include "HDRDetection.h"
#include <common/Typedef.h>
#include "../yuv/PixelFormatYUV.h"
#include "../yuv/YUVConversionTypes.h"

class QPainter;

/**
 * @brief HDR Video Window using Qt RHI (Rendering Hardware Interface) with D3D12 backend
 * 
 * This class implements native HDR rendering with Qt QRhi.
 * The renderer uses an extended-linear HDR swapchain and RGBA16F textures as the unified path.
 * 
 * The fragment shader performs:
 * - YUV->RGB conversion in BT.2020
 * - PQ / HLG / Linear transfer processing in realtime
 * - Conversion to scRGB linear output for HDR presentation
 * 
 * Key features:
 * - Unified RGBA16F rendering path for all HDR modes
 * - GPU YUV conversion and transfer processing in a single fragment shader
 * - Uses QRhiSwapChainHdrInfo to query display HDR metadata (luminance, etc.)
 * - No LUT texture dependency in HDR shader pipeline
 */
class HDR_RhiVideoWindow : public QWindow
{
  Q_OBJECT

public:
  /**
   * @brief Render mode enumeration (HDR only)
   * 
   * Mode_BT2020_PQ_10bit: HDR10 with PQ transfer function (SMPTE ST.2084)
   * Mode_BT2020_HLG_10bit: HDR with HLG transfer function, processed in shader
   * Mode_BT2020_Linear_16bit: Linear light pass-through to display (HDRExtendedDisplayP3Linear)
   *                           Uses RGBA16F textures and scRGB color space
   */
  enum RenderMode {
    Mode_BT2020_PQ_10bit = 1,     // HDR10 BT.2020 PQ 10-bit rendering (SMPTE ST.2084)
    Mode_BT2020_HLG_10bit = 2,    // HLG rendering with realtime shader math
    Mode_BT2020_Linear_16bit = 3  // Linear light pass-through (scRGB, 16-bit float)
  };

  /**
   * @brief Overlay split mode for comparison views
   */
  enum OverlaySplitMode {
    OverlaySplitDisabled = 0,
    OverlaySplitSideBySide = 1,
    OverlaySplitComparison = 2
  };

  /**
   * @brief Callback type used by the overlay to lazily obtain a small RGB
   *        patch of the current frame.
   *
   * Given a rectangle in *frame pixel coordinates* (origin top-left), the
   * implementation returns a QImage of exactly that region's size. It is
   * only invoked when the ZoomBox overlay is enabled and actually has to
   * draw a pixel preview.
   *
   * Returning a null QImage is allowed and signals "no patch available"
   * (the overlay will then fall back to leaving the preview empty).
   */
  using OverlayPatchProvider = std::function<QImage(const QRect&)>;

  /**
   * @brief Lazy YUV pixel sampler for overlay features (zoom info, raw values).
   *
   * Invoked from the GUI thread during overlay painting. Returns false when the
   * handler cannot supply a sample for the requested pixel.
   */
  using OverlayYuvPixelProvider =
      std::function<bool(const QPoint&, video::yuv::yuv_t&)>;

  /**
   * @brief Constructor
   * @param parent Parent window (optional)
   */
  explicit HDR_RhiVideoWindow(QWindow* parent = nullptr);
  
  /**
   * @brief Destructor - releases RHI resources
   */
  ~HDR_RhiVideoWindow() override;

  // Configuration
  void setRenderMode(RenderMode mode);
  /** @brief Current HDR transfer mode (PQ / HLG / Linear). */
  RenderMode getRenderMode() const { return m_renderMode; }

  /**
   * @brief Set tone-map / exposure target luminance in nits.
   *
   * PQ uses ITU-R BT.2390 EETF; HLG uses a limited exposure range.
   * Pass 0 to disable mapping (pass-through).
   */
  void setToneMapTargetNits(float nits);
  /** @brief Current exposure / tone-map target luminance in nits. */
  float getToneMapTargetNits() const { return m_toneMapTargetNits; }

  /** @brief Whether the RHI pipeline has finished initialization. */
  bool isInitialized() const { return m_initialized; }
  /** @brief Whether initialized and a valid QRhi instance is available. */
  bool isReadyForRendering() const { return m_initialized && m_rhi != nullptr; }

  /**
   * @brief Explicitly try to initialize the HDR RHI backend.
   *
   * Useful when QWidget::createWindowContainer delays expose events.
   * Requires the window to already be exposed.
   */
  bool tryInitialize();

  /**
   * @brief Apply display HDR capabilities (currently syncs peak luminance).
   * @param capabilities Detected display HDR capabilities
   */
  void setHDRCapabilities(const HDRDetection::HDRCapabilities& capabilities);

  /**
   * @brief Submit a YUV frame with move semantics (zero-copy ownership transfer).
   *
   * Not a Qt slot because MOC does not support rvalue-reference parameters.
   * The caller must not use yuvData after this call.
   */
  void updateFrameYUVMove(QByteArray&& yuvData,
                          int width, int height,
                          const video::yuv::PixelFormatYUV& format,
                          video::yuv::ColorConversion colorConversion);

  /**
   * @brief Register or clear the overlay lazy RGB patch provider.
   *
   * ZoomBox and similar overlays fetch only a small region on demand.
   */
  void setOverlayPatchProvider(OverlayPatchProvider provider)
  {
    m_overlayPatchProvider = std::move(provider);
  }

  /**
   * @brief Register or clear the overlay lazy YUV pixel sampler.
   *
   * Overlays sample from the active video handler instead of mirroring a full frame.
   */
  void setOverlayYuvPixelProvider(OverlayYuvPixelProvider provider)
  {
    m_overlayYuvPixelProvider = std::move(provider);
  }

public slots:
  /**
   * @brief Update frame from a QImage (RGB fallback path).
   * @param newFrame Converted image; prefer updateFrameYUV* for HDR playback
   */
  void updateFrame(const QImage& newFrame);

  /**
   * @brief Update frame from raw YUV (primary GPU path for 10-bit HDR).
   *
   * The const-ref overload copies the buffer; use updateFrameYUVMove when exclusive.
   */
  void updateFrameYUV(const QByteArray& yuvData,
                      int width, int height,
                      const video::yuv::PixelFormatYUV& format,
                      video::yuv::ColorConversion colorConversion);

  /** @brief Clear the current frame, pending YUV, and overlay samplers. */
  void clearFrame();

  /**
   * @brief Update parent view transform (zoom / pan).
   * @param renderZoom Effective render zoom (DPI-compensated)
   * @param offset Pan offset
   * @param displayZoom UI display zoom; if &lt; 0, equals renderZoom
   */
  void updateTransform(double renderZoom, const QPointF& offset, double displayZoom = -1.0);

  /** @brief Set clear / background color and request a redraw. */
  void setBackgroundColor(const QColor& color)
  {
    m_backgroundColor = color;
    requestUpdate();
  }

  // Overlay configuration (short setters inlined to avoid cpp boilerplate)
  void setOverlayEnabled(bool enabled)
  {
    if (m_overlayEnabled == enabled)
      return;
    m_overlayEnabled = enabled;
    requestUpdate();
  }

  void setSplitting(OverlaySplitMode mode, double splitPoint)
  {
    m_overlaySplitMode = mode;
    m_overlaySplitPoint = qBound(0.0, splitPoint, 1.0);
    requestUpdate();
  }

  void setGridParams(int gridSize, const QColor& gridColor)
  {
    m_gridSize = gridSize;
    m_gridColor = gridColor;
    requestUpdate();
  }

  void setZoomBoxState(bool enabled, const QPoint& pixelPosLeft, const QPoint& pixelPosRight)
  {
    m_zoomBoxEnabled = enabled;
    m_zoomBoxPixelPos[0] = pixelPosLeft;
    m_zoomBoxPixelPos[1] = pixelPosRight;
    requestUpdate();
  }

  void setDrawItemPathAndName(bool enabled, const QString& left, const QString& right)
  {
    m_drawPathAndName = enabled;
    m_itemPathLeft = left;
    m_itemPathRight = right;
    requestUpdate();
  }

  void setLoadingFlags(bool leftLoading, bool rightLoading)
  {
    m_loadingLeft = leftLoading;
    m_loadingRight = rightLoading;
    requestUpdate();
  }

  void setDrawRawValues(bool enabled) { m_drawRawValues = enabled; }

  void setPlayingState(bool isPlaying, bool waitingForCaching)
  {
    m_playing = isPlaying;
    m_waitingForCaching = waitingForCaching;
    requestUpdate();
  }

  void setCachingIndicatorPixmap(const QPixmap& pixmap) { m_cachingPixmap = pixmap; }

signals:
  void widgetInitialized();
  void hdrNotSupported(const QString& reason);
  void frameUpdated();
  void rhiError(const QString& error);

protected:
  // QWindow event handlers
  void exposeEvent(QExposeEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool event(QEvent* event) override;

private:
  // RHI initialization
  bool initializeRhi();
  bool initializeSwapChain();
  bool initializePipeline();
  bool initializeYUVPipeline();
  void releaseRhiResources();
  
  // Rendering
  void render();
  void renderFrame(QRhiCommandBuffer* cb);
  void updateProjectionMatrix();
  
  // Resource management
  bool uploadTextureData(const QImage& image, QRhiResourceUpdateBatch* batch);
  bool uploadYUVTextureData(const QByteArray& yuvData, int width, int height,
                            const video::yuv::PixelFormatYUV& format,
                            QRhiResourceUpdateBatch* batch);
  
  // Overlay rendering (QPainter to QImage, then upload to RHI texture)
  void paintOverlays();
  void paintGrid(QPainter& painter, const QPoint& center, const QPointF& offset, int xMinClip, int xMaxClip);
  void paintPixelRulersX(QPainter& painter, const QPoint& center, const QPointF& offset, int xPixMin, int xPixMax, double zoom);
  void paintPixelRulersY(QPainter& painter, const QPoint& center, const QPointF& offset, int yPixMax, int xPos, double zoom);
  void paintZoomBox(QPainter& painter, int viewIndex, const QPoint& center, const QPoint& drawAreaBotR, int xSplit, int frameIndex);
  /**
   * @brief Draw raw YUV values on top of pixels in HDR mode for high zoom levels.
   */
  void paintRawPixelValues(QPainter& painter,
                           const QPoint& center,
                           const QPointF& offset,
                           int xPixMin,
                           int xPixMax,
                           double zoom);
  void drawItemPathAndName(QPainter& painter, int posX, int width, const QString& path);
  void drawLoadingMessage(QPainter& painter, const QPoint& pos);
  bool isPixelDarkAt(const QPoint& pixel) const;
  bool getYuvPixelValue(const QPoint& pixelPos, video::yuv::yuv_t& value) const;
  
  template <typename T> static T clipValue(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

  /**
   * @brief Queue a pending YUV frame for deferred GPU upload.
   * Shared by updateFrameYUV (copy) and updateFrameYUVMove (move).
   */
  void queuePendingYUV(QByteArray&& yuvData,
                       int width, int height,
                       const video::yuv::PixelFormatYUV& format,
                       video::yuv::ColorConversion colorConversion);

  /**
   * @brief Refresh the cached YUV->RGB matrix/offset when conversion mode changes.
   */
  void ensureCachedColorMatrix(video::yuv::ColorConversion colorConversion)
  {
    if (m_colorMatrixCacheValid && m_cachedColorConversion == colorConversion)
      return;
    video::hdr::HDRColorConversion::buildColorMatrix(
        colorConversion, m_cachedColorMatrix, m_cachedOffsetVec);
    m_cachedColorConversion = colorConversion;
    m_colorMatrixCacheValid = true;
  }

  /**
   * @brief Create or reuse R16 Y/U/V textures and rebind SRB when plane sizes change.
   * @param width Luma width in pixels.
   * @param height Luma height in pixels.
   * @param uvWidth Chroma plane width in pixels.
   * @param uvHeight Chroma plane height in pixels.
   * @return true when textures are ready for upload.
   */
  bool ensureYUVTextures(int width, int height, int uvWidth, int uvHeight);

private:
  // Render mode
  RenderMode m_renderMode;
  float m_toneMapTargetNits{0.0f};
  bool m_initialized{false};
  bool m_initializationFailed{false};
  // Guards one-time initialization against re-entrant expose/update sequences.
  mutable QMutex m_initMutex;
  std::atomic<bool> m_initializationInProgress{false};
  bool m_loggedNotExposedState{false};

         // Qt RHI resources
  std::unique_ptr<QRhi> m_rhi;
  std::unique_ptr<QOffscreenSurface> m_fallbackSurface;  // For OpenGL fallback
  std::unique_ptr<QRhiSwapChain> m_swapChain;
  std::unique_ptr<QRhiRenderPassDescriptor> m_renderPassDesc;
  
  // RGB pipeline resources
  std::unique_ptr<QRhiBuffer> m_vertexBuffer;
  std::unique_ptr<QRhiBuffer> m_indexBuffer;
  std::unique_ptr<QRhiBuffer> m_uniformBuffer;
  std::unique_ptr<QRhiTexture> m_videoTexture;
  std::unique_ptr<QRhiSampler> m_sampler;
  std::unique_ptr<QRhiShaderResourceBindings> m_srb;
  std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
  
  // YUV pipeline resources (GPU-based YUV->RGB conversion)
  std::unique_ptr<QRhiTexture> m_texY;
  std::unique_ptr<QRhiTexture> m_texU;
  std::unique_ptr<QRhiTexture> m_texV;
  std::unique_ptr<QRhiBuffer> m_yuvUniformBuffer;
  std::unique_ptr<QRhiShaderResourceBindings> m_yuvSrb;
  std::unique_ptr<QRhiGraphicsPipeline> m_yuvPipeline;
  
  // Uniform data structure for RGB shaders
  struct UniformData {
    float projectionMatrix[16];
    float textureMatrix[16];
  };
  
  // Uniform data structure for YUV shaders
  // Note: For HDR10, the colorMatrix performs BT.2020 YUV->RGB conversion only
  // The output is RGBA16F linear light for scRGB HDR presentation
  // IMPORTANT: Layout must match shader's std140 uniform block exactly:
  //   mat4 projectionMatrix (16 floats)
  //   mat4 textureMatrix    (16 floats)
  //   mat4 colorMatrix      (16 floats) - 3x3 matrix in columns 0-2, column 3 unused
  //   vec4 offsetVec        (4 floats)
  //   vec4 hdrParams        (4 floats) - HDR processing parameters
  struct YUVUniformData {
    float projectionMatrix[16];
    float textureMatrix[16];
    float colorMatrix[16];  // mat4 in std140 layout (must be 16 floats to match shader)
    float offsetVec[4];     // vec4 in std140 layout
    float hdrParams[4];     // x=contentMaxNits, y=displayMaxNits, z=renderMode, w=reserved
  };

         // Cached matrices
  QMatrix4x4 m_textureMatrix;
  QMatrix4x4 m_projectionMatrix;
  double m_parentZoom{1.0};
  double m_displayZoom{1.0};
  QPointF m_parentOffset;

         // Frame data
  QImage m_currentFrame;
  // Lazy patch provider used by overlay (e.g. ZoomBox) to fetch small RGB
  // regions of the current frame without triggering a full-frame CPU
  // YUV->RGB conversion. Set by SplitViewWidget; may be empty.
  OverlayPatchProvider m_overlayPatchProvider;
  // Lazy YUV pixel provider for zoom info / raw-value overlays.
  OverlayYuvPixelProvider m_overlayYuvPixelProvider;
  QSize m_frameSize;
  bool m_useYUVMode{false};
  int m_yuvWidth{0};
  int m_yuvHeight{0};
  int m_uvWidth{0};   // Track UV plane dimensions for format change detection
  int m_uvHeight{0};  // YUV420 vs YUV422 have different UV heights
  video::yuv::ColorConversion m_currentColorConversion{video::yuv::ColorConversion::BT2020_LimitedRange};

  // Cached YUV->RGB matrix/offset; rebuilt only when color conversion changes.
  bool m_colorMatrixCacheValid{false};
  video::yuv::ColorConversion m_cachedColorConversion{video::yuv::ColorConversion::BT2020_LimitedRange};
  float m_cachedColorMatrix[16]{};
  float m_cachedOffsetVec[4]{};
  
  // Pending YUV data for deferred upload.
  // Protected by m_pendingYUVMutex to keep producer/consumer state consistent.
  mutable QMutex m_pendingYUVMutex;
  QByteArray m_pendingYUVData;
  video::yuv::PixelFormatYUV m_pendingYUVFormat;
  video::yuv::ColorConversion m_pendingColorConversion{video::yuv::ColorConversion::BT2020_LimitedRange};
  uint64_t m_pendingYUVSequence{0};
  uint64_t m_lastUploadedYUVSequence{0};
  bool m_yuvDataPending{false};

  // Staging buffers for endian/semi-planar repacking. Must outlive the
  // QRhiResourceUpdateBatch until renderFrame() submits resourceUpdate().
  // Capacity is retained across frames (resize never shrinks) to avoid
  // per-frame heap churn on the P010/BE path.
  QByteArray m_repackBufferY;
  QByteArray m_repackBufferU;
  QByteArray m_repackBufferV;

  QColor m_backgroundColor{Qt::black};
  
  // Peak display luminance from EDID / swap-chain query (used by overlays)
  float m_displayMaxLuminance{1000.0f};

         // Overlay state
  bool m_overlayEnabled{false};
  OverlaySplitMode m_overlaySplitMode{OverlaySplitDisabled};
  double m_overlaySplitPoint{0.5};
  int m_gridSize{0};
  QColor m_gridColor{Qt::white};
  bool m_zoomBoxEnabled{false};
  QPoint m_zoomBoxPixelPos[2]{QPoint(-1,-1), QPoint(-1,-1)};
  bool m_drawPathAndName{false};
  QString m_itemPathLeft;
  QString m_itemPathRight;
  bool m_loadingLeft{false};
  bool m_loadingRight{false};
  bool m_drawRawValues{false};
  bool m_playing{false};
  bool m_waitingForCaching{false};
  QPixmap m_cachingPixmap;
  QFont m_zoomFactorFont{QStringLiteral("helvetica"), 24};

         // Overlay rendering resources
  QImage m_overlayImage;
  std::unique_ptr<QRhiTexture> m_overlayTexture;
  std::unique_ptr<QRhiSampler> m_overlaySampler;
  std::unique_ptr<QRhiShaderResourceBindings> m_overlaySrb;
  std::unique_ptr<QRhiGraphicsPipeline> m_overlayPipeline;
  std::unique_ptr<QRhiBuffer> m_overlayUniformBuffer;
  bool m_overlayResourcesInitialized{false};
  
  // Overlay rendering helper
  void renderOverlayInPass(QRhiCommandBuffer* cb);
  bool initializeOverlayPipeline();
};

#endif // HDR_RHIVIDEOWINDOW_H
