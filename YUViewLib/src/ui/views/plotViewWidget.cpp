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

const QPoint marginTopLeft(30, 5);
const QPoint marginBottomRight(5, 30);

const int axisMaxValueMargin = 10;
const int tickLength = 5;
const int fadeBoxThickness = 10;

const QColor gridLineMajor(180, 180, 180);
const QColor gridLineMinor(230, 230, 230);

PlotViewWidget::PlotViewWidget(QWidget *parent)
  : MoveAndZoomableView(parent)
{
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

void PlotViewWidget::zoomToFit(bool checked)
{
  Q_UNUSED(checked);

  if (!this->model)
    return;

  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  this->updateAxis(plotRect);

  bool modelContainsDataYet = false;
  double minZoomFactor = -1;
  for (auto streamIndex : this->showStreamList)
  {
    const auto param = this->model->getStreamParameter(streamIndex);
    for (unsigned int plotIndex = 0; plotIndex < param.getNrPlots(); plotIndex++)
    {
      const auto plotParam = param.plotParameters[plotIndex];
      if (plotParam.nrpoints > 0)
      {
        modelContainsDataYet = true;
        const auto firstPointWidth = model->getPlotPoint(streamIndex, plotIndex, 0).width;
        
        double newZoomFactor = this->zoomFactor;
        if (firstPointWidth * newZoomFactor > 1)
        {
          while (firstPointWidth * newZoomFactor > 1)
            newZoomFactor /= ZOOM_STEP_FACTOR;
        }
        else
        {
          while (firstPointWidth * newZoomFactor < 1)
            newZoomFactor *= ZOOM_STEP_FACTOR;
        }
        if (minZoomFactor == -1)
          minZoomFactor = newZoomFactor;
        else
          minZoomFactor = std::min(minZoomFactor, newZoomFactor);
      }
    }
  }

  if (!modelContainsDataYet)
    return;

  // Zoom the view so that we can see a certain amount of items
  this->setZoomFactor(minZoomFactor);

  // Move the view so that the first value is at the left
  auto visibleRange = this->getVisibleRange(Axis::X);
  if (!visibleRange)
    return;

  this->viewInitializedForModel = true;
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

void PlotViewWidget::paintEvent(QPaintEvent *paint_event)
{
  Q_UNUSED(paint_event);

  QPainter painter(this);

  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);

  DEBUG_PLOT("PlotViewWidget::paintEvent widget " << widgetRect << " plot rect " << plotRect);

  this->updateAxis(plotRect);

  auto valuesX = this->getAxisValuesToShow(Axis::X);
  auto valuesY = this->getAxisValuesToShow(Axis::Y);
  this->drawGridLines(painter, this->propertiesAxis[0], plotRect, valuesX);
  this->drawGridLines(painter, this->propertiesAxis[1], plotRect, valuesY);

  this->drawPlot(painter, plotRect);
  this->drawZoomRect(painter, plotRect);

  this->drawWhiteBoarders(painter, plotRect, widgetRect);
  this->drawAxis(painter, plotRect);

  this->drawAxisTicksAndValues(painter, this->propertiesAxis[0], valuesX);
  this->drawAxisTicksAndValues(painter, this->propertiesAxis[1], valuesY);

  this->drawInfoBox(painter, plotRect);
  //this->drawDebugBox(painter, plotRect);

  this->drawFadeBoxes(painter, plotRect, widgetRect);

  if (!this->model)
  {
    drawTextInCenterOfArea(painter, this->rect(), "Please select an item");
  }
}

void PlotViewWidget::resizeEvent(QResizeEvent *event)
{
  const auto widgetRect = QRectF(this->rect());
  const auto plotRect = QRectF(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  this->updateAxis(plotRect);
  MoveAndZoomableView::resizeEvent(event);
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
        const auto pointIndex = model->getPointIndex(streamIndex, plotIndex, mouseHoverPos.x());
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
  
  const auto clipLeft = int(-(visibleRange->min + 0.5) * this->zoomToPixelsPerValueX * this->zoomFactor);
  const auto axisLengthX = this->propertiesAxis[0].line.p2().x() - this->propertiesAxis[0].line.p1().x();
  const auto axisLengthInValues = axisLengthX / this->zoomToPixelsPerValueX / this->zoomFactor;
  const auto clipRight = int(-(visibleRange->max - axisLengthInValues - 0.5) * this->zoomToPixelsPerValueX * this->zoomFactor);

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
  const auto widgetRect = QRect(this->rect());
  const auto plotRect = QRect(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  const auto plotRectBottomLeft = plotRect.bottomLeft();
  return QPoint(plotRectBottomLeft.x() + fadeBoxThickness, plotRectBottomLeft.y() - fadeBoxThickness);
}

void PlotViewWidget::setZoomFactor(double zoom)
{
  MoveAndZoomableView::setZoomFactor(zoom);
  this->setMoveOffset(this->moveOffset);
}

void PlotViewWidget::drawWhiteBoarders(QPainter &painter, const QRectF &plotRect, const QRectF &widgetRect)
{
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(plotRect.left(), widgetRect.bottom())));
  painter.drawRect(QRectF(QPointF(0, 0), QPointF(widgetRect.right(), plotRect.top())));
  painter.drawRect(QRectF(QPointF(plotRect.right(), 0), widgetRect.bottomRight()));
  painter.drawRect(QRectF(QPointF(0, plotRect.bottom()), widgetRect.bottomRight()));
}

QList<PlotViewWidget::TickValue> PlotViewWidget::getAxisValuesToShow(const PlotViewWidget::Axis axis) const
{
  const auto &properties = this->propertiesAxis[(axis == Axis::X) ? 0 : 1];
  const auto axisVector = (axis == Axis::X) ? QPointF(1, 0) : QPointF(0, -1);
  const auto axisLengthInPixels = QPointF::dotProduct(properties.line.p2() - properties.line.p1(), axisVector);

  double rangeMin, rangeMax;
  {
    const auto lineStartInPlot = this->convertPixelPosToPlotPos(properties.line.p1());
    rangeMin = (axis == Axis::X) ? lineStartInPlot.x() : lineStartInPlot.y();
    auto lineEndInPlot = this->convertPixelPosToPlotPos(properties.line.p2());
    rangeMax = (axis == Axis::X) ? lineEndInPlot.x() : lineEndInPlot.y();
  }
  const auto valueRange = rangeMax - rangeMin;

  const int minPixelDistanceBetweenValues = 50;
  const auto nrValuesToShowMax = double(axisLengthInPixels) / minPixelDistanceBetweenValues;

  auto nrWholeValuesInRange = int(std::floor(valueRange));

  double factorMajor = 1.0;
  while (factorMajor * 10 * nrWholeValuesInRange < nrValuesToShowMax)
    factorMajor *= 10;
  while (factorMajor * nrWholeValuesInRange > nrValuesToShowMax)
    factorMajor /= 10;

  double factorMinor = factorMajor;
  while (factorMinor * nrWholeValuesInRange * 2 < nrValuesToShowMax)
    factorMinor *= 2;

  DEBUG_PLOT("PlotViewWidget::getAxisValuesToShow nrWholeValuesInRange " << nrWholeValuesInRange << " nrValuesToShowMax " << nrValuesToShowMax << " factorMajor " << factorMajor << " factorMinor " << factorMinor);

  auto getValuesForFactor = [rangeMin, rangeMax](double factor)
  {
    QList<double> values;
    int min = std::ceil(rangeMin * factor);
    int max = std::floor(rangeMax * factor);
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

void PlotViewWidget::drawAxis(QPainter &painter, const QRectF &plotRect)
{
  painter.setPen(QPen(Qt::black, 1));
  painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());
  painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());
}

void PlotViewWidget::drawAxisTicksAndValues(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QList<PlotViewWidget::TickValue> &values)
{
  const auto tickLine = (properties.axis == Axis::X) ? QPointF(0, tickLength) : QPointF(-tickLength, 0);

  QFont displayFont = painter.font();
  QFontMetricsF metrics(displayFont);
  painter.setPen(QPen(Qt::black, 1));
  for (auto v : values)
  {
    //auto pixelPosOnAxis = ((v.value - properties.minValue) / valueRange) * axisLengthInPixels;
    QPointF p = properties.line.p1();
    if (properties.axis == Axis::X)
      p.setX(v.pixelPosInWidget);
    else
      p.setY(v.pixelPosInWidget);
    painter.drawLine(p, p + tickLine);

    auto text = QString("%1").arg(v.value);
    auto textSize = metrics.size(0, text);
    QRectF textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(p);
    if (properties.axis == Axis::X)
      textRect.moveTop(p.y() + tickLength + 2);
    else
      textRect.moveRight(p.x() - tickLength - 2);
    
    painter.drawText(textRect, Qt::AlignCenter, text);
  }
}

void PlotViewWidget::drawGridLines(QPainter &painter, const PlotViewWidget::AxisProperties &properties, const QRectF &plotRect, const QList<TickValue> &values)
{
  auto drawStart = (properties.axis == Axis::X) ? plotRect.topLeft() : plotRect.bottomLeft();
  auto drawEnd = (properties.axis == Axis::X) ? plotRect.bottomLeft() : plotRect.bottomRight();

  for (auto v : values)
  {
    if (properties.axis == Axis::X)
    {
      drawStart.setX(v.pixelPosInWidget);
      drawEnd.setX(v.pixelPosInWidget);
    }
    else
    {
      drawStart.setY(v.pixelPosInWidget);
      drawEnd.setY(v.pixelPosInWidget);
    }

    painter.setPen(v.minorTick ? gridLineMinor : gridLineMajor);
    painter.drawLine(drawStart, drawEnd);
  }
}

void PlotViewWidget::drawFadeBoxes(QPainter &painter, const QRectF plotRect, const QRectF &widgetRect)
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

void PlotViewWidget::drawPlot(QPainter &painter, const QRectF &plotRect) const
{
  if (!this->model)
    return;

  const auto plotXMin = this->convertPixelPosToPlotPos(plotRect.bottomLeft()).x() - 0.5;
  const auto plotXMax = this->convertPixelPosToPlotPos(plotRect.bottomRight()).x() + 0.5;

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
        for (unsigned i = 0; i < plotParam.nrpoints; i++)
        {
          const auto valueStart = model->getPlotPoint(streamIndex, plotIndex, i);
          const auto linePointStart = this->convertPlotPosToPixelPos(QPointF(valueStart.x, valueStart.y));

          if (valueStart.x < plotXMin || valueStart.x > plotXMax)
            continue;

          linePoints.append(linePointStart);
        }

        DEBUG_PLOT("PlotViewWidget::drawPlot Start drawing line with " << linePoints.size() << " points");
        QPen linePen(QColor(255, 200, 30));
        linePen.setWidthF(detailedPainting ? 2.0 : 1.0);
        painter.setPen(linePen);
        painter.drawPolyline(linePoints);

        // if (detailedPainting)
        //   painter.drawEllipse(linePointStart, 2.0, 2.0);
      }
    }
  }
}

