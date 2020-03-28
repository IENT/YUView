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

#include <QtWidgets/QGraphicsItem>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>
#include <QtWidgets/QScrollBar>

class BitrateBarChart : public QWidget
{
  Q_OBJECT

public:
  BitrateBarChart(QWidget *parent = 0);
  void setModel(parserCommon::BitrateItemModel *model);

private:
  QPointer<parserCommon::BitrateItemModel> model;

protected:

  // Override some events from the widget
  void paintEvent(QPaintEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override { Q_UNUSED(event); }
  void mousePressEvent(QMouseEvent *event) override { Q_UNUSED(event); }
  void mouseReleaseEvent(QMouseEvent *event) override { Q_UNUSED(event); }
  void wheelEvent (QWheelEvent *event) override { Q_UNUSED(event); }
  void mouseDoubleClickEvent(QMouseEvent *event) override { Q_UNUSED(event); }
  void keyPressEvent(QKeyEvent *event) override { Q_UNUSED(event); }
  void resizeEvent(QResizeEvent *event) override;

private:
  void drawXAxis(QPainter &painter);
  void drawYAxis(QPainter &painter);

  struct AxisProperties
  {
    double minValue {0};
    double maxValue {1};
    bool showDoubleValues {true};

    // Updated when drawing the axis. For x-axis x values, for y-axis y values.
    int pixelValueOfMinValue {-1};
    int pixelValueOfMaxValue {-1};
  };
  AxisProperties propertiesAxisX;
  AxisProperties propertiesAxisY;

  static QList<double> getAxisValuesToShow(AxisProperties &properties);

  double barsPerWidthOf100Pixels{ 8.0 };
  double maxYValue{ 0 };
};