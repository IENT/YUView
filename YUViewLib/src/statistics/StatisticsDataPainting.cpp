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

#include "StatisticsType.h"
#include "common/functions.h"


#include <QPainter>
#include <QPainterPath>

namespace
{

// Activate this if you want to know when what is loaded.
#define STATISTICS_DEBUG_PAINTING 0
#if STATISTICS_DEBUG_PAINTING && !NDEBUG
#define DEBUG_PAINT qDebug
#else
#define DEBUG_PAINT(fmt, ...) ((void)0)
#endif

QPoint getPolygonCenter(const QPolygon &polygon)
{
  auto p = QPoint(0, 0);
  for (size_t k = 0; k < polygon.count(); k++)
    p += polygon.point(k);
  p /= polygon.count();
  return p;
}

void paintVector(QPainter *                   painter,
                 const stats::StatisticsType &statisticsType,
                 const double &               zoomFactor,
                 const int &                  x1,
                 const int &                  y1,
                 const int &                  x2,
                 const int &                  y2,
                 const float &                vx,
                 const float &                vy,
                 bool                         isLine,
                 const int &                  xMin,
                 const int &                  xMax,
                 const int &                  yMin,
                 const int &                  yMax)
{

  // Is the arrow (possibly) visible?
  if (!(x1 < xMin && x2 < xMin) && !(x1 > xMax && x2 > xMax) && !(y1 < yMin && y2 < yMin) &&
      !(y1 > yMax && y2 > yMax))
  {
    // Set the pen for drawing
    auto vectorPen  = statisticsType.vectorPen;
    auto arrowColor = vectorPen.color();
    if (statisticsType.mapVectorToColor)
      arrowColor.setHsvF(clip((atan2f(vy, vx) + M_PI) / (2 * M_PI), 0.0, 1.0), 1.0, 1.0);
    arrowColor.setAlpha(arrowColor.alpha() * ((float)statisticsType.alphaFactor / 100.0));
    vectorPen.setColor(arrowColor);
    if (statisticsType.scaleVectorToZoom)
      vectorPen.setWidthF(vectorPen.widthF() * zoomFactor / 8);
    painter->setPen(vectorPen);
    painter->setBrush(arrowColor);

    // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or
    // smaller.
    if (zoomFactor > 1)
    {
      // At which angle do we draw the triangle?
      // A vector to the right (1,  0) -> 0°
      // A vector to the top   (0, -1) -> 90°
      const auto angle = qAtan2(vy, vx);

      // Draw the vector head if the vector is not 0,0
      if ((vx != 0 || vy != 0))
      {
        // The size of the arrow head
        const int headSize =
            (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !statisticsType.scaleVectorToZoom)
                ? 8
                : zoomFactor / 2;

        if (statisticsType.arrowHead != stats::StatisticsType::arrowHead_t::none)
        {
          // We draw an arrow head. This means that we will have to draw a shortened line
          const int shorten =
              (statisticsType.arrowHead == stats::StatisticsType::arrowHead_t::arrow)
                  ? headSize * 2
                  : headSize * 0.5;

          if (sqrt(vx * vx * zoomFactor * zoomFactor + vy * vy * zoomFactor * zoomFactor) > shorten)
          {
            // Shorten the line and draw it
            auto vectorLine = QLineF(
                x1, y1, double(x2) - cos(angle) * shorten, double(y2) - sin(angle) * shorten);
            painter->drawLine(vectorLine);
          }
        }
        else
          // Draw the not shortened line
          painter->drawLine(x1, y1, x2, y2);

        if (statisticsType.arrowHead == stats::StatisticsType::arrowHead_t::arrow)
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
        else if (statisticsType.arrowHead == stats::StatisticsType::arrowHead_t::circle)
          painter->drawEllipse(x2 - headSize / 2, y2 - headSize / 2, headSize, headSize);
      }

      if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && statisticsType.renderVectorDataValues)
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
}

} // namespace

void stats::paintStatisticsData(QPainter *             painter,
                                stats::StatisticsData &statisticsData,
                                int                    frameIndex,
                                double                 zoomFactor)
{
  if (statisticsData.getFrameIndex() != frameIndex)
  {
    DEBUG_PAINT("StatisticsData::paintStatistics Frame index was not updated. Use setFrameIndex "
                "first and load data.");
    return;
  }

  // Save the state of the painter. This is restored when the function is done.
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  QRect statRect;
  statRect.setSize(statisticsData.getFrameSize() * zoomFactor);
  statRect.moveCenter(QPoint(0, 0));

  // Get the visible coordinates of the statistics
  auto viewport       = painter->viewport();
  auto worldTransform = painter->worldTransform();
  int  xMin           = statRect.width() / 2 - worldTransform.dx();
  int  yMin           = statRect.height() / 2 - worldTransform.dy();
  int  xMax           = statRect.width() / 2 - (worldTransform.dx() - viewport.width());
  int  yMax           = statRect.height() / 2 - (worldTransform.dy() - viewport.height());

  painter->translate(statRect.topLeft());

  auto statsTypes = statisticsData.getStatisticsTypes();

  // First, get if more than one statistic that has block values is rendered.
  bool moreThanOneBlockStatRendered = false;
  bool oneBlockStatRendered         = false;
  for (const auto &type : statsTypes)
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

  std::unique_lock<std::mutex> lock(statisticsData.accessMutex);

  // Draw all the block types. Also, if the zoom factor is larger than STATISTICS_DRAW_VALUES_ZOOM,
  // also save a list of all the values of the blocks and their position in order to draw the values
  // in the next step.
  QList<QPoint>      drawStatPoints; // The positions of each value
  QList<QStringList> drawStatTexts;  // For each point: The values to draw
  double             maxLineWidth =
      0.0; // The maximum width of the lines that is drawn. This will be used as an offset.

  for (auto it = statsTypes.rbegin(); it != statsTypes.rend(); it++)
  {
    if (!it->render || !statisticsData.hasDataForTypeID(it->typeID))
      continue;

    for (const auto &valueItem : statisticsData[it->typeID].valueData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      auto rect = QRect(valueItem.pos[0], valueItem.pos[1], valueItem.size[0], valueItem.size[1]);
      auto displayRect = QRect(rect.left() * zoomFactor,
                               rect.top() * zoomFactor,
                               rect.width() * zoomFactor,
                               rect.height() * zoomFactor);

      // Check if the rectangle of the statistics item is even visible
      bool rectVisible = (!(displayRect.left() > xMax || displayRect.right() < xMin ||
                            displayRect.top() > yMax || displayRect.bottom() < yMin));
      if (!rectVisible)
        continue;

      int value = valueItem.value; // This value determines the color for this item
      if (it->renderValueData)
      {
        // Get the right color for the item and draw it.
        Color rectColor;
        if (it->scaleValueToBlockSize)
          rectColor =
              it->colorMapper.getColor(float(value) / (valueItem.size[0] * valueItem.size[1]));
        else
          rectColor = it->colorMapper.getColor(value);
        rectColor.setAlpha(rectColor.alpha() * ((float)it->alphaFactor / 100.0));

        auto rectQColor = functions::convertToQColor(rectColor);
        painter->setBrush(rectQColor);
        painter->fillRect(displayRect, rectQColor);
      }

      // optionally, draw a grid around the region
      if (it->renderGrid)
      {
        // Set the grid color (no fill)
        auto gridPen = it->gridPen;
        if (it->scaleGridToZoom)
          gridPen.setWidthF(gridPen.widthF() * zoomFactor);
        painter->setPen(gridPen);
        painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

        // Save the line width (if thicker)
        if (gridPen.widthF() > maxLineWidth)
          maxLineWidth = gridPen.widthF();

        painter->drawRect(displayRect);
      }

      // Save the position/text in order to draw the values later
      if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
      {
        auto valTxt = it->getValueTxt(value);
        if (valTxt.isEmpty() && it->scaleValueToBlockSize)
          valTxt = QString("%1").arg(float(value) / (valueItem.size[0] * valueItem.size[1]));

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
  for (auto it = statsTypes.rbegin(); it != statsTypes.rend(); it++)
  {
    if (!it->render || !statisticsData.hasDataForTypeID(it->typeID))
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the value data
    for (const auto &valueItem : statisticsData[it->typeID].polygonValueData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      auto boundingRect        = valueItem.corners.boundingRect();
      auto trans               = QTransform().scale(zoomFactor, zoomFactor);
      auto displayPolygon      = trans.map(valueItem.corners);
      auto displayBoundingRect = displayPolygon.boundingRect();

      // Check if the rectangle of the statistics item is even visible
      bool isVisible = (!(displayBoundingRect.left() > xMax || displayBoundingRect.right() < xMin ||
                          displayBoundingRect.top() > yMax || displayBoundingRect.bottom() < yMin));

      if (isVisible)
      {
        int value = valueItem.value; // This value determines the color for this item
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

          auto qColor = functions::convertToQColor(color);
          painter->setBrush(qColor);
          painter->fillPath(path, qColor);
        }

        // optionally, draw a grid around the region
        if (it->renderGrid)
        {
          // Set the grid color (no fill)
          auto gridPen = it->gridPen;
          if (it->scaleGridToZoom)
            gridPen.setWidthF(gridPen.widthF() * zoomFactor);
          painter->setPen(gridPen);
          painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

          // Save the line width (if thicker)
          if (gridPen.widthF() > maxLineWidth)
            maxLineWidth = gridPen.widthF();

          painter->drawPolygon(displayPolygon);
        }

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
  }

  // Step three: Draw the values of the block types
  if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM)
  {
    // For every point, draw only one block of values. So for every point, we check if there are
    // also other text entries for the same point and then we draw all of them
    auto lineOffset = QPoint(int(maxLineWidth / 2), int(maxLineWidth / 2));
    for (int i = 0; i < drawStatPoints.count(); i++)
    {
      auto txt      = drawStatTexts[i].join("\n");
      auto textRect = painter->boundingRect(QRect(), Qt::AlignLeft, txt);
      textRect.moveTopLeft(drawStatPoints[i] + QPoint(3, 1) + lineOffset);
      painter->drawText(textRect, Qt::AlignLeft, txt);
    }
  }

  // Draw all the arrows
  for (auto it = statsTypes.rbegin(); it != statsTypes.rend(); it++)
  {
    if (!it->render || !statisticsData.hasDataForTypeID(it->typeID))
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the vector data
    for (const auto &vectorItem : statisticsData[it->typeID].vectorData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto rect =
          QRect(vectorItem.pos[0], vectorItem.pos[1], vectorItem.size[0], vectorItem.size[1]);
      const auto displayRect = QRect(rect.left() * zoomFactor,
                                     rect.top() * zoomFactor,
                                     rect.width() * zoomFactor,
                                     rect.height() * zoomFactor);

      if (it->renderVectorData)
      {
        // Calculate the start and end point of the arrow. The vector starts at center of the block.
        int   x1, y1, x2, y2;
        float vx, vy;
        if (vectorItem.isLine)
        {
          x1 = displayRect.left() + zoomFactor * vectorItem.point[0].x();
          y1 = displayRect.top() + zoomFactor * vectorItem.point[0].y();
          x2 = displayRect.left() + zoomFactor * vectorItem.point[1].x();
          y2 = displayRect.top() + zoomFactor * vectorItem.point[1].y();
          vx = (float)(x2 - x1) / it->vectorScale;
          vy = (float)(y2 - y1) / it->vectorScale;
        }
        else
        {
          x1 = displayRect.left() + displayRect.width() / 2;
          y1 = displayRect.top() + displayRect.height() / 2;

          // The length of the vector
          vx = (float)vectorItem.point[0].x() / it->vectorScale;
          vy = (float)vectorItem.point[0].y() / it->vectorScale;

          // The end point of the vector
          x2 = x1 + zoomFactor * vx;
          y2 = y1 + zoomFactor * vy;
        }

        // Check if the arrow is even visible. The arrow can be visible even though the stat
        // rectangle is not
        const bool arrowVisible = !(x1 < xMin && x2 < xMin) && !(x1 > xMax && x2 > xMax) &&
                                  !(y1 < yMin && y2 < yMin) && !(y1 > yMax && y2 > yMax);
        if (arrowVisible)
        {
          // Set the pen for drawing
          auto vectorPen  = it->vectorPen;
          auto arrowColor = vectorPen.color();
          if (it->mapVectorToColor)
            arrowColor.setHsvF(clip((atan2f(vy, vx) + M_PI) / (2 * M_PI), 0.0, 1.0), 1.0, 1.0);
          arrowColor.setAlpha(arrowColor.alpha() * ((float)it->alphaFactor / 100.0));
          vectorPen.setColor(arrowColor);
          if (it->scaleVectorToZoom)
            vectorPen.setWidthF(vectorPen.widthF() * zoomFactor / 8);
          if (vectorItem.isLine)
            vectorPen.setCapStyle(Qt::RoundCap);
          painter->setPen(vectorPen);
          painter->setBrush(arrowColor);

          // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or
          // smaller.
          if (zoomFactor > 1)
          {
            // At which angle do we draw the triangle?
            // A vector to the right (1,  0) -> 0°
            // A vector to the top   (0, -1) -> 90°
            const auto angle = qAtan2(vy, vx);

            // Draw the vector head if the vector is not 0,0
            if ((vx != 0 || vy != 0))
            {
              // The size of the arrow head
              const int headSize =
                  (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !it->scaleVectorToZoom)
                      ? 8
                      : zoomFactor / 2;

              if (it->arrowHead != StatisticsType::arrowHead_t::none)
              {
                // We draw an arrow head. This means that we will have to draw a shortened line
                const int shorten = (it->arrowHead == StatisticsType::arrowHead_t::arrow)
                                        ? headSize * 2
                                        : headSize * 0.5;
                if (sqrt(vx * vx * zoomFactor * zoomFactor + vy * vy * zoomFactor * zoomFactor) >
                    shorten)
                {
                  // Shorten the line and draw it
                  QLineF vectorLine = QLineF(
                      x1, y1, double(x2) - cos(angle) * shorten, double(y2) - sin(angle) * shorten);
                  painter->drawLine(vectorLine);
                }
              }
              else
                // Draw the not shortened line
                painter->drawLine(x1, y1, x2, y2);

              if (it->arrowHead == StatisticsType::arrowHead_t::arrow)
              {
                // Save the painter state, translate to the arrow tip, rotate the painter and draw
                // the normal triangle.
                painter->save();

                // Draw the arrow tip with fixed size
                painter->translate(QPoint(x2, y2));
                painter->rotate(qRadiansToDegrees(angle));
                const QPoint points[3] = {QPoint(0, 0),
                                          QPoint(-headSize * 2, -headSize),
                                          QPoint(-headSize * 2, headSize)};
                painter->drawPolygon(points, 3);

                // Restore. Revert translation/rotation of the painter.
                painter->restore();
              }
              else if (it->arrowHead == StatisticsType::arrowHead_t::circle)
                painter->drawEllipse(x2 - headSize / 2, y2 - headSize / 2, headSize, headSize);
            }

            if (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && it->renderVectorDataValues)
            {
              if (vectorItem.isLine)
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
      }

      // Check if the rectangle of the statistics item is even visible
      const bool rectVisible = (!(displayRect.left() > xMax || displayRect.right() < xMin ||
                                  displayRect.top() > yMax || displayRect.bottom() < yMin));
      if (rectVisible)
      {
        // optionally, draw a grid around the region that the arrow is defined for
        if (it->renderGrid && rectVisible)
        {
          auto gridPen = it->gridPen;
          if (it->scaleGridToZoom)
            gridPen.setWidthF(gridPen.widthF() * zoomFactor);

          painter->setPen(gridPen);
          painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

          painter->drawRect(displayRect);
        }
      }
    }

    // Go through all the affine transform data
    for (const auto &affineTFItem : statisticsData[it->typeID].affineTFData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      const auto rect = QRect(
          affineTFItem.pos[0], affineTFItem.pos[1], affineTFItem.size[0], affineTFItem.size[1]);
      const auto displayRect = QRect(rect.left() * zoomFactor,
                                     rect.top() * zoomFactor,
                                     rect.width() * zoomFactor,
                                     rect.height() * zoomFactor);
      // Check if the rectangle of the statistics item is even visible
      const bool rectVisible = (!(displayRect.left() > xMax || displayRect.right() < xMin ||
                                  displayRect.top() > yMax || displayRect.bottom() < yMin));
      if (!rectVisible)
        continue;

      if (it->renderVectorData)
      {
        // affine vectors start at bottom left, top left and top right of the block
        // mv0: LT, mv1: RT, mv2: LB
        int   xLTstart, yLTstart, xRTstart, yRTstart, xLBstart, yLBstart;
        int   xLTend, yLTend, xRTend, yRTend, xLBend, yLBend;
        float vxLT, vyLT, vxRT, vyRT, vxLB, vyLB;

        xLTstart = displayRect.left();
        yLTstart = displayRect.top();
        xRTstart = displayRect.right();
        yRTstart = displayRect.top();
        xLBstart = displayRect.left();
        yLBstart = displayRect.bottom();

        // The length of the vectors
        vxLT = (float)affineTFItem.point[0].x() / it->vectorScale;
        vyLT = (float)affineTFItem.point[0].y() / it->vectorScale;
        vxRT = (float)affineTFItem.point[1].x() / it->vectorScale;
        vyRT = (float)affineTFItem.point[1].y() / it->vectorScale;
        vxLB = (float)affineTFItem.point[2].x() / it->vectorScale;
        vyLB = (float)affineTFItem.point[2].y() / it->vectorScale;

        // The end point of the vectors
        xLTend = xLTstart + zoomFactor * vxLT;
        yLTend = yLTstart + zoomFactor * vyLT;
        xRTend = xRTstart + zoomFactor * vxRT;
        yRTend = yRTstart + zoomFactor * vyRT;
        xLBend = xLBstart + zoomFactor * vxLB;
        yLBend = yLBstart + zoomFactor * vyLB;

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
                    xMin,
                    xMax,
                    yMin,
                    yMax);
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
                    xMin,
                    xMax,
                    yMin,
                    yMax);
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
                    xMin,
                    xMax,
                    yMin,
                    yMax);
      }

      // optionally, draw a grid around the region that the arrow is defined for
      if (it->renderGrid && rectVisible)
      {
        auto gridPen = it->gridPen;
        if (it->scaleGridToZoom)
          gridPen.setWidthF(gridPen.widthF() * zoomFactor);

        painter->setPen(gridPen);
        painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

        painter->drawRect(displayRect);
      }
    }
  }

  // Draw all polygon vector data
  for (auto it = statsTypes.rbegin(); it != statsTypes.rend(); it++)
  {
    if (!it->render || !statisticsData.hasDataForTypeID(it->typeID))
      // This statistics type is not rendered or could not be loaded.
      continue;

    // Go through all the vector data
    for (const auto &vectorItem : statisticsData[it->typeID].polygonVectorData)
    {
      // Calculate the size and position of the rectangle to draw (zoomed in)
      auto trans               = QTransform().scale(zoomFactor, zoomFactor);
      auto displayPolygon      = trans.map(vectorItem.corners);
      auto displayBoundingRect = displayPolygon.boundingRect();

      // Check if the rectangle of the statistics item is even visible
      bool isVisible = (!(displayBoundingRect.left() > xMax || displayBoundingRect.right() < xMin ||
                          displayBoundingRect.top() > yMax || displayBoundingRect.bottom() < yMin));
      if (!isVisible)
        continue;

      if (it->renderVectorData)
      {
        // start vector at center of the block
        int   center_x, center_y, head_x, head_y;
        float vx, vy;

        center_x = 0;
        center_y = 0;
        for (const QPoint &point : displayPolygon)
        {
          center_x += point.x();
          center_y += point.y();
        }
        center_x /= displayPolygon.size();
        center_y /= displayPolygon.size();

        // The length of the vector
        vx = (float)vectorItem.point[0].x() / it->vectorScale;
        vy = (float)vectorItem.point[0].y() / it->vectorScale;

        // The end point of the vector
        head_x = center_x + zoomFactor * vx;
        head_y = center_y + zoomFactor * vy;

        // Is the arrow (possibly) visible?
        if (!(center_x < xMin && head_x < xMin) && !(center_x > xMax && head_x > xMax) &&
            !(center_y < yMin && head_y < yMin) && !(center_y > yMax && head_y > yMax))
        {
          // Set the pen for drawing
          auto vectorPen  = it->vectorPen;
          auto arrowColor = vectorPen.color();
          if (it->mapVectorToColor)
            arrowColor.setHsvF(clip((atan2f(vy, vx) + M_PI) / (2 * M_PI), 0.0, 1.0), 1.0, 1.0);
          arrowColor.setAlpha(arrowColor.alpha() * ((float)it->alphaFactor / 100.0));
          vectorPen.setColor(arrowColor);
          if (it->scaleVectorToZoom)
            vectorPen.setWidthF(vectorPen.widthF() * zoomFactor / 8);
          painter->setPen(vectorPen);
          painter->setBrush(arrowColor);

          // Draw the arrow tip, or a circle if the vector is (0,0) if the zoom factor is not 1 or
          // smaller.
          if (zoomFactor > 1)
          {
            // At which angle do we draw the triangle?
            // A vector to the right (1,  0) -> 0°
            // A vector to the top   (0, -1) -> 90°
            const auto angle = qAtan2(vy, vx);

            // Draw the vector head if the vector is not 0,0
            if ((vx != 0 || vy != 0))
            {
              // The size of the arrow head
              const int headSize =
                  (zoomFactor >= STATISTICS_DRAW_VALUES_ZOOM && !it->scaleVectorToZoom)
                      ? 8
                      : zoomFactor / 2;
              if (it->arrowHead != StatisticsType::arrowHead_t::none)
              {
                // We draw an arrow head. This means that we will have to draw a shortened line
                const int shorten = (it->arrowHead == StatisticsType::arrowHead_t::arrow)
                                        ? headSize * 2
                                        : headSize * 0.5;
                if (sqrt(vx * vx * zoomFactor * zoomFactor + vy * vy * zoomFactor * zoomFactor) >
                    shorten)
                {
                  // Shorten the line and draw it
                  auto vectorLine = QLineF(center_x,
                                           center_y,
                                           double(head_x) - cos(angle) * shorten,
                                           double(head_y) - sin(angle) * shorten);
                  painter->drawLine(vectorLine);
                }
              }
              else
                // Draw the not shortened line
                painter->drawLine(center_x, center_y, head_x, head_y);

              if (it->arrowHead == StatisticsType::arrowHead_t::arrow)
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
              else if (it->arrowHead == StatisticsType::arrowHead_t::circle)
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
      if (it->renderGrid && isVisible)
      {
        auto gridPen = it->gridPen;
        if (it->scaleGridToZoom)
          gridPen.setWidthF(gridPen.widthF() * zoomFactor);

        painter->setPen(gridPen);
        painter->setBrush(QBrush(QColor(Qt::color0), Qt::NoBrush)); // no fill color

        painter->drawPolygon(displayPolygon);
      }
    }
  }

  // Restore the state the state of the painter from before this function was called.
  // This will reset the set pens and the translation.
  painter->restore();
}