void PlotViewWidget::drawInfoBox(QPainter &painter, const QRectF &plotRect) const
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
  painter.translate(plotRect.bottomRight().x() - axisMaxValueMargin - margin - textDocument.size().width() - padding * 2 + 1, plotRect.bottomRight().y() - axisMaxValueMargin - margin - textDocument.size().height() - padding * 2 + 1);

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

void PlotViewWidget::drawDebugBox(QPainter &painter, const QRectF &plotRect) const
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
  painter.translate(plotRect.topLeft().x() + axisMaxValueMargin + margin + 1, plotRect.topLeft().y() + axisMaxValueMargin + margin + 1);

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

void PlotViewWidget::drawZoomRect(QPainter &painter, const QRectF plotRect) const
{
  if (this->viewAction != ViewAction::ZOOM_RECT)
    return;

  if (!this->fixYAxis)
  {
    MoveAndZoomableView::drawZoomRect(painter);
    return;
  }

  auto yTop    = plotRect.top() + fadeBoxThickness;
  auto yBottom = plotRect.bottom() - fadeBoxThickness;
  const auto mouseRect = QRectF(QPointF(viewZoomingMousePosStart.x(), yTop), QPointF(viewZoomingMousePos.x(), yBottom));

  painter.setPen(ZOOM_RECT_PEN);
  painter.setBrush(ZOOM_RECT_BRUSH);
  painter.drawRect(mouseRect);
}

