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

#include "plotViewWidget.h"

#include <QPainter>
#include <QTextDocument>
#include <cmath>

#include "common/typedef.h"

#define PLOTVIEW_WIDGET_DEBUG_OUTPUT 0
#if PLOTVIEW_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_PLOT(fmt) qDebug() << fmt
#else
#define DEBUG_PLOT(fmt,...) ((void)0)
#endif

const auto marginTop = 5;
const auto marginRight = 5;

const int axisMaxValueMargin = 10;
const int tickLength = 3;
const int tickToTextSpace = 2;
const int fadeBoxThickness = 10;

const QColor gridLineMajor(180, 180, 180);
const QColor gridLineMinor(230, 230, 230);

PlotViewWidget::PlotViewWidget(QWidget *parent)
  : MoveAndZoomableView(parent)
{
  paletteBackgroundColorSettingsTag = "Plot/BackgroundColor";
  this->setMouseTracking(true);

  this->propertiesAxis[0].axis = Axis::X;
  this->propertiesAxis[1].axis = Axis::Y;
}

void PlotViewWidget::setModel(PlotModel *model)
{
  this->model = model;
  this->modelNrStreamsChanged();
  if (this->model)
  {
    this->viewInitializedForModel = false;
    this->initViewFromModel();
    this->connect(model, &PlotModel::dataChanged, this, &PlotViewWidget::modelDataChanged);
    this->connect(model, &PlotModel::nrStreamsChanged, this, &PlotViewWidget::modelNrStreamsChanged);
    this->update();
  }
}

void PlotViewWidget::modelDataChanged()
{
  if (this->model)
  {
    this->initViewFromModel();
    this->update();
  }
}

void PlotViewWidget::modelNrStreamsChanged()
{
  this->showStreamList.clear();
  if (this->model)
  {
    for (unsigned int i = 0; i < model->getNrStreams(); i++)
      this->showStreamList.append(i);
  }
  DEBUG_PLOT("PlotViewWidget::updateStreamInfo showStreamList " << this->showStreamList);
}

void PlotViewWidget::zoomToFitInternal()
{
  if (!this->model)
    return;

  const auto plotXMin = this->convertPixelPosToPlotPos(this->plotRect.bottomLeft(), 1.0).x() - 0.5;
  const auto plotXMax = this->convertPixelPosToPlotPos(this->plotRect.bottomRight(), 1.0).x() + 0.5;
  const auto widthInPlotSpace = plotXMax - plotXMin;

  bool modelContainsDataYet = false;
  double minZoomFactor = -1;
  for (auto streamIndex : this->showStreamList)
  {
    const auto param = this->model->getStreamParameter(streamIndex);

    bool streamContainsData = false;
    for (const auto &plotParam : param.plotParameters)
      streamContainsData |= plotParam.nrpoints > 0;

    if (!streamContainsData)
      continue;

    modelContainsDataYet = true;
    const auto streamXRange = param.xRange.max - param.xRange.min;

    double newZoomFactor = 1.0;
    while (streamXRange * newZoomFactor > widthInPlotSpace)
      newZoomFactor /= ZOOM_STEP_FACTOR;
    if (newZoomFactor == 1.0)
    {
      while (streamXRange * newZoomFactor < widthInPlotSpace)
        newZoomFactor *= ZOOM_STEP_FACTOR;
    }

    if (minZoomFactor == -1)
      minZoomFactor = newZoomFactor;
    else
      minZoomFactor = std::min(minZoomFactor, newZoomFactor);
  }

  if (!modelContainsDataYet)
    return;

  if (minZoomFactor < ZOOMINGLIMIT.min || minZoomFactor > ZOOMINGLIMIT.max)
    return;

  // Zoom the view so that we can see a certain amount of items
  this->setZoomFactor(minZoomFactor);

  // Move the view so that the first value is at the left
  auto visibleRange = this->getVisibleRange(Axis::X);
  if (!visibleRange)
    return;
  
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

  painter.drawText(textRect, Qt::AlignCenter, text);
}

void PlotViewWidget::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  MoveAndZoomableView::updatePaletteIfNeeded();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const auto widgetRect = QRectF(this->rect());

  DEBUG_PLOT("PlotViewWidget::paintEvent widget " << widgetRect);

  this->updatePlotRectAndAxis(painter);
  
  const auto axisRangeX = getAxisRange(Axis::X, this->propertiesAxis[0]);
  const auto axisRangeY = getAxisRange(Axis::Y, this->propertiesAxis[1]);

  const auto ticksX = this->getAxisTicksToShow(Axis::X, axisRangeX);
  const auto ticksY = this->getAxisTicksToShow(Axis::Y, axisRangeY);

  this->drawGridLines(painter, this->propertiesAxis[0], ticksX, axisRangeX);
  this->drawGridLines(painter, this->propertiesAxis[1], ticksY, axisRangeY);

  this->drawLimits(painter);
  this->drawPlot(painter);
  this->drawZoomRect(painter);
  
  this->drawWhiteBoarders(painter, widgetRect);
  this->drawAxis(painter);

  this->drawAxisTicks(painter, this->propertiesAxis[0], ticksX);
  this->drawAxisTicks(painter, this->propertiesAxis[1], ticksY);
  this->drawAxisTickLabels(painter, this->propertiesAxis[0], ticksX, axisRangeX);
  this->drawAxisTickLabels(painter, this->propertiesAxis[1], ticksY, axisRangeY);

  this->drawInfoBox(painter);
  //this->drawDebugBox(painter, plotRect);

  this->drawFadeBoxes(painter, widgetRect);
  this->drawWhiteBoxesInLabelArea(painter, widgetRect);

  if (this->model == nullptr)
  {
    drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  }
}

