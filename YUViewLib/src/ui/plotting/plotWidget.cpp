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

#include "plotWidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QPainter>
#include <cmath>

const QPoint marginTopLeft(30, 5);
const QPoint marginBottomRight(5, 30);

const int axisMaxValueMargin = 10;
const int tickLength = 5;
const int fadeBoxThickness = 10;

const QColor gridLineMajor(180, 180, 180);
const QColor gridLineMinor(230, 230, 230);

PlotWidget::PlotWidget(QWidget *parent)
  : QWidget(parent)
{
}

void PlotWidget::setModel(parserCommon::BitrateItemModel *model)
{
  this->model = model;
  this->update();
}

void PlotWidget::resizeEvent(QResizeEvent *event)
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

void PlotWidget::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  QPainter painter(this);

  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);

  auto valuesX = this->getAxisValuesToShow(Axis::X, this->propertiesAxis[0]);
  auto valuesY = this->getAxisValuesToShow(Axis::Y, this->propertiesAxis[1]);
  drawGridLines(painter, Axis::X, this->propertiesAxis[0], plotRect, valuesX);
  drawGridLines(painter, Axis::Y, this->propertiesAxis[1], plotRect, valuesY);

  if (false)
  {
    // DEBUG draw plot
    painter.setBrush(Qt::blue);
    painter.setPen(Qt::NoPen);
    painter.drawRect(widgetRect);
  }

  drawWhiteBoarders(painter, plotRect, widgetRect);
  drawAxis(painter, plotRect);

  this->propertiesAxis[0].line = getAxisLine(Axis::X, plotRect);
  this->propertiesAxis[1].line = getAxisLine(Axis::Y, plotRect);

  drawAxisTicksAndValues(painter, Axis::X, this->propertiesAxis[0], valuesX);
  drawAxisTicksAndValues(painter, Axis::Y, this->propertiesAxis[1], valuesY);

  drawFadeBoxes(painter, plotRect, widgetRect);

  // if (!this->model)
  // {
  //   drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  //   return;
  // }

  // drawTextInCenterOfArea(painter, this->rect(), "Drawing drawing :)");
}

void PlotWidget::drawWhiteBoarders(QPainter &painter, const QRectF &plotRect, const QRectF &widgetRect)
{
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(plotRect.left(), widgetRect.bottom())));
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(widgetRect.right(), plotRect.top())));
  painter.drawRect(QRectF(QPointF(plotRect.right(), 0), widgetRect.bottomRight()));
  painter.drawRect(QRectF(QPointF(0, plotRect.bottom()), widgetRect.bottomRight()));
}

QLineF PlotWidget::getAxisLine(const PlotWidget::Axis axis, const QRectF plotRect)
{
  QLineF line;
  if (axis == Axis::X)
  {
    QPointF thicknessDirection(fadeBoxThickness, 0);
    line = {plotRect.bottomLeft() + thicknessDirection, plotRect.bottomRight() - thicknessDirection};
  }
  else
  {
    QPointF thicknessDirection(0, fadeBoxThickness);
    line = {plotRect.bottomLeft() - thicknessDirection, plotRect.topLeft() + thicknessDirection};
  }
  return line;
}

QList<PlotWidget::TickValue> PlotWidget::getAxisValuesToShow(const PlotWidget::Axis axis, const PlotWidget::AxisProperties &properties)
{
  const auto axisVector = (axis == Axis::X) ? QPointF(1, 0) : QPointF(0, -1);
  const auto axisLengthInPixels = QPointF::dotProduct(properties.line.p2() - properties.line.p1(), axisVector);
  const auto valueRange = properties.maxValue - properties.minValue;

  const int minPixelDistanceBetweenValues = 50;
  const auto nrValuesToShowMax = double(axisLengthInPixels) / minPixelDistanceBetweenValues;

  auto nrWholeValuesInRange = int(std::floor(valueRange));

  auto offsetLeft = std::ceil(properties.minValue) - properties.minValue;
  auto offsetRight = properties.maxValue - std::floor(properties.maxValue);
  auto rangeRemainder = valueRange - int(std::floor(valueRange));
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
    int min = std::ceil(properties.minValue * factor);
    int max = std::floor(properties.maxValue * factor);
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

void PlotWidget::drawAxis(QPainter &painter, const QRectF &plotRect)
{
  painter.setPen(QPen(Qt::black, 1));
  painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());
  painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
}

void PlotWidget::drawAxisTicksAndValues(QPainter &painter, const PlotWidget::Axis axis, const AxisProperties &properties, const QList<PlotWidget::TickValue> &values)
{
  const auto axisVector = (axis == Axis::X) ? QPointF(1, 0) : QPointF(0, -1);
  const auto tickLine = (axis == Axis::X) ? QPointF(0, tickLength) : QPointF(-tickLength, 0);

  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);
  painter.setPen(QPen(Qt::black, 1));
  for (auto v : values)
  {
    //auto pixelPosOnAxis = ((v.value - properties.minValue) / valueRange) * axisLengthInPixels;
    QPointF p = properties.line.p1() + v.pixelPosOnAxis * axisVector;
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

void PlotWidget::drawGridLines(QPainter &painter, const Axis axis, const AxisProperties &propertiesThis, const QRectF &plotRect, const QList<TickValue> &values)
{
  auto drawStart = (axis == Axis::X) ? plotRect.topLeft() : plotRect.bottomLeft();
  auto drawEnd = (axis == Axis::X) ? plotRect.bottomLeft() : plotRect.bottomRight();

  for (auto v : values)
  {
    if (axis == Axis::X)
    {
      auto x = v.pixelPosOnAxis + propertiesThis.line.p1().x();
      drawStart.setX(x);
      drawEnd.setX(x);
    }
    else
    {
      auto y = propertiesThis.line.p1().y() - v.pixelPosOnAxis;
      drawStart.setY(y);
      drawEnd.setY(y);
    }

    painter.setPen(v.minorTick ? gridLineMinor : gridLineMajor);
    painter.drawLine(drawStart, drawEnd);
  }
}

void PlotWidget::drawFadeBoxes(QPainter &painter, const QRectF plotRect, const QRectF &widgetRect)
{
  QLinearGradient gradient;
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  gradient.setCoordinateMode(QGradient::ObjectMode);
#else
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
#endif
  gradient.setStart(QPointF(0.0, 0.0));

  painter.setPen(Qt::NoPen);

  auto setGradientBrush = [&gradient, &painter](bool inverse) {
    gradient.setColorAt(inverse ? 1 : 0, Qt::white);
    gradient.setColorAt(inverse ? 0 : 1, Qt::transparent);
    painter.setBrush(gradient);
  };

  // Vertival
  gradient.setFinalStop(QPointF(1.0, 0));
  setGradientBrush(false);
  painter.drawRect(QRectF(plotRect.left(), 0, fadeBoxThickness, widgetRect.height()));
  setGradientBrush(true);
  painter.drawRect(QRectF(plotRect.right(), 0, -fadeBoxThickness, widgetRect.height()));

  // Horizontal
  gradient.setFinalStop(QPointF(0.0, 1.0));
  setGradientBrush(true);
  painter.drawRect(QRectF(0, plotRect.bottom(), widgetRect.width(), -fadeBoxThickness));
  setGradientBrush(false);
  painter.drawRect(QRectF(0, plotRect.top(), widgetRect.width(), fadeBoxThickness));
}