void PlotViewWidget::updateAxis(const QRectF &plotRect)
{ 
  const QPointF thicknessDirectionX(fadeBoxThickness, 0);
  this->propertiesAxis[0].line = {plotRect.bottomLeft() + thicknessDirectionX, plotRect.bottomRight() - thicknessDirectionX};
  
  const QPointF thicknessDirectionY(0, fadeBoxThickness);
  this->propertiesAxis[1].line = {plotRect.bottomLeft() - thicknessDirectionY, plotRect.topLeft() + thicknessDirectionY};

  this->zoomToPixelsPerValueY = double(zoomToPixelsPerValueX);
  if (this->model)
  {
    const auto &streamParam = this->model->getStreamParameter(0);
    const auto rangeY = double(streamParam.yRange.max - streamParam.yRange.min);
    this->zoomToPixelsPerValueY = (this->propertiesAxis[1].line.p1().y() - this->propertiesAxis[1].line.p2().y()) / rangeY;
  }
  
  DEBUG_PLOT("PlotViewWidget::updateAxis lineX " << this->propertiesAxis[0].line << " lineY " << this->propertiesAxis[1].line);
}

QPointF PlotViewWidget::convertPlotPosToPixelPos(const QPointF &plotPos) const
{
  const auto pixelPosX = this->propertiesAxis[0].line.p1().x() + (plotPos.x() * this->zoomToPixelsPerValueX) * this->zoomFactor + this->moveOffset.x();
  
  if (this->fixYAxis)
  {
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (plotPos.y() * this->zoomToPixelsPerValueY);
    return {pixelPosX, pixelPosY};
  }
  else
  {
    const auto pixelPosY = this->propertiesAxis[1].line.p1().y() - (plotPos.y() * this->zoomToPixelsPerValueY) * this->zoomFactor + this->moveOffset.y();
    return {pixelPosX, pixelPosY};
  }
}