void PlotViewWidget::mouseMoveEvent(QMouseEvent *mouseMoveEvent)
{
  MoveAndZoomableView::mouseMoveEvent(mouseMoveEvent);

  QMap<unsigned, QMap<unsigned, unsigned>> newHoverePointIndexPerStreamAndPlot;
  if (this->model)
  {
    const auto pos = mouseMoveEvent->pos();
    const auto mouseHoverPos = this->convertPixelPosToPlotPos(pos);
    for (unsigned streamIndex = 0; streamIndex < this->model->getNrStreams(); streamIndex++)
    {
      const auto streamParam = model->getStreamParameter(streamIndex);
      for (unsigned plotIndex = 0; plotIndex < streamParam.getNrPlots(); plotIndex++)
      {
        const auto pointIndex = model->getPointIndex(streamIndex, plotIndex, mouseHoverPos);
        if (pointIndex)
          newHoverePointIndexPerStreamAndPlot[streamIndex][plotIndex] = *pointIndex;
      }
    }
    mouseMoveEvent->accept();
  }

  if (this->currentlyHoveredPointPerStreamAndPlot != newHoverePointIndexPerStreamAndPlot)
  {
    this->currentlyHoveredPointPerStreamAndPlot = newHoverePointIndexPerStreamAndPlot;
    update();
  }
}

void PlotViewWidget::setMoveOffset(QPoint offset)
{
  if (!this->model)
  {
    MoveAndZoomableView::setMoveOffset(offset);
    return;
  }

  auto visibleRange = this->getVisibleRange(Axis::X);
  if (!visibleRange)
  {
    MoveAndZoomableView::setMoveOffset(offset);
    return;
  }
  
  const auto clipLeft = int(-(visibleRange->min) * this->zoomToPixelsPerValueX * this->zoomFactor);
  const auto axisLengthX = this->propertiesAxis[0].line.p2().x() - this->propertiesAxis[0].line.p1().x();
  const auto axisLengthInValues = axisLengthX / this->zoomToPixelsPerValueX / this->zoomFactor;
  const auto clipRight = int(-(visibleRange->max - axisLengthInValues) * this->zoomToPixelsPerValueX * this->zoomFactor);

  QPoint offsetClipped;
  if (axisLengthInValues > (visibleRange->max - visibleRange->min))
    offsetClipped = QPoint(clip(offset.x(), clipLeft, clipRight) , 0);
  else
    offsetClipped = QPoint(clip(offset.x(), clipRight, clipLeft) , 0);
  
  DEBUG_PLOT("PlotViewWidget::setMoveOffset offset " << offset << " clipped " << offsetClipped);
  MoveAndZoomableView::setMoveOffset(offsetClipped);
}

QPoint PlotViewWidget::getMoveOffsetCoordinateSystemOrigin(const QPoint zoomPoint) const
{
  Q_UNUSED(zoomPoint);
  const auto plotRectBottomLeft = this->plotRect.bottomLeft();
  return QPoint(plotRectBottomLeft.x() + fadeBoxThickness, plotRectBottomLeft.y() - fadeBoxThickness);
}

void PlotViewWidget::setZoomFactor(double zoom)
{
  MoveAndZoomableView::setZoomFactor(zoom);
  this->setMoveOffset(this->moveOffset);
}

void PlotViewWidget::drawWhiteBoarders(QPainter &painter, const QRectF &widgetRect) const
{
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(this->plotRect.left(), widgetRect.bottom())));
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(widgetRect.right(), this->plotRect.top())));
  painter.drawRect(QRectF(QPointF(this->plotRect.right(), 0), widgetRect.bottomRight()));
  painter.drawRect(QRectF(QPointF(0, this->plotRect.bottom()), widgetRect.bottomRight()));
}

