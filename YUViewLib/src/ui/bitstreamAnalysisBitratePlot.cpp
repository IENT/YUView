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
const int axisMaxValueMargin = 10;
const int tickLength = 5;

const QColor gridLineMajor = QColor(180, 180, 180);
const QColor gridLineMinor = QColor(230, 230, 230);

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

  for (auto axis : std::vector<Axis>{Axis::X, Axis::Y})
  {
    auto &properties = this->propertiesAxis[axis == Axis::X ? 0 : 1];
    properties.startEnd = this->determineAxisStartEnd(axis);

    auto values = this->getAxisValuesToShow(axis);
    drawAxisAndTip(painter, axis);
    drawAxisTicksAndValues(painter, axis, values);
    drawGridLines(painter, axis, values);
  }

  // if (!this->model)
  // {
  //   drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  //   return;
  // }

  // drawTextInCenterOfArea(painter, this->rect(), "Drawing drawing :)");
}

QList<BitrateBarChart::TickValue> BitrateBarChart::getAxisValuesToShow(BitrateBarChart::Axis axis) const
{
  auto properties = this->propertiesAxis[axis == Axis::X ? 0 : 1];
  const QPoint axisVector = (axis == Axis::X) ? QPoint(1, 0) : QPoint(0, -1);
  const auto axisLengthInPixels = QPoint::dotProduct(properties.startEnd.second - properties.startEnd.first, axisVector);
  const auto valueRange = properties.maxValue - properties.minValue;

  const int minPixelDistanceBetweenValues = 50;
  const auto nrValuesToShowMax = double(axisLengthInPixels) / minPixelDistanceBetweenValues;

  auto nrWholeValuesInRange = int(floor(valueRange));

  auto offsetLeft = ceil(properties.minValue) - properties.minValue;
  auto offsetRight = properties.maxValue - floor(properties.maxValue);
  auto rangeRemainder = valueRange - int(floor(valueRange));
  if (offsetLeft < rangeRemainder && offsetRight < rangeRemainder)
    nrWholeValuesInRange++;

  double factorMajor = 1.0;
  while (factorMajor * 10 * nrWholeValuesInRange < nrValuesToShowMax)
    factorMajor *= 10;
  while (factorMajor / 10 * nrWholeValuesInRange > nrValuesToShowMax)
    factorMajor /= 10;

  double factorMinor = factorMajor;
  while (factorMinor * nrWholeValuesInRange * 2 < nrValuesToShowMax)
    factorMinor *= 2;

  auto getValuesForFactor = [properties](double factor)
  {
    QList<double> values;
    int min = ceil(properties.minValue * factor);
    int max = floor(properties.maxValue * factor);
    for (int i = min; i <= max; i++)
      values.append(double(i) / factor);
    return values;
  };

  auto valuesMajor = getValuesForFactor(factorMajor);
  auto valuesMinor = getValuesForFactor(factorMinor);
  
  QList<TickValue> values;
  for (auto v : valuesMinor)
  {
    const bool isMinor = !valuesMajor.contains(v);
    auto pixelPosOnAxis = ((v - properties.minValue) / valueRange) * (axisLengthInPixels - axisMaxValueMargin);
    values.append({v, pixelPosOnAxis, isMinor});
  }
  return values;
}

QPair<QPoint, QPoint> BitrateBarChart::determineAxisStartEnd(Axis axis) const
{
  QRect drawRect;
  if (axis == Axis::X)
  {
    drawRect.setWidth(this->rect().width() - widthAxisY - marginRight);
    drawRect.setHeight(heightAxisX);
    drawRect.moveBottomRight(this->rect().bottomRight() - QPoint(marginRight, 0));

    const QPoint offsetBottom = QPoint(0, drawRect.height() * 3 / 4);
    return {drawRect.bottomLeft() - offsetBottom, drawRect.bottomRight() - offsetBottom};
  }
  else
  {
    drawRect.setWidth(widthAxisY);
    drawRect.setHeight(this->rect().height() - heightAxisX - marginTop);
    drawRect.moveTopLeft(QPoint(0, marginTop));

    const QPoint offsetLeft = QPoint(drawRect.width() / 4, 0);
    return {drawRect.bottomRight() - offsetLeft, drawRect.topRight() - offsetLeft};
  }
}

void BitrateBarChart::drawAxisAndTip(QPainter &painter, Axis axis) const
{
  auto properties = this->propertiesAxis[axis == Axis::X ? 0 : 1];
  auto offsetTip1 = (axis == Axis::X) ? QPoint(-5,  3) : QPoint( 3, 5);
  auto offsetTip2 = (axis == Axis::X) ? QPoint(-5, -3) : QPoint(-3, 5);

  const auto start = properties.startEnd.first;
  const auto end = properties.startEnd.second;
  painter.setPen(QPen(Qt::black, 1));
  painter.drawLine(start, end);
  painter.drawLine(end, end + offsetTip1);
  painter.drawLine(end, end + offsetTip2);
}

void BitrateBarChart::drawAxisTicksAndValues(QPainter &painter, BitrateBarChart::Axis axis, QList<BitrateBarChart::TickValue> &values) const
{
  auto properties = this->propertiesAxis[axis == Axis::X ? 0 : 1];

  const QPoint axisVector = (axis == Axis::X) ? QPoint(1, 0) : QPoint(0, -1);
  const QPoint tickLine = (axis == Axis::X) ? QPoint(0, tickLength) : QPoint(-tickLength, 0);

  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);
  painter.setPen(QPen(Qt::black, 1));
  for (auto v : values)
  {
    //auto pixelPosOnAxis = ((v.value - properties.minValue) / valueRange) * axisLengthInPixels;
    QPointF p = properties.startEnd.first + v.pixelPosOnAxis * axisVector;
    painter.drawLine(p, p + tickLine);

    auto text = QString("%1").arg(v.value);
    auto textSize = metrics.size(0, text);
    QRectF textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(p);
    if (axis == Axis::X)
      textRect.moveTop(p.y() + tickLength + 2);
    else
      textRect.moveRight(p.x() - tickLength - 2);
    
    painter.drawText(textRect, Qt::AlignCenter, text);
  }
}

void BitrateBarChart::drawGridLines(QPainter &painter, BitrateBarChart::Axis axis, QList<BitrateBarChart::TickValue> &values) const
{
  auto thisAxis = this->propertiesAxis[axis == Axis::X ? 0 : 1];
  auto otherAxis = this->propertiesAxis[axis == Axis::X ? 1 : 0];
  
  QPointF drawStart = otherAxis.startEnd.first;
  QPointF drawEnd = otherAxis.startEnd.second;
  for (auto v : values)
  {
    if (axis == Axis::X)
    {
      auto x = v.pixelPosOnAxis + thisAxis.startEnd.first.x();
      drawStart.setX(x);
      drawEnd.setX(x);
    }
    else
    {
      auto y = thisAxis.startEnd.first.y() - v.pixelPosOnAxis;
      drawStart.setY(y);
      drawEnd.setY(y);
    }

    painter.setPen(v.minorTick ? gridLineMinor : gridLineMajor);
    painter.drawLine(drawStart, drawEnd);
  }
}