QPointF PlotViewWidget::convertPixelPosToPlotPos(const QPointF &pixelPos) const
{
  const auto valueX = ((pixelPos.x() - this->propertiesAxis[0].line.p1().x() - this->moveOffset.x()) / this->zoomFactor) / this->zoomToPixelsPerValueX;

  if (this->fixYAxis)
  {
    const auto valueY = (- pixelPos.y() + this->propertiesAxis[1].line.p1().y()) / this->zoomToPixelsPerValueY;
    return {valueX, valueY};
  }
  else
  {
    const auto valueY = ((- pixelPos.y() + this->propertiesAxis[1].line.p1().y() + this->moveOffset.y()) / this->zoomFactor) / this->zoomToPixelsPerValueY;
    return {valueX, valueY};
  }
}

void PlotViewWidget::onZoomRectUpdateOffsetAndZoom(QRect zoomRect, double additionalZoomFactor)
{
  const auto widgetRect = QRect(this->rect());
  const auto plotRect = QRect(marginTopLeft, widgetRect.bottomRight() - marginBottomRight);
  const auto plotRectBottomLeft = plotRect.bottomLeft();
  auto moveOrigin = QPoint(plotRectBottomLeft.x() + fadeBoxThickness, plotRectBottomLeft.y() - fadeBoxThickness);

  const QPoint zoomRectCenterOffset = zoomRect.center() - moveOrigin;
  auto newMoveOffset = (this->moveOffset - zoomRectCenterOffset) * additionalZoomFactor + plotRect.center();
  this->setZoomFactor(this->zoomFactor * additionalZoomFactor);
  this->setMoveOffset(newMoveOffset);

  DEBUG_PLOT("MoveAndZoomableView::mouseReleaseEvent end zoom box - zoomRectCenterOffset " << zoomRectCenterOffset << " newMoveOffset " << newMoveOffset);
}

std::optional<Range<int>> PlotViewWidget::getVisibleRange(const Axis axis) const
{
  if (this->showStreamList.empty())
    return {};

  Range<int> visibleRange {0, 0};
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

void PlotViewWidget::initViewFromModel()
{
  if (this->viewInitializedForModel || !this->model)
    return;

  this->zoomToFit();
}
