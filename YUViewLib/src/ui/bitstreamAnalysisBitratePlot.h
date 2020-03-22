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
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCore/QPointer>
#include <QtCharts/QVBarModelMapper>
#include <QtCharts/QVXYModelMapper>
#include <QtWidgets/QWidget>
#include <QtWidgets/QScrollBar>
#include <QtCharts/QValueAxis>

class BitrateBarChartCallout : public QGraphicsItem
{
public:
  BitrateBarChartCallout();
  void setChart(QtCharts::QChart *chart);
  QRectF boundingRect() const override;

  void setTextAndAnchor(const QString &text, QPointF anchor);
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
  QString text;
  QRectF textRect;
  QRectF rect;
  QPointF anchor;
  QtCharts::QChart *chart{ nullptr };
};

class BitrateBarChart : public QWidget
{
  Q_OBJECT

public:
  BitrateBarChart(QWidget *parent = 0);
  void setModel(parserCommon::BitrateItemModel *model);

private slots:
  void onScrollBarValueChanged(int value);
  void tooltip(bool status, int index, QtCharts::QBarSet *barset);
  void onRowsInserted(const QModelIndex &parent, int first, int last);

private:
  QPointer<QtCharts::QChartView> chartView;
  QPointer<QScrollBar> scrollBar;

  QPointer<QtCharts::QVBarModelMapper> barMapper;
  QPointer<QtCharts::QVXYModelMapper> lineModelMapper;
  QPointer<QtCharts::QValueAxis> axisX;
  QPointer<QtCharts::QValueAxis> axisY;
  
  QtCharts::QChart chart;

  QPointer<parserCommon::BitrateItemModel> model;

  BitrateBarChartCallout currentTooltip;

protected:
  void resizeEvent(QResizeEvent *event) override;

  void updateAxis();
  void updateScrollBarRange();

  double barsPerWidthOf100Pixels{ 8.0 };
  double maxYValue{ 0 };
};