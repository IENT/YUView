/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "StatisticsDataPainting.h"

#include <common/FunctionsGui.h>
#include <statistics/StatisticsType.h>

#include <QPainter>
#include <QPainterPath>
#include <QtGui/QPolygon>
#include <QtMath>
#include <cmath>

namespace stats
{

namespace
{

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_PAINTING 0
#if STATISTICS_DEBUG_PAINTING && !NDEBUG
#define DEBUG_PAINT qDebug
#else
#define DEBUG_PAINT(fmt, ...) ((void)0)
#endif

QPolygon convertToQPolygon(const Polygon &poly)
{
  if (poly.empty())
    return QPolygon();

  auto qPoly = QPolygon(int(poly.size()));
  for (int i = 0; i < int(poly.size()); i++)
    qPoly.setPoint(i, QPoint(poly[i].x, poly[i].y));
  return qPoly;
}

QPoint getPolygonCenter(const QPolygon &polygon)
{
  auto p = QPoint(0, 0);
  for (int k = 0; k < polygon.count(); k++)
    p += polygon.point(k);
  p /= polygon.count();
  return p;
}

Qt::PenStyle patternToQPenStyle(Pattern &pattern)
{
  if (pattern == Pattern::Solid)
    return Qt::SolidLine;
  if (pattern == Pattern::Dash)
    return Qt::DashLine;
  if (pattern == Pattern::Dot)
    return Qt::DotLine;
  if (pattern == Pattern::DashDot)
    return Qt::DashDotLine;
  if (pattern == Pattern::DashDotDot)
    return Qt::DashDotDotLine;
  return Qt::SolidLine;
}

QPen styleToPen(LineDrawStyle &style)
{
  return QPen(functionsGui::toQColor(style.color), style.width, patternToQPenStyle(style.pattern));
}

struct VisibleLimits
{
  Range<int> xRange;
  Range<int> yRange;
};

void paintVector(QPainter *            painter,
                 const StatisticsType &type,
                 const double &        zoomFactor,
                 const int &           x1,
                 const int &           y1,
                 const int &           x2,
                 const int &           y2,
                 const float &         vx,
                 const float &         vy,
                 bool                  isLine,
                 const VisibleLimits & visibleLimits)
{
  const bool arrowDefinitelyInvisible =
      (x1 < visibleLimits.xRange.min && x2 < visibleLimits.xRange.min) ||
      (x1 > visibleLimits.xRange.max && x2 > visibleLimits.xRange.max) ||
      (y1 < visibleLimits.yRange.min && y2 < visibleLimits.yRange.min) ||
      (y1 > visibleLimits.yRange.max && y2 > visibleLimits.yRange.max);
  if (arrowDefinitelyInvisible)
    return;

  // Set the pen for drawing
  auto vectorStyle = type.vectorStyle;
  auto arrowColor  = functionsGui::toQColor(vectorStyle.color);
  if (type.mapVectorToColor)
    arrowColor.setHsvF(
        functions::clip((std::atan2(vy, vx) + M_PI) / (2 * M_PI), 0.0, 1.0), 1.0, 1.0);
  arrowColor.setAlpha(arrowColor.alpha() * ((float)type.alphaFactor / 100.0));

  if (type.scaleVectorToZoom)
    vectorStyle.width = vectorStyle.width * zoomFactor / 8;

  painter->setPen(QPen(arrowColor, vectorStyle.width, patternToQPenStyle(vectorStyle.pattern)));
  painter->setBrush(arrowColor);

  // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or
  // smaller.
  if (zoomFactor > 1)
  {
    // At which angle do we draw the triangle?
    // A vector to the right (1,  0) -> 0°
    // A vector to the top   (0, -1) -> 90°
    const auto angle = std::atan2(vy, vx);

    // Draw the vector head if the vector is not 0,0
    if ((vx != 0 || vy != 0))
    {
      // The size of the arrow head
      const int headSize = (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !type.scaleVectorToZoom)
                               ? 8
                               : zoomFactor / 2;

      if (type.arrowHead != StatisticsType::ArrowHead::none)
      {
        // We draw an arrow head. This means that we will have to draw a shortened line
        const int shorten =
            (type.arrowHead == StatisticsType::ArrowHead::arrow) ? headSize * 2 : headSize * 0.5;

        if (std::sqrt(vx * vx * zoomFactor * zoomFactor + vy * vy * zoomFactor * zoomFactor) >
            shorten)
        {
          // Shorten the line and draw it
          auto vectorLine = QLineF(x1,
                                   y1,
                                   double(x2) - std::cos(angle) * shorten,
                                   double(y2) - std::sin(angle) * shorten);
          painter->drawLine(vectorLine);
        }
      }
      else
        // Draw the not shortened line
        painter->drawLine(x1, y1, x2, y2);

      if (type.arrowHead == StatisticsType::ArrowHead::arrow)
      {
        // Save the painter state, translate to the arrow tip, rotate the painter and draw the
        // normal triangle.
        painter->save();

        // Draw the arrow tip with fixed size
        painter->translate(QPoint(x2, y2));
        painter->rotate(qRadiansToDegrees(angle));
        const QPoint points[3] = {
            QPoint(0, 0), QPoint(-headSize * 2, -headSize), QPoint(-headSize * 2, headSize)};
        painter->drawPolygon(points, 3);

        // Restore. Revert translation/rotation of the painter.
        painter->restore();
      }
      else if (type.arrowHead == StatisticsType::ArrowHead::circle)
        painter->drawEllipse(x2 - headSize / 2, y2 - headSize / 2, headSize, headSize);
    }

    if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && type.renderVectorDataValues)
    {
      if (isLine)
      {
        // if we just draw a line, we want to simply see the coordinate pairs
        auto txt1 = QString("(%1, %2)").arg(x1 / zoomFactor).arg(y1 / zoomFactor);
        auto txt2 = QString("(%1, %2)").arg(x2 / zoomFactor).arg(y2 / zoomFactor);

        auto textRect1 = painter->boundingRect(QRect(), Qt::AlignLeft, txt1);
        auto textRect2 = painter->boundingRect(QRect(), Qt::AlignLeft, txt2);

        textRect1.moveCenter(QPoint(x1, y1));
        textRect2.moveCenter(QPoint(x2, y2));

        // as angle = atan2(y2-y1, x2-x1) move txt accordingly
        int a = qRadiansToDegrees(angle);
        if (a < 45 && a > -45)
        {
          textRect1.moveRight(x1);
          textRect2.moveLeft(x2);
        }
        else if (a <= -45 && a > -135)
        {
          textRect1.moveTop(y1);
          textRect2.moveBottom(y2);
        }
        else if (a >= 45 && a < 135)
        {
          textRect1.moveBottom(y1);
          textRect2.moveTop(y2);
        }
        else
        {
          textRect1.moveLeft(x1);
          textRect2.moveRight(x2);
        }

        painter->drawText(textRect1, Qt::AlignLeft, txt1);
        painter->drawText(textRect2, Qt::AlignLeft, txt2);
      }
      else
      {
        // Also draw the vector value next to the arrow head
        auto txt      = QString("x %1\ny %2").arg(vx).arg(vy);
        auto textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
        textRect.moveCenter(QPoint(x2, y2));
        int a = qRadiansToDegrees(angle);
        if (a < 45 && a > -45)
          textRect.moveLeft(x2);
        else if (a <= -45 && a > -135)
          textRect.moveBottom(y2);
        else if (a >= 45 && a < 135)
          textRect.moveTop(y2);
        else
          textRect.moveRight(x2);
        painter->drawText(textRect, Qt::AlignLeft, txt);
      }
    }
  }
  else
  {
    // No arrow head is drawn. Only draw a line.
    painter->drawLine(x1, y1, x2, y2);
  }
}

bool isRectVisible(const QRect &rect, const VisibleLimits &visibleLimits)
{
  return rect.left() < visibleLimits.xRange.max && rect.right() > visibleLimits.xRange.min &&
         rect.top() < visibleLimits.yRange.max && rect.bottom() > visibleLimits.yRange.min;
}

void drawRegionGrid(QPainter *            painter,
                    const StatisticsType &type,
                    double                zoomFactor,
                    const QRect &         rect)
{
  if (!type.renderGrid)
    return;

  auto gridStyle = type.gridStyle;
  if (type.scaleGridToZoom)
    gridStyle.width = gridStyle.width * zoomFactor;

  painter->setPen(styleToPen(gridStyle));
  painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color
  painter->drawRect(rect);
}

void drawRegionGrid(QPainter *            painter,
                    const StatisticsType &type,
                    double                zoomFactor,
                    const QPolygon &      polygon)
{
  if (!type.renderGrid)
    return;

  auto gridStyle = type.gridStyle;
  if (type.scaleGridToZoom)
    gridStyle.width = gridStyle.width * zoomFactor;

  painter->setPen(styleToPen(gridStyle));
  painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color
  painter->drawPolygon(polygon);
}

double determineMaxRenderedLineWidth(const StatisticsTypes &types, double zoomFactor)
{
  double maxLineWidth = 0;
  for (const auto &type : types)
  {
    const auto width = type.gridStyle.width * zoomFactor;
    if (type.render && width > maxLineWidth)
      maxLineWidth = width;
  }
  return maxLineWidth;
}

} // namespace

