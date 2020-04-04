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

#include "MoveAndZoomableView.h"

#include "common/functions.h"

#include <QSettings>

const int ZOOM_STEP_FACTOR = 2;

#define MOVEANDZOOMABLEVIEW_WIDGET_DEBUG_OUTPUT 1
#if MOVEANDZOOMABLEVIEW_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_VIEW(fmt) qDebug() << fmt
#else
#define DEBUG_VIEW(fmt,...) ((void)0)
#endif

MoveAndZoomableView::MoveAndZoomableView(QWidget *parent) 
  : QWidget(parent)
{
  // We want to have all mouse events (even move)
  setMouseTracking(true);

  updateSettings();
}

void MoveAndZoomableView::zoom(MoveAndZoomableView::ZoomMode zoomMode, const QPoint &zoomPoint, double newZoomFactor)
{
  // The zoom point works like this: After the zoom operation the pixel at zoomPoint shall
  // still be at the same position (zoomPoint)

  // What is the factor that we will zoom in by?
  // The zoom factor could currently not be a multiple of ZOOM_STEP_FACTOR
  // if the user used pinch zoom. So let's go back to the step size of ZOOM_STEP_FACTOR
  // and calculate the next higher zoom which is a multiple of ZOOM_STEP_FACTOR.
  // E.g.: If the zoom factor currently is 1.9 we want it to become 2 after zooming.

  double newZoom = 1.0;
  if (zoomMode == ZoomMode::IN)
  {
    if (this->zoomFactor > 1.0)
    {
      double inf = std::numeric_limits<double>::infinity();
      while (newZoom <= this->zoomFactor && newZoom < inf)
        newZoom *= ZOOM_STEP_FACTOR;
    }
    else
    {
      while (newZoom > this->zoomFactor)
        newZoom /= ZOOM_STEP_FACTOR;
      newZoom *= ZOOM_STEP_FACTOR;
    }
  }
  else if (zoomMode == ZoomMode::OUT)
  {
    if (this->zoomFactor > 1.0)
    {
      while (newZoom < zoomFactor)
        newZoom *= ZOOM_STEP_FACTOR;
      newZoom /= ZOOM_STEP_FACTOR;
    }
    else
    {
      while (newZoom >= this->zoomFactor && newZoom > 0.0)
        newZoom /= ZOOM_STEP_FACTOR;
    }
  }
  else if (zoomMode == ZoomMode::TO_VALUE)
  {
    newZoom = newZoomFactor;
  }
  // So what is the zoom factor that we use in this step?
  double stepZoomFactor = newZoom / this->zoomFactor;

  if (!zoomPoint.isNull())
  {
    // // The center point has to be moved relative to the zoomPoint

    // // Get the absolute center point of the item
    // QPoint drawArea_botR(width(), height());
    // QPoint centerPoint = drawArea_botR / 2;

    // if (viewSplitMode == SIDE_BY_SIDE)
    // {
    //   // For side by side mode, the center points are centered in each individual split view

    //   // Which side of the split view are we zooming in?
    //   // Get the center point of that view
    //   int xSplit = int(drawArea_botR.x() * splittingPoint);
    //   if (zoomPoint.x() > xSplit)
    //     // Zooming in the right view
    //     centerPoint = QPoint(xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2);
    //   else
    //     // Zooming in the left view
    //     centerPoint = QPoint(xSplit / 2, drawArea_botR.y() / 2);
    // }

    // // The absolute center point of the item under the cursor
    // QPoint itemCenter = centerPoint + centerOffset;

    // // Move this item center point
    // QPoint diff = itemCenter - zoomPoint;
    // diff *= stepZoomFactor;
    // itemCenter = zoomPoint + diff;

    // // Calculate the new center offset
    // setCenterOffset(itemCenter - centerPoint);
  }
  else
  {
    // Zoom without considering the mouse position
    this->moveOffset = this->moveOffset * stepZoomFactor;
  }

  this->zoomFactor = newZoom;

  update();
}

void MoveAndZoomableView::wheelEvent(QWheelEvent *e)
{
  QPoint p = e->pos();
  e->accept();
  zoom(e->delta() > 0 ? ZoomMode::IN : ZoomMode::OUT, p);
}

void MoveAndZoomableView::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  this->update();
}

void MoveAndZoomableView::updateMouseCursor()
{
  updateMouseCursor(mapFromGlobal(QCursor::pos()));
}

void MoveAndZoomableView::updateMouseCursor(const QPoint &mousePos)
{
  // Check if the position is within the widget
  if (mousePos.x() < 0 || mousePos.x() > width() || mousePos.y() < 0 || mousePos.y() > height())
    return;

  if (viewDragging)
    // Dragging the view around. Show the closed hand cursor.
    setCursor(Qt::ClosedHandCursor);

  setCursor(Qt::ArrowCursor);
}

