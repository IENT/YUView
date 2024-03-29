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

#include "ShowColorFrame.h"

#include <common/FunctionsGui.h>

#include <QPainter>

ShowColorWidget::ShowColorWidget(QWidget *parent) : QFrame(parent)
{
}

void ShowColorWidget::paintEvent(QPaintEvent *event)
{
  QFrame::paintEvent(event);

  QPainter painter(this);
  auto     fw       = this->frameWidth();
  auto     r        = this->rect();
  auto     drawRect = QRect(r.left() + fw, r.top() + fw, r.width() - fw * 2, r.height() - fw * 2);

  // Get the min/max values from the color map
  const auto minVal = this->colMapper.valueRange.min;
  const auto maxVal = this->colMapper.valueRange.max;

  if (this->renderRangeValues)
  {
    // How high is one digit when drawing it?
    QFontMetrics metrics(this->font());
    auto         h = metrics.size(0, "0").height();

    // Draw two small lines (at the left and at the right)
    const int lineHeight = 3;
    int       y          = drawRect.height() - h;
    painter.drawLine(drawRect.left(), y, drawRect.left(), y - lineHeight);
    painter.drawLine(drawRect.right(), y, drawRect.right(), y - lineHeight);

    // Draw the text values left and right
    painter.drawText(
        drawRect.left(), y, drawRect.width(), h, Qt::AlignLeft, QString::number(minVal));
    painter.drawText(
        drawRect.left(), y, drawRect.width(), h, Qt::AlignRight, QString::number(maxVal));
    // Draw the middle value
    auto middleValue = (maxVal - minVal) / 2 + minVal;
    if (middleValue != minVal && middleValue != maxVal)
    {
      // Where (x coordinate) to draw the middle value
      auto xPos      = drawRect.width() / 2;
      auto drawWidth = drawRect.width();
      if ((maxVal - minVal) % 2 == 1)
      {
        // The difference is uneven. The middle value is not in the exact middle of the interval.
        auto step = drawRect.width() / (maxVal - minVal);
        xPos      = drawRect.left() + int(step * middleValue);
        drawWidth = step * 2 * middleValue;
      }
      painter.drawLine(xPos, y, xPos, y - lineHeight);
      painter.drawText(
          drawRect.left(), y, drawWidth, h, Qt::AlignHCenter, QString::number(middleValue));
    }

    // Modify the draw rect, so that the actual range is drawn over the values
    drawRect.setHeight(drawRect.height() - h - lineHeight);
  }

  if (this->renderRange)
  {
    // Split the rect into lines with width of 1 pixel
    const auto y0 = drawRect.bottom();
    const auto y1 = drawRect.top();
    for (auto x = drawRect.left(); x <= drawRect.right(); x++)
    {
      // For every line (1px width), draw a line.
      // Set the right color

      auto xRel   = double(x) / (drawRect.right() - drawRect.left()); // 0...1
      auto xRange = minVal + (double(maxVal - minVal)) * xRel;

      auto c = this->colMapper.getColor(xRange);
      if (this->isEnabled())
        painter.setPen(functionsGui::toQColor(c));
      else
      {
        auto gray = 64 + c.gray() / 2;
        painter.setPen(QColor(gray, gray, gray));
      }
      // Draw the line
      painter.drawLine(x, y0, x, y1);
    }
  }
  else
  {
    if (this->isEnabled())
      painter.fillRect(drawRect, plainColor);
    else
    {
      auto gray = 64 + qGray(plainColor.rgb()) / 2;
      painter.fillRect(drawRect, QColor(gray, gray, gray));
    }
  }
}

void ShowColorWidget::setColorMapper(const stats::color::ColorMapper &mapper)
{
  this->renderRange = true;
  this->colMapper   = mapper;
  this->update();
}

void ShowColorWidget::setPlainColor(const QColor &color)
{
  this->renderRange = false;
  this->plainColor  = color;
  this->update();
}

void ShowColorWidget::mouseReleaseEvent(QMouseEvent *)
{
  // If the mouse is released, emit a clicked() event.
  emit clicked();
}