QList<PlotViewWidget::TickValue> PlotViewWidget::getAxisTicksToShow(const Axis axis, Range<double> visibleRange) const
{
  const auto &properties = this->propertiesAxis[(axis == Axis::X) ? 0 : 1];
  const auto axisVector = (axis == Axis::X) ? QPointF(1, 0) : QPointF(0, -1);
  const auto axisLengthInPixels = QPointF::dotProduct(properties.line.p2() - properties.line.p1(), axisVector);

  const auto rangeWidth = visibleRange.max - visibleRange.min;
  if (rangeWidth == 0)
    return {};

  const int minPixelDistanceBetweenValues = 50;
  const auto nrTicksToShowMax = double(axisLengthInPixels) / minPixelDistanceBetweenValues;

  double factorMajor = 1.0;
  while (factorMajor * 10 * rangeWidth < nrTicksToShowMax)
    factorMajor *= 10;
  while (factorMajor * rangeWidth > nrTicksToShowMax)
    factorMajor /= 10;

  double factorMinor = factorMajor;
  while (factorMinor * rangeWidth * 2 < nrTicksToShowMax)
    factorMinor *= 2;

  DEBUG_PLOT("PlotViewWidget::getAxisTicksToShow rangeWidth " << rangeWidth << " nrTicksToShowMax " << nrTicksToShowMax << " factorMajor " << factorMajor << " factorMinor " << factorMinor);

  /* Get the actual values to show between min and max. However, we want some more values to the left
   * and the right for correct display of the tick labels. Unfortunately we don't know how wide the labels will be
   * so we will just use a worst case assumption and add half the width of the view left and right.
   */
  auto rangeToReturn = visibleRange;
  rangeToReturn.min -= rangeWidth / 2;
  rangeToReturn.max += rangeWidth / 2;
  const auto plotRange = this->getVisibleRange(axis);
  if (plotRange)
  {
    rangeToReturn.min = std::max(rangeToReturn.min, plotRange->min);
    rangeToReturn.max = std::min(rangeToReturn.max, plotRange->max);
  }

  auto getValuesForFactor = [rangeToReturn](double factor)
  {
    QList<double> values;
    int min = std::ceil(rangeToReturn.min * factor);
    int max = std::floor(rangeToReturn.max * factor);
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
    auto graphPos = (axis == Axis::X) ? QPointF(v, 0) : QPointF(0, v);
    auto pixelPosInWidget = this->convertPlotPosToPixelPos(graphPos);
    auto axisValue = (axis == Axis::X) ? pixelPosInWidget.x() : pixelPosInWidget.y();
    values.append({v, axisValue, isMinor});
  }
  return values;
}

void PlotViewWidget::drawAxis(QPainter &painter) const
{
  painter.setPen(QPen(Qt::black, 1));
  painter.drawLine(this->plotRect.bottomLeft(), this->plotRect.topLeft());
  painter.drawLine(this->plotRect.bottomLeft(), this->plotRect.bottomRight());
}

void PlotViewWidget::drawAxisTicks(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QList<PlotViewWidget::TickValue> &ticks) const
{
  if (ticks.isEmpty() || this->model == nullptr)
    return;

  const auto tickLine = (properties.axis == Axis::X) ? QPointF(0, tickLength) : QPointF(-tickLength, 0);

  painter.setPen(QPen(Qt::black, 1));
  for (auto tick : ticks)
  {
    QPointF p = properties.line.p1();
    if (properties.axis == Axis::X)
      p.setX(tick.pixelPosInWidget);
    else
      p.setY(tick.pixelPosInWidget);
    painter.drawLine(p, p + tickLine);
  }
}

void PlotViewWidget::drawAxisTickLabels(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QList<PlotViewWidget::TickValue> &ticks, Range<double> visibleRange) const
{
  if (ticks.isEmpty() || this->model == nullptr)
    return;

  painter.setPen(QPen(Qt::black, 1));
  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);

  // For drawing the labels we extend the visible range by 1/2 left and right because these might still be visible
  const auto visibleRangeWidthHalf = (visibleRange.max - visibleRange.min) / 2;
  visibleRange.min -= visibleRangeWidthHalf;
  visibleRange.max += visibleRangeWidthHalf;
  
  QList<PlotViewWidget::TickValue> ticksToDrawTextFor;
  if (properties.axis == Axis::Y)
  {
    for (auto tick : ticks)
    {
      if (tick.value >= visibleRange.min && tick.value <= visibleRange.max)
        ticksToDrawTextFor.append(tick);
    }
  }
  else
  {
    if (ticks.count() < 2)
    {
      if (ticks[0].value >= visibleRange.min && ticks[0].value <= visibleRange.max)
        ticksToDrawTextFor.append(ticks[0]);
    }
    else
    {
      const auto maxLabelSize = this->getMaxLabelDrawSize(painter, properties.axis, ticks);
      const auto distanceBetweenLabels = 20;
      const double minDistanceBetweenTicks = maxLabelSize.width() + distanceBetweenLabels;
      const double tickDistanceInValues = ticks[1].value - ticks[0].value;
      const auto tickDistanceInPixels = convertPlotPosToPixelPos(QPointF(ticks[1].value, 0)).x() - convertPlotPosToPixelPos(QPointF(ticks[0].value, 0)).x();
      auto drawEveryNthTick = 1;
      while (tickDistanceInPixels * drawEveryNthTick < minDistanceBetweenTicks)
        drawEveryNthTick *= 2;

      for (auto tick : ticks)
      {
        const auto indexOfTickFrom0 = int(std::round(tick.value / tickDistanceInValues));
        const auto isTickVisibleBySubsampling = indexOfTickFrom0 % drawEveryNthTick == 0;
        const auto isTickWithinVisibleRange = (tick.value >= visibleRange.min && tick.value <= visibleRange.max);
        if (isTickVisibleBySubsampling && isTickWithinVisibleRange)
          ticksToDrawTextFor.append(tick);
      }
    }
  }
  
  for (auto tick : ticksToDrawTextFor)
  {
    const auto text = this->model->formatValue(properties.axis, tick.value);
    auto textSize = metrics.size(0, text);
    
    QPointF p = properties.line.p1();
    if (properties.axis == Axis::X)
      p.setX(tick.pixelPosInWidget);
    else
      p.setY(tick.pixelPosInWidget);
    
    QRectF textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(p);
    if (properties.axis == Axis::X)
      textRect.moveTop(p.y() + tickLength + tickToTextSpace);
    else
      textRect.moveRight(p.x() - tickLength - tickToTextSpace);

    painter.drawText(textRect, Qt::AlignCenter, text);
  }
}

