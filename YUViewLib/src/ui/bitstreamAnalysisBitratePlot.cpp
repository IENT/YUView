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

#include "bitstreamAnalysisBitratePlot.h"

#include <QtWidgets/QVBoxLayout>
#include <QPainter>

const int widthAxisY = 30;
const int heightAxisX = 30;
const int marginTop = 5;
const int marginRight = 5;

BitrateBarChart::BitrateBarChart(QWidget *parent)
  : QWidget(parent)
{
}

void BitrateBarChart::setModel(parserCommon::BitrateItemModel *model)
{
  this->model = model;
  this->update();
}

void BitrateBarChart::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  this->update();
}

void drawTextInCenterOfArea(QPainter &painter, QRect area, QString text)
{
  // Set the QRect where to show the text
  QFont displayFont = painter.font();
  QFontMetrics metrics(displayFont);
  QSize textSize = metrics.size(0, text);

  QRect textRect;
  textRect.setSize(textSize);
  textRect.moveCenter(area.center());

  // Draw a rectangle around the text in white with a black border
  QRect boxRect = textRect + QMargins(5, 5, 5, 5);
  painter.setPen(QPen(Qt::black, 1));
  painter.fillRect(boxRect,Qt::white);
  painter.drawRect(boxRect);

  // Draw the text
  painter.drawText(textRect, Qt::AlignCenter, text);
}

void BitrateBarChart::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  QPainter painter(this);

  drawXAxis(painter);
  drawYAxis(painter);

  // if (!this->model)
  // {
  //   drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  //   return;
  // }

  // drawTextInCenterOfArea(painter, this->rect(), "Drawing drawing :)");
}

QList<double> BitrateBarChart::getAxisValuesToShow(AxisProperties &properties)
{
  auto axisLengthInPixels = properties.pixelValueOfMaxValue - properties.pixelValueOfMinValue;
  auto valueRange = properties.maxValue - properties.minValue;

  const auto nrValuesToShowMax = double(axisLengthInPixels) / 50;

  auto nrWholeValuesInRange = int(floor(valueRange));

  auto offsetLeft = ceil(properties.minValue) - properties.minValue;
  auto offsetRight = properties.maxValue - floor(properties.maxValue);
  auto rangeRemainder = valueRange - int(floor(valueRange));
  if (offsetLeft < rangeRemainder && offsetRight < rangeRemainder)
    nrWholeValuesInRange++;

  double factor = 1.0;
  while (factor * 10 * nrWholeValuesInRange < nrValuesToShowMax)
    factor *= 10;
  while (factor / 10 * nrWholeValuesInRange > nrValuesToShowMax)
    factor /= 10;

  QList<double> values;
  int min = ceil(properties.minValue * factor);
  int max = floor(properties.maxValue * factor);
  for (int i = min; i <= max; i++)
    values.append(double(i) / factor);
  return values;
}

void BitrateBarChart::drawXAxis(QPainter &painter)
{
  QRect drawRect;
  drawRect.setWidth(this->rect().width() - widthAxisY - marginRight);
  drawRect.setHeight(heightAxisX);
  drawRect.moveBottomRight(this->rect().bottomRight() - QPoint(marginRight, 0));

  // Line
  const QPoint offsetBottom = QPoint(0, drawRect.height() * 3 / 4);
  const QPoint axisStart = drawRect.bottomLeft() - offsetBottom;
  const QPoint axisEnd = drawRect.bottomRight() - offsetBottom;
  painter.drawLine(axisStart, axisEnd);

  // Tip
  painter.drawLine(axisEnd, axisEnd + QPoint(-5,  3));
  painter.drawLine(axisEnd, axisEnd + QPoint(-5, -3));

  this->propertiesAxisX.pixelValueOfMinValue = axisStart.x();
  this->propertiesAxisX.pixelValueOfMaxValue = axisEnd.x() - 8;

  auto values = getAxisValuesToShow(this->propertiesAxisX);
  const auto valueRange = this->propertiesAxisX.maxValue - this->propertiesAxisX.minValue;
  const auto pixelRange = axisEnd.x() - axisStart.x();
  QFont displayFont = painter.font();
  QFontMetrics metrics(displayFont);
  painter.setPen(QPen(Qt::black, 1));
  for (auto v : values)
  {
    int pixelPosX = int(((v - this->propertiesAxisX.minValue) / valueRange) * pixelRange);
    QPoint p = axisStart + QPoint(pixelPosX, 0);
    painter.drawLine(p, p + QPoint(0, 5));

    QString text = QString("%1").arg(v);
    QSize textSize = metrics.size(0, text);
    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(p);
    textRect.moveTop(p.y() + 6);

    painter.drawText(textRect, Qt::AlignCenter, text);
  }
}

void BitrateBarChart::drawYAxis(QPainter &painter)
{
  QRect drawRect;
  drawRect.setWidth(widthAxisY);
  drawRect.setHeight(this->rect().height() - heightAxisX - marginTop);
  drawRect.moveTopLeft(QPoint(0, marginTop));

  // Line
  const QPoint offsetLeft = QPoint(drawRect.width() / 4, 0);
  const QPoint axisStart = drawRect.bottomRight() - offsetLeft;
  const QPoint axisEnd = drawRect.topRight() - offsetLeft;
  painter.drawLine(axisStart, axisEnd + QPoint(0, 5));

  // Tip
  painter.drawLine(axisEnd, axisEnd - QPoint(-3, 5));
  painter.drawLine(axisEnd, axisEnd - QPoint( 3, 5));

  this->propertiesAxisY.pixelValueOfMinValue = axisStart.y();
  this->propertiesAxisY.pixelValueOfMaxValue = axisEnd.y() - 8;
}