void paintStatisticsData(QPainter *             painter,
                         const DataPerTypeMap & data,
                         const StatisticsTypes &types,
                         Size                   frameSize,
                         double                 zoomFactor)
{
  // Save the state of the painter. This is restored when the function is done.
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  QRect statRect;
  statRect.setSize(QSize(frameSize.width * zoomFactor, frameSize.height * zoomFactor));
  statRect.moveCenter(QPoint(0, 0));

  // Get the visible coordinates of the statistics
  auto          viewport       = painter->viewport();
  auto          worldTransform = painter->worldTransform();
  VisibleLimits visibleLimits;
  {
    auto xMin            = int(statRect.width() / 2 - worldTransform.dx());
    auto xMax            = int(statRect.width() / 2 - (worldTransform.dx() - viewport.width()));
    visibleLimits.xRange = {xMin, xMax};

    auto yMin            = int(statRect.height() / 2 - worldTransform.dy());
    auto yMax            = int(statRect.height() / 2 - (worldTransform.dy() - viewport.height()));
    visibleLimits.yRange = {yMin, yMax};
  }

  painter->translate(statRect.topLeft());

  // First, get if more than one statistic that has block values is rendered.
  bool moreThanOneBlockStatRendered = false;
  bool oneBlockStatRendered         = false;
  for (const auto &type : types)
  {
    if (type.render && type.hasValueData)
    {
      if (oneBlockStatRendered)
      {
        moreThanOneBlockStatRendered = true;
        break;
      }
      else
        oneBlockStatRendered = true;
    }
  }

  // Draw all the block types. Also, if the zoom factor is larger than STATISTICS_DRAW_VALUES_ZOOM,
  // also save a list of all the values of the blocks and their position in order to draw the values
  // in the next step.
  QList<QPoint>      drawStatPoints; // The positions of each value
  QList<QStringList> drawStatTexts;  // For each point: The values to draw

  for (auto it = types.crbegin(); it != types.crend(); it++)
  {
    if (!it->render || data.count(it->typeID) == 0)
      continue;

    for (const auto &valueItem : data.at(it->typeID).valueData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto displayRect = QRect(valueItem.pos.x * zoomFactor,
                                     valueItem.pos.y * zoomFactor,
                                     valueItem.width * zoomFactor,
                                     valueItem.height * zoomFactor);

      if (!isRectVisible(displayRect, visibleLimits))
        continue;

      int value = valueItem.value; // This value determines the color for this item
      if (it->renderValueData)
      {
        // Get the right color for the item and draw it.
        Color rectColor;
        if (it->scaleValueToBlockSize)
          rectColor = it->colorMapper.getColor(float(value) / (valueItem.width * valueItem.height));
        else
          rectColor = it->colorMapper.getColor(value);
        rectColor.setAlpha(rectColor.alpha() * ((float)it->alphaFactor / 100.0));

        auto rectQColor = functionsGui::toQColor(rectColor);
        painter->setBrush(rectQColor);
        painter->fillRect(displayRect, rectQColor);
      }

      if (isRectVisible(displayRect, visibleLimits))
        drawRegionGrid(painter, *it, zoomFactor, displayRect);

      // Save the position/text in order to draw the values later
      if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
      {
        auto valTxt = it->getValueTxt(value);
        if (valTxt.isEmpty() && it->scaleValueToBlockSize)
          valTxt = QString("%1").arg(float(value) / (valueItem.width * valueItem.height));

        auto typeTxt = it->typeName;
        auto statTxt = moreThanOneBlockStatRendered ? typeTxt + ":" + valTxt : valTxt;

        int i = drawStatPoints.indexOf(displayRect.topLeft());
        if (i == -1)
        {
          // No value for this point yet. Append it and start a new QStringList
          drawStatPoints.append(displayRect.topLeft());
          drawStatTexts.append(QStringList(statTxt));
        }
        else
          // There is already a value for this point. Just append the text.
          drawStatTexts[i].append(statTxt);
      }
    }
  }

  // Draw all the polygon value types. Also, if the zoom factor is larger than
  // STATISTICS_DRAW_VALUES_ZOOM, also save a list of all the values of the blocks and their
  // position in order to draw the values in the next step. QList<QPoint> drawStatPoints;       //
  // The positions of each value QList<QStringList> drawStatTexts;   // For each point: The values
  // to draw double maxLineWidth = 0.0;          // Also get the maximum width of the lines that is
  // drawn. This will be used as an offset.
  for (auto it = types.crbegin(); it != types.crend(); it++)
  {
    if (!it->render || data.count(it->typeID) == 0)
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the value data
    for (const auto &valueItem : data.at(it->typeID).polygonValueData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      auto valuePoly           = convertToQPolygon(valueItem);
      auto boundingRect        = valuePoly.boundingRect();
      auto trans               = QTransform().scale(zoomFactor, zoomFactor);
      auto displayPolygon      = trans.map(valuePoly);
      auto displayBoundingRect = displayPolygon.boundingRect();

      if (!isRectVisible(displayBoundingRect, visibleLimits))
        continue;

      const int value = valueItem.value; // This value determines the color for this item
      if (it->renderValueData)
      {
        // Get the right color for the item and draw it.
        Color color;
        if (it->scaleValueToBlockSize)
          color = it->colorMapper.getColor(
              float(value) / (boundingRect.size().width() * boundingRect.size().height()));
        else
          color = it->colorMapper.getColor(value);
        color.setAlpha(color.alpha() * ((float)it->alphaFactor / 100.0));

        // Fill polygon
        QPainterPath path;
        path.addPolygon(displayPolygon);

        auto qColor = functionsGui::toQColor(color);
        painter->setBrush(qColor);
        painter->fillPath(path, qColor);
      }

      drawRegionGrid(painter, *it, zoomFactor, displayPolygon);

      // Todo: draw text for polygon statistics
      // // Save the position/text in order to draw the values later
      if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
      {
        auto valTxt  = it->getValueTxt(value);
        auto typeTxt = it->typeName;
        auto statTxt = moreThanOneBlockStatRendered ? typeTxt + ":" + valTxt : valTxt;

        int i = drawStatPoints.indexOf(getPolygonCenter(displayPolygon));
        if (i == -1)
        {
          // No value for this point yet. Append it and start a new QStringList
          drawStatPoints.append(getPolygonCenter(displayPolygon));
          drawStatTexts.append(QStringList(statTxt));
        }
        else
          // There is already a value for this point. Just append the text.
          drawStatTexts[i].append(statTxt);
      }
    }
  }

  // Step three: Draw the values of the block types
  if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
  {
    // For every point, draw only one block of values. So for every point, we check if there are
    // also other text entries for the same point and then we draw all of them
    const auto maxLineWidth = determineMaxRenderedLineWidth(types, zoomFactor);
    const auto lineOffset   = QPoint(int(maxLineWidth / 2), int(maxLineWidth / 2));
    for (int i = 0; i < drawStatPoints.count(); i++)
    {
      const auto txt      = drawStatTexts[i].join("\n");
      auto       textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
      textRect.moveTopLeft(drawStatPoints[i] + QPoint(3, 1) + lineOffset);
      painter->drawText(textRect, Qt::AlignLeft, txt);
    }
  }

  // Draw all the vector/line/affine data
  for (auto it = types.crbegin(); it != types.crend(); it++)
  {
    if (!it->render || data.count(it->typeID) == 0)
      // This statistics type is not rendered or could not be loaded.
      continue;

    for (const auto &vectorItem : data.at(it->typeID).vectorData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto displayRect = QRect(vectorItem.pos.x * zoomFactor,
                                     vectorItem.pos.y * zoomFactor,
                                     vectorItem.width * zoomFactor,
                                     vectorItem.height * zoomFactor);

      if (it->renderVectorData)
      {
        // Calculate the start and end point of the arrow. The vector starts at center of the block.
        const auto x1 = displayRect.left() + displayRect.width() / 2;
        const auto y1 = displayRect.top() + displayRect.height() / 2;

        // The length of the vector
        const auto vx = double(vectorItem.vector.x / it->vectorScale);
        const auto vy = double(vectorItem.vector.y / it->vectorScale);

        // The end point of the vector
        const auto x2 = int(x1 + zoomFactor * vx);
        const auto y2 = int(y1 + zoomFactor * vy);

        const bool isLine = false;
        paintVector(painter, *it, zoomFactor, x1, y1, x2, y2, vx, vy, isLine, visibleLimits);
      }

      if (isRectVisible(displayRect, visibleLimits))
        drawRegionGrid(painter, *it, zoomFactor, displayRect);
    }

    for (const auto &lineItem : data.at(it->typeID).lineData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto displayRect = QRect(lineItem.pos.x * zoomFactor,
                                     lineItem.pos.y * zoomFactor,
                                     lineItem.width * zoomFactor,
                                     lineItem.height * zoomFactor);

      if (it->renderVectorData)
      {
        // Calculate the start and end point of the arrow. The vector starts at center of the block.
        auto x1 = int(displayRect.left() + zoomFactor * lineItem.line.p1.x);
        auto y1 = int(displayRect.top() + zoomFactor * lineItem.line.p1.y);
        auto x2 = int(displayRect.left() + zoomFactor * lineItem.line.p2.x);
        auto y2 = int(displayRect.top() + zoomFactor * lineItem.line.p2.y);
        auto vx = double(x2 - x1) / it->vectorScale;
        auto vy = double(y2 - y1) / it->vectorScale;

        const bool isLine = true;
        paintVector(painter, *it, zoomFactor, x1, y1, x2, y2, vx, vy, isLine, visibleLimits);
      }

      if (isRectVisible(displayRect, visibleLimits))
        drawRegionGrid(painter, *it, zoomFactor, displayRect);
    }

    for (const auto &affineTFItem : data.at(it->typeID).affineTFData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto displayRect = QRect(affineTFItem.pos.x * zoomFactor,
                                     affineTFItem.pos.y * zoomFactor,
                                     affineTFItem.width * zoomFactor,
                                     affineTFItem.height * zoomFactor);
      if (!isRectVisible(displayRect, visibleLimits))
        continue;

      if (it->renderVectorData)
      {
        // affine vectors start at bottom left, top left and top right of the block
        // mv0: LT, mv1: RT, mv2: LB

        const auto xLTstart = displayRect.left();
        const auto yLTstart = displayRect.top();
        const auto xRTstart = displayRect.right();
        const auto yRTstart = displayRect.top();
        const auto xLBstart = displayRect.left();
        const auto yLBstart = displayRect.bottom();

        // The length of the vectors
        const auto vxLT = double(affineTFItem.point[0].x) / it->vectorScale;
        const auto vyLT = double(affineTFItem.point[0].y) / it->vectorScale;
        const auto vxRT = double(affineTFItem.point[1].x) / it->vectorScale;
        const auto vyRT = double(affineTFItem.point[1].y) / it->vectorScale;
        const auto vxLB = double(affineTFItem.point[2].x) / it->vectorScale;
        const auto vyLB = double(affineTFItem.point[2].y) / it->vectorScale;

        // The end point of the vectors
        const auto xLTend = int(xLTstart + zoomFactor * vxLT);
        const auto yLTend = int(yLTstart + zoomFactor * vyLT);
        const auto xRTend = int(xRTstart + zoomFactor * vxRT);
        const auto yRTend = int(yRTstart + zoomFactor * vyRT);
        const auto xLBend = int(xLBstart + zoomFactor * vxLB);
        const auto yLBend = int(yLBstart + zoomFactor * vyLB);

        paintVector(painter,
                    *it,
                    zoomFactor,
                    xLTstart,
                    yLTstart,
                    xLTend,
                    yLTend,
                    vxLT,
                    vyLT,
                    false,
                    visibleLimits);
        paintVector(painter,
                    *it,
                    zoomFactor,
                    xRTstart,
                    yRTstart,
                    xRTend,
                    yRTend,
                    vxRT,
                    vyRT,
                    false,
                    visibleLimits);
        paintVector(painter,
                    *it,
                    zoomFactor,
                    xLBstart,
                    yLBstart,
                    xLBend,
                    yLBend,
                    vxLB,
                    vyLB,
                    false,
                    visibleLimits);
      }

      if (isRectVisible(displayRect, visibleLimits))
        drawRegionGrid(painter, *it, zoomFactor, displayRect);
    }
  }

  // Draw all polygon vector data
  for (auto it = types.crbegin(); it != types.crend(); it++)
  {
    if (!it->render || data.count(it->typeID) == 0)
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the vector data
    for (const auto &polygonWithVector : data.at(it->typeID).polygonVectorData)
    {
      if (polygonWithVector.size() < 3)
        continue; // need at least triangle -- or more corners

      // Calculate the size and position of the rectangle to draw (zoomed in)
      auto vectorPoly          = convertToQPolygon(polygonWithVector);
      auto trans               = QTransform().scale(zoomFactor, zoomFactor);
      auto displayPolygon      = trans.map(vectorPoly);
      auto displayBoundingRect = displayPolygon.boundingRect();

      if (!isRectVisible(displayBoundingRect, visibleLimits))
        continue;

      if (it->renderVectorData)
      {
        // start vector at center of the block
        int center_x = 0;
        int center_y = 0;
        for (const QPoint &point : displayPolygon)
        {
          center_x += point.x();
          center_y += point.y();
        }
        center_x /= displayPolygon.size();
        center_y /= displayPolygon.size();

        // The length of the vector
        auto vx = double(polygonWithVector.vector.x) / it->vectorScale;
        auto vy = double(polygonWithVector.vector.y) / it->vectorScale;

        // The end point of the vector
        auto head_x = int(center_x + zoomFactor * vx);
        auto head_y = int(center_y + zoomFactor * vy);

        // Is the arrow (possibly) visible?
        if (!(center_x < visibleLimits.xRange.min && head_x < visibleLimits.xRange.min) &&
            !(center_x > visibleLimits.xRange.max && head_x > visibleLimits.xRange.max) &&
            !(center_y < visibleLimits.yRange.min && head_y < visibleLimits.yRange.min) &&
            !(center_y > visibleLimits.yRange.max && head_y > visibleLimits.yRange.max))
        {
          // Set the pen for drawing
          auto vectorStyle = it->vectorStyle;
          auto arrowColor  = functionsGui::toQColor(vectorStyle.color);
          if (it->mapVectorToColor)
            arrowColor.setHsvF(
                functions::clip((std::atan2(vy, vx) + M_PI) / (2 * M_PI), 0.0, 1.0), 1.0, 1.0);
          arrowColor.setAlpha(arrowColor.alpha() * ((float)it->alphaFactor / 100.0));
          if (it->scaleVectorToZoom)
            vectorStyle.width = vectorStyle.width * zoomFactor / 8;

          painter->setPen(
              QPen(arrowColor, vectorStyle.width, patternToQPenStyle(vectorStyle.pattern)));
          painter->setBrush(arrowColor);

          // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or
          // smaller.
          if (zoomFactor > 1)
          {
            // At which angle do we draw the triangle?
            // A vector to the right (1,  0) -> 0°
            // A vector to the top   (0, -1) -> 90°
            const auto angle = std::atan2(vy, vx);

            // Draw the vector head if the vector is not 0,0
            if ((vx != 0 || vy != 0))
            {
              // The size of the arrow head
              const int headSize =
                  (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !it->scaleVectorToZoom)
                      ? 8
                      : zoomFactor / 2;
              if (it->arrowHead != StatisticsType::ArrowHead::none)
              {
                // We draw an arrow head. This means that we will have to draw a shortened line
                const int shorten = (it->arrowHead == StatisticsType::ArrowHead::arrow)
                                        ? headSize * 2
                                        : headSize * 0.5;
                if (std::sqrt(vx * vx * zoomFactor * zoomFactor +
                              vy * vy * zoomFactor * zoomFactor) > shorten)
                {
                  // Shorten the line and draw it
                  auto vectorLine = QLineF(center_x,
                                           center_y,
                                           double(head_x) - std::cos(angle) * shorten,
                                           double(head_y) - std::sin(angle) * shorten);
                  painter->drawLine(vectorLine);
                }
              }
              else
                // Draw the not shortened line
                painter->drawLine(center_x, center_y, head_x, head_y);

              if (it->arrowHead == StatisticsType::ArrowHead::arrow)
              {
                // Save the painter state, translate to the arrow tip, rotate the painter and draw
                // the normal triangle.
                painter->save();

                // Draw the arrow tip with fixed size
                painter->translate(QPoint(head_x, head_y));
                painter->rotate(qRadiansToDegrees(angle));
                const QPoint points[3] = {QPoint(0, 0),
                                          QPoint(-headSize * 2, -headSize),
                                          QPoint(-headSize * 2, headSize)};
                painter->drawPolygon(points, 3);

                // Restore. Revert translation/rotation of the painter.
                painter->restore();
              }
              else if (it->arrowHead == StatisticsType::ArrowHead::circle)
                painter->drawEllipse(
                    head_x - headSize / 2, head_y - headSize / 2, headSize, headSize);
            }

            // Todo
            // if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM &&
            // it->renderVectorDataValues)
            // {
            //   // Also draw the vector value next to the arrow head
            //     QString txt = QString("x %1\ny %2").arg(vx).arg(vy);
            //     QRect textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
            //     textRect.moveCenter(QPoint(head_x,head_y));
            //     int a = qRadiansToDegrees(angle);
            //     if (a < 45 && a > -45)
            //       textRect.moveLeft(head_x);
            //     else if (a <= -45 && a > -135)
            //       textRect.moveBottom(head_y);
            //     else if (a >= 45 && a < 135)
            //       textRect.moveTop(head_y);
            //     else
            //       textRect.moveRight(head_x);
            //     painter->drawText(textRect, Qt::AlignLeft, txt);

            // }
          }
          else
          {
            // No arrow head is drawn. Only draw a line.
            painter->drawLine(center_x, center_y, head_x, head_y);
          }
        }
      }

      // optionally, draw the polygon outline
      if (it->renderGrid)
      {
        auto gridStyle = it->gridStyle;
        if (it->scaleGridToZoom)
          gridStyle.width = gridStyle.width * zoomFactor;

        painter->setPen(styleToPen(gridStyle));
        painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

        painter->drawPolygon(displayPolygon);
      }
    }
  }

  // Restore the state the state of the painter from before this function was called.
  // This will reset the set pens and the translation.
  painter->restore();
}

} // namespace stats