void MoveAndZoomableView::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && currentlyPinching)
    // The mouse event was generated by the system from a touch event which is already handled by the touch pinch handler.
    return;

  if (mouse_event->buttons() == Qt::NoButton)
  {
    // The mouse is moved, but no button is pressed. This should not be caught here. Maybe a mouse press/release event
    // got lost somewhere. In this case go to the normal mode.
    if (viewDragging)
    {
      // End dragging
      viewDragging = false;
      viewDraggingMouseMoved = false;
      DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent no Button - abort dragging");
    }
    else if (viewZooming)
    {
      viewZooming = false;
      DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent no Button - abort zooming");
    }
  }

  mouse_event->accept();

  if (viewDragging)
  {
    // The user is currently dragging the view. Calculate the new offset from the center position
    setCenterOffset(viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart));
    auto mouseMoved = viewDraggingMousePosStart - mouse_event->pos();
    if (mouseMoved.manhattanLength() > 3)
      viewDraggingMouseMoved = true;

    DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent dragging pos " << mouse_event->pos());
    update();
  }
  else if (viewZooming)
  {
    // The user is currently using the mouse to zoom. Save the current mouse position so that we can draw a zooming rectangle.
    viewZoomingMousePos = mouse_event->pos();
    DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent zooming pos ", viewZoomingMousePos);
    update();
  }
  else
    updateMouseCursor(mouse_event->pos());
}

void MoveAndZoomableView::mousePressEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && currentlyPinching)
    // The mouse event was generated by the system from a touch event which is already handled by the touch pinch handler.
    return;

  if (viewFrozen)
    return;

  if ((mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
     (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE))
  {
    // The user pressed the 'move' mouse button. In this case drag the view.
    viewDragging = true;

    // Save the position where the user grabbed the item (screen), and the current value of
    // the centerOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen).
    viewDraggingMousePosStart = mouse_event->pos();
    viewDraggingStartOffset = centerOffset;
    viewDraggingMouseMoved = false;

    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent start dragging posStart " << viewDraggingMousePosStart << " startOffset " << viewDraggingStartOffset);

    mouse_event->accept();
  }
  else if ((mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_RIGHT_MOVE))
  {
    // The user pressed the 'zoom' mouse button. In this case start drawing the zoom box.
    viewZooming = true;

    // Save the position of the mouse where the user started the zooming.
    viewZoomingMousePosStart = mouse_event->pos();
    viewZoomingMousePos = viewZoomingMousePosStart;

    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent start zoombox posStart " << viewZoomingMousePosStart << " mousePos " << viewZoomingMousePos);

    mouse_event->accept();
  }

  updateMouseCursor(mouse_event->pos());
}

void MoveAndZoomableView::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (viewFrozen)
    return;

  if (viewDragging && (
     (mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
     (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    // The user released the mouse 'move' button and was dragging the view.
    DEBUG_VIEW("MoveAndZoomableView::mouseReleaseEvent end dragging posEnd " << mouse_event->pos());

    mouse_event->accept();

    // Calculate the new center offset one last time
    setCenterOffset(viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart));

    if (mouse_event->button() == Qt::RightButton && !viewDraggingMouseMoved)
      this->openContextMenu();

    // End dragging
    viewDragging = false;
    viewDraggingMouseMoved = false;
    update();
  }
  else if (viewZooming && (
           (mouse_event->button() == Qt::RightButton  && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    // The user used the mouse to zoom. End this operation.
    DEBUG_VIEW("MoveAndZoomableView::mouseReleaseEvent end zoombox posEnd " << mouse_event->pos());

    mouse_event->accept();

    // Zoom so that the whole rectangle is visible and center it in the view.
    QRect zoomRect = QRect(viewZoomingMousePosStart, mouse_event->pos());
    if (abs(zoomRect.width()) < 2 && abs(zoomRect.height()) < 2)
    {
      // The user just pressed the button without moving the mouse.
      viewZooming = false;
      update();
      return;
    }

    // Get the absolute center point of the view
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    // Calculate the new center offset
    QPoint zoomRectCenterOffset = zoomRect.center() - centerPoint;
    setCenterOffset(centerOffset - zoomRectCenterOffset);

    // Now we zoom in as far as possible
    double additionalZoomFactor = 1.0;
    while (abs(zoomRect.width())  * additionalZoomFactor * ZOOM_STEP_FACTOR <= width() &&
           abs(zoomRect.height()) * additionalZoomFactor * ZOOM_STEP_FACTOR <= height())
    {
      // We can zoom in more
      zoomFactor = zoomFactor * ZOOM_STEP_FACTOR;
      additionalZoomFactor *= ZOOM_STEP_FACTOR;
      setCenterOffset(centerOffset * ZOOM_STEP_FACTOR);
    }

    // End zooming
    viewZooming = false;

    // The view was moved. Update the widget.
    update();
  }
}

void MoveAndZoomableView::setCenterOffset(QPoint offset, bool setOtherViewIfLinked, bool callUpdate)
{
  if (enableLink && setOtherViewIfLinked)
    for (auto v : this->linkedViews)
      v->setCenterOffset(offset, false, callUpdate);

  centerOffset = offset;
}

/** The common settings might have changed.
  * Reload all settings from the QSettings and set them.
  */
void MoveAndZoomableView::updateSettings()
{
  QSettings settings;

  // Load the mouse mode
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    mouseMode = MOUSE_RIGHT_MOVE;
  else
    mouseMode = MOUSE_LEFT_MOVE;

  // Something about how we draw might have been changed
  update();
}
