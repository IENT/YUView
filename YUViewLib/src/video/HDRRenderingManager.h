#pragma once

#include <QObject>
#include <QString>
#include <QWidget>
#include <QImage>
#include <QMutex>

#include "HDR_VideoWidget.h"
#include "HDRDetectionWorker.h"
#include "HDRDetection.h"
#include "ui/views/SplitViewWidget.h"

class videoHandlerYUV;

/**
 * @brief Manages HDR rendering functionality for video handlers
 * 
 * This class encapsulates all HDR-related operations including:
 * - HDR capability detection
 * - HDR widget creation and management
 * - HDR rendering state management
 * - Communication between HDR components and video handlers
 */
class HDRRenderingManager : public QObject
{
  Q_OBJECT

public:
  explicit HDRRenderingManager(QObject* parent = nullptr);
  ~HDRRenderingManager();

  // HDR state management (thread-safe)
  bool isHDRRenderingActive() const;
  void setHDRRenderingEnabled(bool enabled);
  
  // HDR widget management (thread-safe)
  HDR_VideoWidget* getHDRWidget() const;
  HDR_VideoWidget* createHDRWidget(QWidget* parent = nullptr);
  void hideHDRWidget();
  void showHDRWidget();
  
  // HDR detection (thread-safe)
  void startHDRDetection();
  bool isHDRDetectionInProgress() const;
  HDRDetection::HDRCapabilities getHDRCapabilities() const;
  
  // Frame rendering
  void updateHDRFrame(const QImage& frame);
  QImage getHDRRenderedImage();

signals:
  // HDR rendering state change signals
  void hdrRenderingStateChanged(bool enabled, HDR_VideoWidget* widget);
  void hdrWidgetNeedsDisplay(HDR_VideoWidget* widget, bool show);
  void hdrDetectionCompleted(bool supported);
  void hdrDetectionFailed(const QString& error);

private slots:
  // HDR-related slots
  void onHDRNotSupported(const QString& reason);
  void onHDRModeChanged(HDR_VideoWidget::RenderMode mode);
  void onHDRDetectionComplete(const HDRDetection::HDRCapabilities& capabilities);
  void onHDRDetectionFailed(const QString& error);

private:
  // Thread-safe helper methods
  bool isHDRRenderingActive_locked() const;  // Internal method requiring lock held
  void setHDRWidgetReady_locked(bool ready);  // Internal method requiring lock held
  bool isHDRWidgetReady_locked() const;       // Internal method requiring lock held

  // HDR rendering members (protected by mutex)
  mutable QMutex m_stateMutex;
  HDR_VideoWidget* m_hdrWidget;
  HDRDetectionWorker* m_hdrDetectionWorker;
  bool m_useHDRRendering;
  bool m_isHDRWidgetReady;  // CRITICAL FIX: Track HDR widget readiness state
  HDRDetection::HDRCapabilities m_hdrCapabilities;
  
  // Helper methods
  void initializeHDRWidget();
  void cleanupHDRResources();
};
