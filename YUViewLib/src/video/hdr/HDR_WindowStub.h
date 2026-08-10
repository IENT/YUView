/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

/**
 * @file HDR_WindowStub.h
 * @brief Compile-time stub for HDR_RhiVideoWindow when Qt < 6.4.
 *
 * Official CI still builds on Ubuntu 22.04 (system Qt 6.2). The real QRhi HDR
 * path needs Qt 6.4+. This stub keeps call sites compiling; HDR stays inactive.
 */

#include "HDRDetection.h"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QWindow>

#include <functional>

#include "../yuv/PixelFormatYUV.h"
#include "../yuv/YUVConversionTypes.h"

class HDR_WindowStub : public QWindow
{
  Q_OBJECT

public:
  enum RenderMode {
    Mode_BT2020_PQ_10bit     = 1,
    Mode_BT2020_HLG_10bit    = 2,
    Mode_BT2020_Linear_16bit = 3
  };

  enum OverlaySplitMode {
    OverlaySplitDisabled    = 0,
    OverlaySplitSideBySide  = 1,
    OverlaySplitComparison  = 2
  };

  using OverlayPatchProvider =
      std::function<QImage(const QRect &)>;
  using OverlayYuvPixelProvider =
      std::function<bool(const QPoint &, video::yuv::yuv_t &)>;

  explicit HDR_WindowStub(QWindow *parent = nullptr)
      : QWindow(parent)
  {
  }

  void setRenderMode(RenderMode mode) { m_renderMode = mode; }
  RenderMode getRenderMode() const { return m_renderMode; }

  void setToneMapTargetNits(float nits) { m_toneMapTargetNits = nits; }
  float getToneMapTargetNits() const { return m_toneMapTargetNits; }

  bool isInitialized() const { return false; }
  bool isReadyForRendering() const { return false; }
  bool tryInitialize() { return false; }

  void setHDRCapabilities(const HDRDetection::HDRCapabilities &) {}
  void setBackgroundColor(const QColor &) {}

  void updateFrame(const QImage &) {}
  void updateFrameYUV(const QByteArray &,
                      int,
                      int,
                      const video::yuv::PixelFormatYUV &,
                      video::yuv::ColorConversion)
  {
  }
  void updateFrameYUVMove(QByteArray &&,
                          int,
                          int,
                          const video::yuv::PixelFormatYUV &,
                          video::yuv::ColorConversion)
  {
  }
  void clearFrame() {}

  void setOverlayEnabled(bool) {}
  void setSplitting(OverlaySplitMode, double) {}
  void setGridParams(int, const QColor &) {}
  void setZoomBoxState(bool, const QPoint &, const QPoint &) {}
  void setDrawItemPathAndName(bool, const QString &, const QString &) {}
  void setLoadingFlags(bool, bool) {}
  void setDrawRawValues(bool) {}
  void setPlayingState(bool, bool) {}
  void setCachingIndicatorPixmap(const QPixmap &) {}
  void setOverlayPatchProvider(OverlayPatchProvider) {}
  void setOverlayYuvPixelProvider(OverlayYuvPixelProvider) {}

public slots:
  void updateTransform(double, const QPointF &, double = -1.0) {}

signals:
  void widgetInitialized();

private:
  RenderMode m_renderMode{Mode_BT2020_PQ_10bit};
  float      m_toneMapTargetNits{0.0f};
};