void PlotViewWidget::drawGridLines(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QList<TickValue> &ticks, Range<double> visibleRange) const
{
  auto drawStart = (properties.axis == Axis::X) ? this->plotRect.topLeft() : this->plotRect.bottomLeft();
  auto drawEnd = (properties.axis == Axis::X) ? this->plotRect.bottomLeft() : this->plotRect.bottomRight();

  for (auto tick : ticks)
  {
    // The ticks list contains more ticks then we have to draw lines for (because of the labels)
    if (tick.value < visibleRange.min || tick.value > visibleRange.max)
      continue;

    if (properties.axis == Axis::X)
    {
      drawStart.setX(tick.pixelPosInWidget);
      drawEnd.setX(tick.pixelPosInWidget);
    }
    else
    {
      drawStart.setY(tick.pixelPosInWidget);
      drawEnd.setY(tick.pixelPosInWidget);
    }

    painter.setPen(tick.minorTick ? gridLineMinor : gridLineMajor);
    painter.drawLine(drawStart, drawEnd);
  }
}

void PlotViewWidget::drawFadeBoxes(QPainter &painter, const QRectF &widgetRect) const
{
  QLinearGradient gradient;
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  gradient.setCoordinateMode(QGradient::ObjectMode);
#else
  gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
#endif
  gradient.setStart(QPointF(0.0, 0.0));

  painter.setPen(Qt::NoPen);

  auto setGradientBrush = [&gradient, &painter](bool inverse)
  {
    gradient.setColorAt(inverse ? 1 : 0, Qt::white);
    gradient.setColorAt(inverse ? 0 : 1, Qt::transparent);
    painter.setBrush(gradient);
  };

  // Vertival
  gradient.setFinalStop(QPointF(1.0, 0));
  setGradientBrush(false);
  painter.drawRect(QRectF(this->plotRect.left(), 0, fadeBoxThickness, widgetRect.height()));
  setGradientBrush(true);
  painter.drawRect(QRectF(this->plotRect.right(), 0, -fadeBoxThickness, widgetRect.height()));

  // Horizontal
  gradient.setFinalStop(QPointF(0.0, 1.0));
  setGradientBrush(true);
  painter.drawRect(QRectF(0, this->plotRect.bottom(), widgetRect.width(), -fadeBoxThickness));
  setGradientBrush(false);
  painter.drawRect(QRectF(0, this->plotRect.top(), widgetRect.width(), fadeBoxThickness));
}

void PlotViewWidget::drawWhiteBoxesInLabelArea(QPainter &painter, const QRectF &widgetRect) const
{
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  
  painter.drawRect(QRectF(this->plotRect.bottomRight(), widgetRect.bottomRight()));
  painter.drawRect(QRectF(this->plotRect.topLeft(), widgetRect.topLeft()));
  painter.drawRect(QRectF(this->plotRect.bottomLeft(), widgetRect.bottomLeft()));
}

