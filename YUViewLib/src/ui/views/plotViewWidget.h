/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "MoveAndZoomableView.h"
#include "plotModel.h"

class PlotViewWidget : public MoveAndZoomableView
{
  Q_OBJECT

public:
  PlotViewWidget(QWidget *parent = 0);
  void setModel(PlotModel *model);

private slots:
  void modelDataChanged();
  void modelNrStreamsChanged();

protected:

  // Override some events from the widget
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

  void   setMoveOffset(QPointF offset) override;
  QPoint getMoveOffsetCoordinateSystemOrigin(const QPointF zoomPoint = {}) const override;
  void   setZoomFactor(double zoom) override;
  
private:

  struct TickValue
  {
    double value;
    double pixelPosInWidget;
    bool minorTick;
  };

  struct ValueRange
  {
    double min {0};
    double max {1};
  };

  struct AxisProperties
  {
    Axis axis;
    
    // In the widgets pixel domain. The first point marks the 0 point on the axis.
    // This is updated every time the size of the widget changes
    QLineF line;
    bool showDoubleValues {true};
  };
  AxisProperties propertiesAxis[2];

  QRectF plotRect;

  QList<TickValue> getAxisTicksToShow(const Axis axis, Range<double> visibleRange) const;
  void drawWhiteBoarders(QPainter &painter, const QRectF &widgetRect) const;
  void drawAxis(QPainter &painter) const;
  void drawAxisTicks(QPainter &painter, const AxisProperties &properties, const QList<TickValue> &ticks) const;
  void drawAxisTickLabels(QPainter &painter, const AxisProperties &properties, const QList<TickValue> &ticks, Range<double> visibleRange) const;
  void drawGridLines(QPainter &painter, const AxisProperties &properties, const QList<TickValue> &ticks, Range<double> visibleRange) const;
  void drawFadeBoxes(QPainter &painter, const QRectF &widgetRect) const;
  void drawWhiteBoxesInLabelArea(QPainter &painter, const QRectF &widgetRect) const;

  void updatePlotRectAndAxis(QPainter &painter);

  void drawLimits(QPainter &painter) const;
  void drawPlot(QPainter &painter) const;
  void drawInfoBox(QPainter &painter) const;
  void drawDebugBox(QPainter &painter) const;
  void drawZoomRect(QPainter &painter) const;

  // Convert a position in the 2D coordinate system of the plot into a pixel position and vise versa
  QPointF convertPlotPosToPixelPos(const QPointF &plotPos, std::optional<double> zoomFactor = {}) const;
  QPointF convertPixelPosToPlotPos(const QPointF &pixelPos, std::optional<double> zoomFactor = {}) const;

  void zoomToFitInternal() override;
  void onZoomRectUpdateOffsetAndZoom(QRectF zoomRect, double additionalZoomFactor) override;

  std::optional<Range<double>> getVisibleRange(const Axis axis) const;
  Range<double> getAxisRange(Axis axis, AxisProperties axisProperties) const;
  QRectF getMaxLabelDrawSize(QPainter &painter, Axis axis, const QList<TickValue> &ticks) const;

  PlotModel *model {nullptr};

  // At zoom 1.0 (no zoom) we will show values with this distance on the x axis
  double zoomToPixelsPerValueX {10.0};

  QMap<unsigned, QMap<unsigned, unsigned>> currentlyHoveredPointPerStreamAndPlot;

  QList<unsigned int> showStreamList;

  void initViewFromModel();
  bool viewInitializedForModel {false};

  bool fixYAxis {true};
};
