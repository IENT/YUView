/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "splitViewWidget.h"

#include <QPainter>
#include <QColor>
#include <QMouseEvent>
#include "typedef.h"

// The splitter can be grabbed at +-SPLITTER_MARGIN pixels
// TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
#define SPLITTER_MARGIN 4
// The splitter cannot be moved closer to the border of the widget than SPLITTER_CLIPX pixels
// If the splitter is moved closer it cannot be moved back into view and is "lost"
#define SPLITTER_CLIPX 10

splitViewWidget::splitViewWidget(QWidget *parent)
  : QWidget(parent)
{
  p_splittingPoint = 0.5;
  p_splittingDragging = false;
  setSplitting(true);
}

void splitViewWidget::setSplitting(bool bSplitting)
{
  p_splitting = bSplitting;

  // If splitting is active we want to have all mouse move events.
  // Also the ones when no button is pressed
  setMouseTracking(p_splitting);
}

void splitViewWidget::paintEvent(QPaintEvent *)
{
   QPainter painter(this);

   // The pens for drawing the border/splitter.
   QPen borderPen(Qt::gray);
   QPen splitterPen(Qt::gray);
   splitterPen.setStyle(Qt::DashLine);

   // Get the full size of the area that we can draw on (from the paint device base)
   QPoint drawArea_botR(width()-2, height()-2);

   // Draw a black border around the widget
   painter.setPen(borderPen);
   painter.drawRect(QRect(QPoint(0,0), drawArea_botR));
   
   if (p_splitting) 
   {
     // Draw the splitting line
     int x = int(drawArea_botR.x() * p_splittingPoint);

     painter.setPen(splitterPen);
     painter.drawLine(x, 0, x, drawArea_botR.y());
   }
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouseEvent)
{
  if (mouseEvent->button() == Qt::NoButton && p_splitting)
  {
    if (p_splittingDragging) 
    {
      // The user is currently dragging the splitter. Calculate the new splitter point.
      int xClip = clip(mouseEvent->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
      p_splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();
    }
    else 
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * p_splittingPoint);

      if (mouseEvent->x() > (splitPosPix-SPLITTER_MARGIN) && mouseEvent->x() < (splitPosPix+SPLITTER_MARGIN)) 
      {
        // Mouse is over the line in the middle (plus minus 4 pixels)
        setCursor(Qt::SplitHCursor);
      }
      else 
      {
        // Mouse is not over the splitter line
        setCursor(Qt::ArrowCursor);
      }
    }
  }
}

void splitViewWidget::mousePressEvent(QMouseEvent *mouseEvent)
{
  if (mouseEvent->button() == Qt::LeftButton && p_splitting)
  {
    // Left mouse buttons pressed and the view is split
    int splitPosPix = int((width()-2) * p_splittingPoint);

    // TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
    if (mouseEvent->x() > (splitPosPix-SPLITTER_MARGIN) && mouseEvent->x() < (splitPosPix+SPLITTER_MARGIN)) 
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      p_splittingDragging = true;
    }
  }
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent * mouseEvent)
{
  if (mouseEvent->button() == Qt::LeftButton && p_splitting && p_splittingDragging) 
  {
    // The left mouse button was released, we are showing a split view and the user is dragging the splitter.
    // End splitting.

    // Update current splitting position / update last time
    int xClip = clip(mouseEvent->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
    p_splittingPoint = (double)xClip / (double)(width()-2);
    update();

    p_splittingDragging = false;
  }
}