void PlotViewWidget::drawLimits(QPainter &painter) const
{
  const auto plotMin = this->convertPixelPosToPlotPos(this->plotRect.bottomLeft());
  const auto plotMax = this->convertPixelPosToPlotPos(this->plotRect.topRight());

  DEBUG_PLOT("PlotViewWidget::drawLimits");
  for (auto streamIndex : this->showStreamList)
  {
    const auto param = this->model->getStreamParameter(streamIndex);
    for (const auto &limit : param.limits)
    {
      // Only draw visible limits
      QLineF line;
      if (limit.axis == Axis::X)
      {
        if (limit.value < plotMin.x() || limit.value > plotMax.x())
          continue;
        const auto dummyPointForX = this->convertPlotPosToPixelPos(QPointF(limit.value, 0));
        line.setP1(QPointF(dummyPointForX.x(), 0));
        line.setP2(QPointF(dummyPointForX.x(), this->plotRect.height()));
      }
      else
      {
        if (limit.value < plotMin.y() || limit.value > plotMax.y())
          continue;
        const auto dummyPointForY = this->convertPlotPosToPixelPos(QPointF(0, limit.value));
        line.setP1(QPointF(0, dummyPointForY.y()));
        line.setP2(QPointF(this->plotRect.right(), dummyPointForY.y()));

        // Set the QRect where to show the text
        QFont displayFont = painter.font();
        QFontMetrics metrics(displayFont);
        QSize textSize = metrics.size(0, limit.name);

        QRect textRect;
        textRect.setSize(textSize);
        textRect.moveTop(dummyPointForY.y());
        textRect.moveRight(this->plotRect.right() - fadeBoxThickness);
        painter.setPen(Qt::black);
        painter.drawText(textRect, Qt::AlignCenter, limit.name);
      }

      QPen limitPen(Qt::black);
      limitPen.setWidth(2);
      painter.setPen(limitPen);
      painter.drawLine(line);
    }
  }
}

void PlotViewWidget::drawPlot(QPainter &painter) const
{
  if (!this->model)
    return;

  const auto plotXMin = this->convertPixelPosToPlotPos(this->plotRect.bottomLeft()).x() - 0.5;
  const auto plotXMax = this->convertPixelPosToPlotPos(this->plotRect.bottomRight()).x() + 0.5;

  DEBUG_PLOT("PlotViewWidget::drawPlot start");
  for (auto streamIndex : this->showStreamList)
  {
    const auto param = this->model->getStreamParameter(streamIndex);
    for (unsigned int plotIndex = 0; plotIndex < param.getNrPlots(); plotIndex++)
    {
      const auto plotParam = param.plotParameters[plotIndex];
      bool detailedPainting = false;
      if (plotParam.nrpoints > 0)
      {
        const auto firstPoint = model->getPlotPoint(streamIndex, plotIndex, 0);
        if (firstPoint.width * this->zoomFactor > 1)
          detailedPainting = true;
      }

      if (plotParam.type == PlotModel::PlotType::Bar)
      {
        auto setPainterColor = [&painter, &detailedPainting](bool isIntra, bool isHighlight)
        {
          QColor color = isIntra ? QColor(200, 100, 0, 100) : QColor(0, 0, 200, 100);
          if (isHighlight)
            color = color.lighter(150);
          if (detailedPainting)
            painter.setPen(color);
          else
            painter.setPen(Qt::NoPen);
          painter.setBrush(color);
        };

        QVector<QRectF> normalBars;
        QVector<QRectF> intraBars;
        for (unsigned int i = 0; i < plotParam.nrpoints; i++)
        {
          const auto value = model->getPlotPoint(streamIndex, plotIndex, i);

          if (value.x < plotXMin || value.x > plotXMax)
            continue;

          const auto halfWidth = value.width / 2;
          const auto barTopLeft = this->convertPlotPosToPixelPos(QPointF(value.x - halfWidth, value.y));
          const auto barBottomRight = this->convertPlotPosToPixelPos(QPointF(value.x + halfWidth, 0));
          
          const bool isHoveredBar = 
            this->currentlyHoveredPointPerStreamAndPlot.contains(streamIndex) &&
            this->currentlyHoveredPointPerStreamAndPlot[streamIndex].contains(plotIndex) &&
            this->currentlyHoveredPointPerStreamAndPlot[streamIndex][plotIndex] == i;
          
          const auto r = QRectF(barTopLeft, barBottomRight);
          if (isHoveredBar)
          {
            setPainterColor(value.intra, true);
            painter.drawRect(r);
          }
          else
          {
            if (value.intra)
              intraBars.append(r);
            else
              normalBars.append(r);
          }
        }

        DEBUG_PLOT("PlotViewWidget::drawPlot Start drawing " << normalBars.size() << " bars");
        setPainterColor(false, false);
        painter.drawRects(normalBars);

        DEBUG_PLOT("PlotViewWidget::drawPlot Start drawing " << intraBars.size() << " intra bars");
        setPainterColor(true, false);
        painter.drawRects(intraBars);
      }
      else if (plotParam.type == PlotModel::PlotType::Line)
      {
        QPolygonF linePoints;
        QPointF lastPoint;
        for (unsigned i = 0; i < plotParam.nrpoints; i++)
        {
          const auto valueStart = model->getPlotPoint(streamIndex, plotIndex, i);
          const auto linePointStart = this->convertPlotPosToPixelPos(QPointF(valueStart.x, valueStart.y));

          if (valueStart.x < plotXMin)
          {
            lastPoint = linePointStart;
            continue;
          }

          if (linePoints.size() == 0 && i > 0)
            linePoints.append(lastPoint);
          linePoints.append(linePointStart);

          if (valueStart.x > plotXMax)
            // This means that no graph can "go back" on the x-axis. That is ok for now. 
            // If we ever need this, this needs to be smarter here.
            break;

          lastPoint = linePointStart;
        }

        DEBUG_PLOT("PlotViewWidget::drawPlot Start drawing line with " << linePoints.size() << " points");
        QPen linePen(QColor(255, 200, 30));
        linePen.setWidthF(detailedPainting ? 2.0 : 1.0);
        painter.setPen(linePen);
        painter.drawPolyline(linePoints);

        // Draw the currently hovered line in a different color
        if (this->currentlyHoveredPointPerStreamAndPlot.contains(streamIndex) && 
            this->currentlyHoveredPointPerStreamAndPlot[streamIndex].contains(plotIndex))
        {
          const auto index = this->currentlyHoveredPointPerStreamAndPlot[streamIndex][plotIndex];
          if (index > 0)
          {
            const auto valueStart = model->getPlotPoint(streamIndex, plotIndex, index - 1);
            const auto linePointStart = this->convertPlotPosToPixelPos(QPointF(valueStart.x, valueStart.y));
            const auto valueEnd = model->getPlotPoint(streamIndex, plotIndex, index);
            const auto linePointEnd = this->convertPlotPosToPixelPos(QPointF(valueEnd.x, valueEnd.y));

            DEBUG_PLOT("PlotViewWidget::drawPlot Draw hovered line");
            QPen linePen(QColor(50, 50, 200));
            linePen.setWidthF(detailedPainting ? 2.0 : 1.0);
            painter.setPen(linePen);
            painter.drawLine(linePointStart, linePointEnd);
          }
        }

        // if (detailedPainting)
        //   painter.drawEllipse(linePointStart, 2.0, 2.0);
      }
    }
  }
}

