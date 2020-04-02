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

#include "parser/parserCommon.h"

#include <QAction>
#include <QtWidgets/QGraphicsItem>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QScrollBar>

#include "plotModel.h"
#include "dummyPlotModel.h"

class PlotWidget : public QWidget
{
  Q_OBJECT

public:
  PlotWidget(QWidget *parent = 0);
  void setModel(PlotModel *model);

protected:

  // Override some events from the widget
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override { event->ignore(); }
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override { event->ignore(); }
  void wheelEvent (QWheelEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override { event->ignore(); }
  void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
  void resizeEvent(QResizeEvent *event) override;

  virtual bool event(QEvent *event) override;

private:
  enum class Axis
  {
    X,
    Y
  };

  struct TickValue
  {
    double value;
    double pixelPosOnAxis;
    bool minorTick;
  };

  struct AxisProperties
  {
    double minValue {0};
    double maxValue {1};
    bool showDoubleValues {true};

    QLineF line;
  };
  AxisProperties propertiesAxis[2];

  static QLineF getAxisLine(const Axis axis, const QRectF plotRect);
  static QList<TickValue> getAxisValuesToShow(const Axis axis, const AxisProperties &properties);
  static void drawWhiteBoarders(QPainter &painter, const QRectF &plotRect, const QRectF &widgetRect);
  static void drawAxis(QPainter &painter, const QRectF &plotRect);
  static void drawAxisTicksAndValues(QPainter &painter, const Axis axis, const AxisProperties &properties, const QList<TickValue> &values);
  static void drawGridLines(QPainter &painter, const Axis axis, const AxisProperties &propertiesThis, const QRectF &plotRect, const QList<TickValue> &values);
  static void drawFadeBoxes(QPainter &painter, const QRectF plotRect, const QRectF &widgetRect);

  void drawPlot(QPainter &painter, QRectF plotRect) const;

  double zoomFactor {1.0}; //!< The current zoom factor in two dimensions
  enum class ZoomDirection {BOTH, HORIZONTAL};
  ZoomDirection zoomDirection {ZoomDirection::HORIZONTAL};
  enum class ZoomMode {IN, OUT, TO_VALUE};
  void zoom(ZoomMode zoomMode, const QPoint &zoomPoint = QPoint(), double newZoomFactor = 0.0);

  QPoint moveOffset; //!< The offset of the view

  PlotModel *model {nullptr};
  DummyPlotModel dummyModel;
};
