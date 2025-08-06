#pragma once

#include <QObject>
#include <QString>
#include <QWidget>
#include <QImage>

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

  // HDR state management
  bool isHDRRenderingActive() const { return m_useHDRRendering; }
  void setHDRRenderingEnabled(bool enabled);
  
  // HDR widget management
  HDR_VideoWidget* getHDRWidget() const { return m_hdrWidget; }
  HDR_VideoWidget* createHDRWidget(QWidget* parent = nullptr);
  void hideHDRWidget();
  void showHDRWidget();
  
  // HDR detection
  void startHDRDetection();
  bool isHDRDetectionInProgress() const;
  const HDRDetection::HDRCapabilities& getHDRCapabilities() const { return m_hdrCapabilities; }
  
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
  // HDR rendering members
  HDR_VideoWidget* m_hdrWidget;
  HDRDetectionWorker* m_hdrDetectionWorker;
  bool m_useHDRRendering;
  HDRDetection::HDRCapabilities m_hdrCapabilities;
  
  // Helper methods
  void initializeHDRWidget();
  void cleanupHDRResources();
};