void PlotViewWidget::drawInfoBox(QPainter &painter) const
{
  if (!this->model)
    return;

  const auto margin = 6;
  const auto padding = 6;

  QString infoString;
  for (auto streamIndex : this->showStreamList)
  {
    if (!this->currentlyHoveredPointPerStreamAndPlot.contains(streamIndex))
      continue;

    const auto streamParam = this->model->getStreamParameter(streamIndex);
    for (unsigned plotIndex = 0; plotIndex < streamParam.getNrPlots(); plotIndex++)
    {
      if (!this->currentlyHoveredPointPerStreamAndPlot[streamIndex].contains(plotIndex))
        continue;

      const auto pointIndex = this->currentlyHoveredPointPerStreamAndPlot[streamIndex][plotIndex];
      infoString += this->model->getPointInfo(streamIndex, plotIndex, pointIndex);
    }
  }

  // Create a QTextDocument. This object can tell us the size of the rendered text.
  QTextDocument textDocument;
  textDocument.setHtml(infoString);
  textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
  textDocument.setTextWidth(textDocument.size().width());

  // Translate to the position where the text box shall be
  // Consider where the mouse is and move the box out of the way if the mouse is here
  if (!QCursor::pos().isNull())
  {
    const auto mousePos = mapFromGlobal(QCursor::pos());
    auto posX = this->plotRect.bottomRight().x() - axisMaxValueMargin - margin - textDocument.size().width() - padding * 2 + 1;
    auto posY = this->plotRect.bottomRight().y() - axisMaxValueMargin - margin - textDocument.size().height() - padding * 2 + 1;

    const auto margin = 5;
    if (mousePos.x() >= posX - margin && mousePos.x() < posX + textDocument.size().width() + margin)
    {
      posX -= margin * 2 + textDocument.size().width();
      posX = std::max(posX, this->plotRect.left());
    }

    painter.translate(posX, posY);
  }

  // Draw a black rectangle and then the text on top of that
  QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2 * padding, 2 * padding));
  QBrush originalBrush;
  painter.setBrush(QColor(255, 255, 255, 150));
  painter.setPen(Qt::black);
  painter.drawRect(rect);
  painter.translate(padding, padding);
  textDocument.drawContents(&painter);
  painter.setBrush(originalBrush);

  painter.resetTransform();
}

