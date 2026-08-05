#pragma once

#include <QObject>
#include <QString>
#include <QWidget>
#include <QWindow>
#include <QImage>
#include <QMutex>
#include <QTimer>
#include <QByteArray>
#include <QPointer>

// HDR Window backend: QRhi with D3D12 for native Windows HDR10 support
// Requires Qt 6.4+ for QRhi API (Qt 6.6+ uses public API, 6.4-6.5 uses private API)
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
#include "HDR_RhiVideoWindow.h"
using HDR_WindowType = HDR_RhiVideoWindow;
#else
#error "HDR10 rendering requires Qt 6.4 or later for QRhi API support"
#endif

#include "HDRDetectionWorker.h"
#include "HDRDetection.h"
#include "../../ui/views/SplitViewWidget.h"
#include "../yuv/PixelFormatYUV.h"

class videoHandlerYUV;

/**
 * @brief Manages HDR10 rendering functionality for video handlers
 * 
 * This class encapsulates all HDR-related operations including:
 * - HDR capability detection via Windows DXGI
 * - HDR window creation and management using QRhi with D3D12 backend
 * - HDR rendering state management
 * - Communication between HDR components and video handlers
 * 
 * The HDR rendering uses QRhiSwapChain::HDR10 format which creates a swap chain with:
 * - DXGI_FORMAT_R10G10B10A2_UNORM
 * - DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 (ST.2084 PQ transfer function)
 * 
 * Windows HDR compositor handles PQ->display transfer function automatically.
 * The YUV->RGB conversion is done in GPU shader using BT.2020 color matrix.
 */
class HDRRenderingManager : public QObject
{
  Q_OBJECT

public:
  explicit HDRRenderingManager(QObject* parent = nullptr);
  ~HDRRenderingManager();

    // Single source of truth for HDR render-mode and tone-mapping initialization
  // derived from QSettings. Previously this logic was duplicated between
  // createHDRContainerForWindow() and onHDRDetectionComplete(); any future
  // changes to the mapping (new modes, new defaults) only need to happen here.
  static void applyHDRWindowSettings(HDR_WindowType* window);

  // Provide a global singleton instance so all handlers share one HDR window
  static HDRRenderingManager* instance();

         // HDR state management (thread-safe)
  bool isHDRRenderingActive() const;
  void setHDRRenderingEnabled(bool enabled);
  
  // HDR window management (thread-safe)
  // Returns the active HDR window (QRhi-based with D3D12 backend)
  HDR_WindowType* getHDRWindow() const;
  
  // HDR detection (thread-safe)
  void startHDRDetection();
  bool isHDRDetectionInProgress() const;
  HDRDetection::HDRCapabilities getHDRCapabilities() const;
  
  // Frame rendering - RGB path (for pre-converted or SDR content)
  void updateHDRFrame(const QImage& frame);
  
  /**
   * @brief Update HDR frame from raw YUV data (GPU conversion path for HDR10)
   * 
   * This is the primary HDR10 rendering path. The YUV data should be:
   * - 10-bit BT.2020 PQ encoded
   * - Planar format (Y plane, then U/V planes)
   * 
   * The GPU shader performs BT.2020 YUV->RGB matrix conversion.
   * Output is PQ-encoded RGB10A2, Windows handles transfer function.
   * 
   * @param yuvData Raw YUV planar data buffer
   * @param width Frame width in pixels
   * @param height Frame height in pixels
   * @param format YUV pixel format (subsampling, bit depth, plane order)
   * @param colorConversion Color conversion matrix type (should be BT.2020 for HDR10)
   */
  void updateHDRFrameYUV(const QByteArray& yuvData,
                         int width, int height,
                         const video::yuv::PixelFormatYUV& format,
                         video::yuv::ColorConversion colorConversion);
  
  /**
   * @brief Move-optimized version of updateHDRFrameYUV (zero-copy for caller-owned data)
   * 
   * PERFORMANCE OPTIMIZATION: Uses move semantics to transfer buffer ownership,
   * avoiding the ~50MB copy for 4K 10-bit content.
   * 
   * Use when the caller has exclusive ownership and doesn't need the buffer after.
   * 
   * @param yuvData Raw YUV planar data buffer (ownership transferred)
   * @param width Frame width in pixels
   * @param height Frame height in pixels
   * @param format YUV pixel format (subsampling, bit depth, plane order)
   * @param colorConversion Color conversion matrix type (should be BT.2020 for HDR10)
   */
  void updateHDRFrameYUVMove(QByteArray&& yuvData,
                             int width, int height,
                             const video::yuv::PixelFormatYUV& format,
                             video::yuv::ColorConversion colorConversion);
  
signals:
  // HDR rendering state change signals
  void hdrRenderingStateChangedWindow(bool enabled, QWindow* window);
  void hdrDetectionCompleted(bool supported);
  void hdrDetectionFailed(const QString& error);

private slots:
  // HDR-related slots
  void onHDRDetectionComplete(const HDRDetection::HDRCapabilities& capabilities);
  void onHDRDetectionFailed(const QString& error);

private:
  // Thread-safe helper methods
  bool isHDRRenderingActive_locked() const;  // Internal method requiring lock held
  void setHDRWidgetReady_locked(bool ready);  // Internal method requiring lock held
  bool isHDRWidgetReady_locked() const;       // Internal method requiring lock held

         // HDR rendering members (protected by mutex)
  mutable QMutex m_stateMutex;
  // Track QObject lifetime to avoid dangling pointers on shutdown.
  // The HDR window is typically owned by the QWidget returned from createWindowContainer().
  QPointer<HDR_WindowType> m_hdrWindow;
  QPointer<QWidget> m_hdrContainer;
  HDRDetectionWorker* m_hdrDetectionWorker;
  bool m_useHDRRendering;
  bool m_isHDRWidgetReady;
  HDRDetection::HDRCapabilities m_hdrCapabilities;
  
  // Helper methods
  void initializeHDRWidget();
  void cleanupHDRResources();

         // Deferred attach helpers to avoid race with UI construction
  void attemptAttachToSplitView();

         // State for deferred attachment
  int  m_attachRetryCount{0};
  bool m_attachScheduled{false};
};
