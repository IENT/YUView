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

// The splitter can be grabbed at +-SPLITTER_MARGIN pixels
// TODO: plus minus 4 pixels for the handle might be not enough for high DPI displays. This should depend on the screens DPI.
#define SPLITTER_MARGIN 4
// The splitter cannot be moved closer to the border of the widget than SPLITTER_CLIPX pixels
// If the splitter is moved closer it cannot be moved back into view and is "lost"
#define SPLITTER_CLIPX 10

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

  centerRelative = QPointF(0.5, 0.5);
  zoomFactor = 1.0;

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
  QPoint drawArea_botR(width()-2, height()-2);

  // Get the current frame to draw
  int frame = playback->getCurrentFrame();

  // Get the playlist item(s) to draw
  playlistItem *item1, *item2;
  playlist->getSelectedItems(item1, item2);

  if (item1 && (!item2 || !splitting))
  {
    // At least one item is selected but we are not drawing two items
    
    QRect geom = geometry();
    QPoint center = QPoint( int(geom.width() * centerRelative.x()), int(geom.height() * centerRelative.y()));
    painter.translate( center );

    item1->drawFrame( frame, &painter );
  }
    

  // Draw the image
  //if (displayObjects[0]) {
  //  if (displayObjects[0]->displayImage().size() == QSize(0,0))
  //    displayObjects[0]->loadImage(0);

  //  QPixmap image_0 = displayObjects[0]->displayImage();

  //  //painter.drawPixmap(m_display_rect[0], image_0, image_0.rect());
  //}
   
  if (splitting)
  {
    // Draw the splitting line at position x + 0.5 (so that all pixels left of
    // x belong to the left view, and all pixels on the right belong to the right one)
    int x = int(drawArea_botR.x() * splittingPoint);
    QLineF line(x+0.5, 0, x+0.5, drawArea_botR.y());
    QPen splitterPen(Qt::white);
    splitterPen.setStyle(Qt::DashLine);
    painter.setPen(splitterPen);
    painter.drawLine(line);
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
      int xClip = clip(mouse_event->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
      splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();
    }
    else if (viewDragging)
    {
      // The user is currently dragging the view. Calculate the new relative position of the center
      QRect geom = geometry();
      QPointF relMousePos = QPointF( mouse_event->localPos().x() / geom.width(), mouse_event->localPos().y() / geom.height() );

      // Convert to relative position depending on the dimensions
      centerRelative = relMousePos + viewDragging_relative;

      // The view was moved. Update the widget.
      update();
    }
    else if (splitting)
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * splittingPoint);

      if (mouse_event->x() > (splitPosPix-SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITTER_MARGIN)) 
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
    if (splitting && mouse_event->x() > (splitPosPix-SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITTER_MARGIN)) 
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      splittingDragging = true;

      // We handeled this event
      mouse_event->accept();
    }
    else
    {
      // We are not splitting or the user did not grab the splitter.
      // In this case drag the view
      viewDragging = true;

      // Save the position where we are grabbing the view relative to the center
      QRect geom = geometry();
      viewDragging_relative = centerRelative - QPointF( mouse_event->localPos().x() / geom.width(), mouse_event->localPos().y() / geom.height() );
      
      //qDebug() << "MouseGrab: " << relMousePos;
      /*QPoint center = QPoint( int(geom.width() * centerRelative.x()), int(geom.height() * centerRelative.y()));
      viewDragging_centerDist = center - mouse_event->pos*/
      
      // We handeled this event
      mouse_event->accept();
    }
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
    int xClip = clip(mouse_event->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
    splittingPoint = (double)xClip / (double)(width()-2);
    update();

    splittingDragging = false;
  }
  else if (mouse_event->button() == Qt::LeftButton && viewDragging)
  {
    // We want this event
    mouse_event->accept();

    // Calculate the new relative position of the center one last time.
    QRect geom = geometry();
    QPointF relMousePos = QPointF( mouse_event->localPos().x() / geom.width(), mouse_event->localPos().y() / geom.height() );

    // Convert to relative position depending on the dimensions
    centerRelative = relMousePos + viewDragging_relative;

    // The view was moved. Update the widget.
    update();

    // End dragging
    viewDragging = false;
  }
}

void splitViewWidget::resetViews()
{
  centerRelative = QPointF(0.5, 0.5);
  zoomFactor = 1.0;

  update();
}