void PlotViewWidget::drawDebugBox(QPainter &painter) const
{
  const int margin = 6;
  const int padding = 6;

  const auto mousePos = mapFromGlobal(QCursor::pos());
  QString infoString = QString("<h4>Debug</h4>"
                   "<table width=\"100%\">"
                   "<tr><td>MoveOffset</td><td align=\"right\">(%1,%2)</td></tr>"
                   "<tr><td>ZoomFactor</td><td align=\"right\">%3</td></tr>"
                   "<tr><td>Mouse</td><td align=\"right\">(%4,%5)</td></tr>"
                   "</table>"
                   ).arg(this->moveOffset.x()).arg(this->moveOffset.y()).arg(this->zoomFactor).arg(mousePos.x()).arg(mousePos.y());

  // Create a QTextDocument. This object can tell us the size of the rendered text.
  QTextDocument textDocument;
  textDocument.setHtml(infoString);
  textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
  textDocument.setTextWidth(textDocument.size().width());

  // Translate to the position where the text box shall be
  painter.translate(this->plotRect.topLeft().x() + axisMaxValueMargin + margin + 1, this->plotRect.topLeft().y() + axisMaxValueMargin + margin + 1);

  // Draw a black rectangle and then the text on top of that
  QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2 * padding, 2 * padding));
  QBrush originalBrush;
  painter.setBrush(QColor(255, 255, 255, 150));
  painter.setPen(Qt::black);
  painter.drawRect(rect);
  painter.translate(padding, padding);
  textDocument.drawContents(&painter);
  painter.setBrush(originalBrush);

  painter.resetTransform();
}

void PlotViewWidget::drawZoomRect(QPainter &painter) const
{
  if (this->viewAction != ViewAction::ZOOM_RECT)
    return;

  if (!this->fixYAxis)
  {
    MoveAndZoomableView::drawZoomRect(painter);
    return;
  }

  auto yTop    = this->plotRect.top() + fadeBoxThickness;
  auto yBottom = this->plotRect.bottom() - fadeBoxThickness;
  const auto mouseRect = QRectF(QPointF(viewZoomingMousePosStart.x(), yTop), QPointF(viewZoomingMousePos.x(), yBottom));

  painter.setPen(ZOOM_RECT_PEN);
  painter.setBrush(ZOOM_RECT_BRUSH);
  painter.drawRect(mouseRect);
}

void PlotViewWidget::updatePlotRectAndAxis(QPainter &painter)
{
  const QPointF thicknessDirectionX(fadeBoxThickness, 0);
  const QPointF thicknessDirectionY(0, fadeBoxThickness);
  const auto widgetRect = QRectF(this->rect());
  
  // Update axis based on the widget rect first (we don't know the plotRect yet)
  this->propertiesAxis[0].line = {widgetRect.bottomLeft() + thicknessDirectionX, widgetRect.bottomRight() - thicknessDirectionX};
  this->propertiesAxis[1].line = {widgetRect.bottomLeft() - thicknessDirectionY, widgetRect.topLeft() + thicknessDirectionY};

  {
    const auto axisRangeX = getAxisRange(Axis::X, this->propertiesAxis[0]);
    const auto axisRangeY = getAxisRange(Axis::Y, this->propertiesAxis[1]);

    const auto ticksX = this->getAxisTicksToShow(Axis::X, axisRangeX);
    const auto ticksY = this->getAxisTicksToShow(Axis::Y, axisRangeY);

    const auto maxLabelSizeX = this->getMaxLabelDrawSize(painter, Axis::X, ticksX);
    const auto maxLabelSizeY = this->getMaxLabelDrawSize(painter, Axis::Y, ticksY);

    const auto marginTopLeft = QPointF(maxLabelSizeY.width() + tickLength + tickToTextSpace, marginTop);
    const auto marginBottomRight = QPointF(marginRight, maxLabelSizeX.height() + tickLength + tickToTextSpace);

    this->plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  }

  {
    // Now we can use the plotRect for the axis update
    this->propertiesAxis[0].line = {this->plotRect.bottomLeft() + thicknessDirectionX, this->plotRect.bottomRight() - thicknessDirectionX};
    this->propertiesAxis[1].line = {this->plotRect.bottomLeft() - thicknessDirectionY, this->plotRect.topLeft() + thicknessDirectionY};
  }
  
  DEBUG_PLOT("PlotViewWidget::updatePlotRectAndAxis plotRect " << this->plotRect << " lineX " << this->propertiesAxis[0].line << " lineY " << this->propertiesAxis[1].line);
}

QPointF PlotViewWidget::convertPlotPosToPixelPos(const QPointF &plotPos, std::optional<double> zoomFactor) const
{
  if (!zoomFactor)
    zoomFactor = this->zoomFactor;

  Range<double> yRange = {0, 100};
  if (this->model)
    yRange = this->model->getStreamParameter(0).yRange;
  const auto rangeY = double(yRange.max - yRange.min);
  
  const auto pixelPosX = this->propertiesAxis[0].line.p1().x() + (plotPos.x() * this->zoomToPixelsPerValueX) * (*zoomFactor) + this->moveOffset.x();
  
  // line.p1 maps to the maximum y value and line.p2 to the lowest value
  if (this->fixYAxis)
  {
    const auto lineLength = this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y();
    const auto relativePos = (plotPos.y() - yRange.min) / rangeY;
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (relativePos * lineLength);
    return {pixelPosX, pixelPosY};
  }
  else
  {
    const auto rangeY = double(yRange.max - yRange.min);
    const auto zoomToPixelsPerValueY = (this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y()) / rangeY;
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (plotPos.y() * zoomToPixelsPerValueY) * (*zoomFactor) + this->moveOffset.y();
    return {pixelPosX, pixelPosY};
  }
}

