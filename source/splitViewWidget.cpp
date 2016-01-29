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
#include <QSettings>
#include <QDebug>

#include "typedef.h"
#include "playlistitem.h"
#include "playlisttreewidget.h"
#include "playbackController.h"

splitViewWidget::splitViewWidget(QWidget *parent)
  : QWidget(parent)
{
  setFocusPolicy(Qt::NoFocus);

  splittingPoint = 0.5;
  splittingDragging = false;
  setSplitEnabled(false);
  viewDragging = false;
  viewMode = SIDE_BY_SIDE;

  playlist = NULL;
  playback = NULL;

  updateSettings();

  centerOffset = QPoint(0, 0);
  zoomFactor = 1.0;
  
  // Initialize the font and the position of the zoom factor indication
  zoomFactorFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE);
  QFontMetrics fm(zoomFactorFont);
  zoomFactorFontPos = QPoint( 10, fm.height() );

  // We want to have all mouse events (even move)
  setMouseTracking(true); 
}

void splitViewWidget::setSplitEnabled(bool flag)
{
  if (splitting != flag)
  {
    // Value changed
    splitting = flag;

    // Update (redraw) widget
    update();
  }
}

/** The common settings might have changed.
  * Reload all settings from the Qsettings and set them.
  */
void splitViewWidget::updateSettings()
{
  // load the background color from settings and set it
  QPalette Pal(palette());
  QSettings settings;
  QColor bgColor = settings.value("Background/Color").value<QColor>();
  Pal.setColor(QPalette::Background, bgColor);
  setAutoFillBackground(true);
  setPalette(Pal);
}

void splitViewWidget::paintEvent(QPaintEvent *paint_event)
{
  //qDebug() << paint_event->rect();

  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  QPainter painter(this);
      
  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  // Get the current frame to draw
  int frame = playback->getCurrentFrame();

  // Get the playlist item(s) to draw
  playlistItem *item1, *item2;
  playlist->getSelectedItems(item1, item2);

  // The x position of the split (if splitting)
  int xSplit = int(drawArea_botR.x() * splittingPoint);

  if (splitting)
  {
    // Draw two items (or less, if less items are selected)

    // First determine the center points per item
    QPoint centerPoints[2];
    if (viewMode == COMPARISON)
    {
      // For comparison mode, both items have the same center point, in the middle of the view widget
      centerPoints[0] = drawArea_botR / 2;
      centerPoints[1] = centerPoints[0];
    }
    else
    {
      // For side by side mode, the center points are centered in each individual split view 
      int y = drawArea_botR.y() / 2;
      centerPoints[0] = QPoint( xSplit / 2, y );
      centerPoints[1] = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, y );
    }

    if (item1)
    {
      // Set clipping to the left region
      QRegion clip = QRegion(0, 0, xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Trandlate the painter to the position where we want the item to be
      painter.translate( centerPoints[0] + centerOffset );

      // Draw the item at position (0,0). 
      item1->drawFrame( &painter, frame, zoomFactor );

      // Do the inverse translation of the painter
      painter.translate( (centerPoints[0] + centerOffset) * -1 );
    }
    if (item2)
    {
      // Set clipping to the left region
      QRegion clip = QRegion(xSplit, 0, drawArea_botR.x() - xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Trandlate the painter to the position where we want the item to be
      painter.translate( centerPoints[1] + centerOffset );

      // Draw the item at position (0,0). 
      item2->drawFrame( &painter, frame, zoomFactor );

      // Do the inverse translation of the painter
      painter.translate( (centerPoints[1] + centerOffset) * -1 );
    }

    // Reset clipping
    painter.setClipRegion( QRegion(0, 0, drawArea_botR.x(), drawArea_botR.y()) );
  }
  else // (!splitting)
  {
    // Draw one item (if one item is selected)
    if (item1)
    {
      // At least one item is selected but we are not drawing two items
      QPoint centerPoint = drawArea_botR / 2;

      // Trandlate the painter to the position where we want the item to be
      painter.translate( centerPoint + centerOffset );

      // Draw the item at position (0,0). 
      item1->drawFrame( &painter, frame, zoomFactor );

      // Do the inverse translation of the painter
      painter.translate( (centerPoint + centerOffset) * -1 );
    }
  }
  
  if (splitting)
  {
    // Draw the splitting line at position x + 0.5 (so that all pixels left of
    // x belong to the left view, and all pixels on the right belong to the right one)
    QLine line(xSplit, 0, xSplit, drawArea_botR.y());
    QPen splitterPen(Qt::white);
    //splitterPen.setStyle(Qt::DashLine);
    painter.setPen(splitterPen);
    painter.drawLine(line);
  }

  // Draw the zoom factor
  if (zoomFactor != 1.0)
  {
    QString zoomString = QString("x%1").arg(zoomFactor);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(Qt::black));
    painter.setFont(zoomFactorFont);
    painter.drawText(zoomFactorFontPos, zoomString);
  }
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::NoButton)
  {
    // We want this event
    mouse_event->accept();

    if (splitting && splittingDragging)
    {
      // The user is currently dragging the splitter. Calculate the new splitter point.
      int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2- SPLITVIEWWIDGET_SPLITTER_CLIPX));
      splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();
    }
    else if (viewDragging)
    {
      // The user is currently dragging the view. Calculate the new offset from the center position
      centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

      // The view was moved. Update the widget.
      update();
    }
    else if (splitting)
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * splittingPoint);

      if (mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN)) 
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

void splitViewWidget::mousePressEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::LeftButton)
  {
    // Left mouse buttons pressed
    int splitPosPix = int((width()-2) * splittingPoint);

    // TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
    if (splitting && mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN)) 
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      splittingDragging = true;

      // We handeled this event
      mouse_event->accept();
    }
  }
  else if (mouse_event->button() == Qt::RightButton)
  {
    // The user pressed the right mouse button. In this case drag the view.
    viewDragging = true;

    // Reset the cursor if it was another cursor (over the splitting line for example)
    setCursor(Qt::ArrowCursor);

    // Save the position where the user grabbed the item (screen), and the current value of 
    // the centerOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen).
    viewDraggingMousePosStart = mouse_event->pos();
    viewDraggingStartOffset = centerOffset;
      
    //qDebug() << "MouseGrab - Center: " << centerPoint << " rel: " << grabPosRelative;
      
    // We handeled this event
    mouse_event->accept();
  }
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::LeftButton && splitting && splittingDragging) 
  {
    // We want this event
    mouse_event->accept();

    // The left mouse button was released, we are showing a split view and the user is dragging the splitter.
    // End splitting.

    // Update current splitting position / update last time
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2-SPLITVIEWWIDGET_SPLITTER_CLIPX));
    splittingPoint = (double)xClip / (double)(width()-2);
    update();

    splittingDragging = false;
  }
  else if (mouse_event->button() == Qt::RightButton && viewDragging)
  {
    // We want this event
    mouse_event->accept();

    // Calculate the new center offset one last time
    centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

    // The view was moved. Update the widget.
    update();

    // End dragging
    viewDragging = false;
  }
}

void splitViewWidget::resetViews()
{
  centerOffset = QPoint(0,0);
  zoomFactor = 1.0;
  splittingPoint = 0.5;

  update();
}
