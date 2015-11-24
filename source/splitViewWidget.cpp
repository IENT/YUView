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

// The splitter can be grabbed at +-SPLITTER_MARGIN pixels
// TODO: plus minus 4 pixels for the handle might be not enough for high DPI displays. This should depend on the screens DPI.
#define SPLITTER_MARGIN 4
// The splitter cannot be moved closer to the border of the widget than SPLITTER_CLIPX pixels
// If the splitter is moved closer it cannot be moved back into view and is "lost"
#define SPLITTER_CLIPX 10

splitViewWidget::splitViewWidget(QWidget *parent)
  : QWidget(parent)
{
  m_splittingPoint = 0.5;
  m_splittingDragging = false;
  setSplitEnabled(false);

  updateSettings();
}

void splitViewWidget::setSplitEnabled(bool splitting)
{
  if (m_splitting != splitting)
  {
    // Value changed
    m_splitting = splitting;

    // Update (redraw) widget
    update();
  }

  // If splitting is active we want to have all mouse move events.
  // Also the ones when no button is pressed
  setMouseTracking(m_splitting); 
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

  QPainter painter(this);
      
  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width()-2, height()-2);

  // Draw the image
  if (m_disp_obj[0]) {
    if (m_disp_obj[0]->displayImage().size() == QSize(0,0))
      m_disp_obj[0]->loadImage(0);

    QPixmap image_0 = m_disp_obj[0]->displayImage();

    //painter.drawPixmap(m_display_rect[0], image_0, image_0.rect());
  }
   
  if (m_splitting) 
  {
    // Draw the splitting line at position x
    int x = int(drawArea_botR.x() * m_splittingPoint);
    QPen splitterPen(Qt::white);
    splitterPen.setStyle(Qt::DashLine);
    painter.setPen(splitterPen);
    painter.drawLine(x, 0, x, drawArea_botR.y());
  }
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::NoButton && m_splitting)
  {
    // We want this event
    mouse_event->accept();

    if (m_splittingDragging) 
    {
      // The user is currently dragging the splitter. Calculate the new splitter point.
      int xClip = clip(mouse_event->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
      m_splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();
    }
    else 
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * m_splittingPoint);

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
  if (mouse_event->button() == Qt::LeftButton && m_splitting)
  {
    // Left mouse buttons pressed and the view is split
    int splitPosPix = int((width()-2) * m_splittingPoint);

    // TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
    if (mouse_event->x() > (splitPosPix-SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITTER_MARGIN)) 
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      m_splittingDragging = true;

      // We want this event
      mouse_event->accept();
    }
  }
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::LeftButton && m_splitting && m_splittingDragging) 
  {
    // We want this event
    mouse_event->accept();

    // The left mouse button was released, we are showing a split view and the user is dragging the splitter.
    // End splitting.

    // Update current splitting position / update last time
    int xClip = clip(mouse_event->x(), SPLITTER_CLIPX, (width()-2-SPLITTER_CLIPX));
    m_splittingPoint = (double)xClip / (double)(width()-2);
    update();

    m_splittingDragging = false;
  }
}

void splitViewWidget::setActiveDisplayObjects(QSharedPointer<DisplayObject> disp_obj_0, QSharedPointer<DisplayObject> disp_obj_1)
{
  bool update_widget = (m_disp_obj[0] != disp_obj_0 || m_disp_obj[1] != disp_obj_1);
  
  m_disp_obj[0] = disp_obj_0;
  m_disp_obj[1] = disp_obj_1;

  if (update_widget)
    update();
}

void splitViewWidget::resetViews(int view_id)
{
}