QPointF PlotViewWidget::convertPixelPosToPlotPos(const QPointF &pixelPos, std::optional<double> zoomFactor) const
{
  if (!zoomFactor)
    zoomFactor = this->zoomFactor;

  Range<double> yRange = {0, 100};
  if (this->model)
    yRange = this->model->getStreamParameter(0).yRange;
  const auto rangeY = double(yRange.max - yRange.min);
  
  const auto valueX = ((pixelPos.x() - this->propertiesAxis[0].line.p1().x() - this->moveOffset.x()) / (*zoomFactor)) / this->zoomToPixelsPerValueX;

  // line.p1 maps to the maximum y value and line.p2 to the lowest value
  if (this->fixYAxis)
  {
    const auto lineLength = this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y();
    const double relativePos = (this->propertiesAxis[1].line.p1().y() - pixelPos.y()) / lineLength;
    const auto valueY = yRange.min + relativePos * rangeY;
    return {valueX, valueY};
  }
  else
  {
    const auto zoomToPixelsPerValueY = (this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y()) / rangeY;
    const auto valueY = ((- pixelPos.y() + this->propertiesAxis[1].line.p1().y() + this->moveOffset.y()) / (*zoomFactor)) / zoomToPixelsPerValueY;
    return {valueX, valueY};
  }
}

void PlotViewWidget::onZoomRectUpdateOffsetAndZoom(QRect zoomRect, double additionalZoomFactor)
{
  const auto newZoom = this->zoomFactor * additionalZoomFactor;
  if (newZoom < ZOOMINGLIMIT.min || newZoom > ZOOMINGLIMIT.max)
    return;

  const auto plotRectBottomLeft = this->plotRect.bottomLeft();
  auto moveOrigin = QPoint(plotRectBottomLeft.x() + fadeBoxThickness, plotRectBottomLeft.y() - fadeBoxThickness);

  const QPoint zoomRectCenterOffset = zoomRect.center() - moveOrigin;
  auto newMoveOffset = ((this->moveOffset - zoomRectCenterOffset) * additionalZoomFactor + this->plotRect.center()).toPoint();
  this->setZoomFactor(newZoom);
  this->setMoveOffset(newMoveOffset);

  DEBUG_PLOT("MoveAndZoomableView::mouseReleaseEvent end zoom box - zoomRectCenterOffset " << zoomRectCenterOffset << " newMoveOffset " << newMoveOffset);
}

std::optional<Range<double>> PlotViewWidget::getVisibleRange(const Axis axis) const
{
  if (this->showStreamList.empty())
    return {};

  Range<double> visibleRange {0, 0};
  for (int i = 0; i < this->showStreamList.size(); i++)
  {
    const auto streamIndex = this->showStreamList.at(i);
    const auto streamParam = this->model->getStreamParameter(streamIndex);
    const auto range = axis == Axis::X ? streamParam.xRange : streamParam.yRange;
    if (i == 0)
      visibleRange = range;
    else
    {
      visibleRange.min = std::min(visibleRange.min, range.min);
      visibleRange.max = std::max(visibleRange.max, range.max);
    }
  }
  return visibleRange;
}

Range<double> PlotViewWidget::getAxisRange(Axis axis, AxisProperties axisProperties) const
{
  Range<double> range;
  const auto lineStartInPlot = this->convertPixelPosToPlotPos(axisProperties.line.p1());
  range.min = (axis == Axis::X) ? lineStartInPlot.x() : lineStartInPlot.y();
  auto lineEndInPlot = this->convertPixelPosToPlotPos(axisProperties.line.p2());
  range.max = (axis == Axis::X) ? lineEndInPlot.x() : lineEndInPlot.y();
  return range;
}

QRectF PlotViewWidget::getMaxLabelDrawSize(QPainter &painter, Axis axis, const QList<TickValue> &ticks) const
{
  if (!this->model)
    return {};

  painter.setPen(QPen(Qt::black, 1));
  painter.setBrush(Qt::NoBrush);
  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);
  QRectF maxSize;
  for (auto tick : ticks)
  {
    const auto text = this->model->formatValue(axis, tick.value);
    auto textSize = metrics.size(0, text);
    if (textSize.width() > maxSize.width())
      maxSize.setWidth(textSize.width());
    if (textSize.height() > maxSize.height())
      maxSize.setHeight(textSize.height());
  }
  return maxSize;
}

void PlotViewWidget::initViewFromModel()
{
  if (this->viewInitializedForModel || !this->model)
    return;

  const auto range = this->model->getReasonabelRangeToShowOnXAxisPer100Pixels();
  if (range)
  {
    this->zoomToPixelsPerValueX = 100.0 / double(*range);
    this->update();
  }
}
