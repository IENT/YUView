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

#include <cmath>

#include <QSettings>
#include <QGestureEvent>
#include <QInputDialog>
#include <QPinchGesture>
#include <QSwipeGesture>

#define MOVEANDZOOMABLEVIEW_WIDGET_DEBUG_OUTPUT 0
#if MOVEANDZOOMABLEVIEW_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_VIEW(fmt) qDebug() << fmt
#else
#define DEBUG_VIEW(fmt,...) ((void)0)
#endif

const Range<double> MoveAndZoomableView::ZOOMINGLIMIT = {0.00001, 100000};

MoveAndZoomableView::MoveAndZoomableView(QWidget *parent)
  : QWidget(parent)
{
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PinchGesture);

  this->updateSettings();
}

void MoveAndZoomableView::addSlaveView(MoveAndZoomableView *view)
{
  Q_ASSERT_X(view, Q_FUNC_INFO, "nullptr");
  Q_ASSERT_X(this->isMasterView, Q_FUNC_INFO, "Can not add slave to a slave");
  Q_ASSERT_X(view->masterView.isNull(), Q_FUNC_INFO, "Slave already has a master");

  view->isMasterView = false;
  view->masterView = this;
  this->slaveViews.append(view);
}

void MoveAndZoomableView::addContextMenuActions(QMenu *menu)
{
  menu->addAction("Zoom to 1:1", this, &MoveAndZoomableView::resetView, Qt::CTRL + Qt::Key_0);
  menu->addAction("Zoom to Fit", this, &MoveAndZoomableView::zoomToFit, Qt::CTRL + Qt::Key_9);
  menu->addAction("Zoom in", this, &MoveAndZoomableView::zoomIn, Qt::CTRL + Qt::Key_Plus);
  menu->addAction("Zoom out", this, &MoveAndZoomableView::zoomOut, Qt::CTRL + Qt::Key_Minus);
  menu->addSeparator();
  menu->addAction("Zoom to 50%", this, &MoveAndZoomableView::zoomTo50);
  menu->addAction("Zoom to 100%", this, &MoveAndZoomableView::zoomTo100);
  menu->addAction("Zoom to 200%", this, &MoveAndZoomableView::zoomTo200);
  menu->addAction("Zoom to ...", this, &MoveAndZoomableView::zoomToCustom);
}

// Handle the key press event (if this widgets handles it). If not, return false.
bool MoveAndZoomableView::handleKeyPress(QKeyEvent *event)
{
  DEBUG_VIEW(QTime::currentTime().toString("hh:mm:ss.zzz") << "Key: " << event);

  int key = event->key();
  bool controlOnly = event->modifiers() == Qt::ControlModifier;

  if (key == Qt::Key_0 && controlOnly)
  {
    this->resetView(false);
    return true;
  }
  else if (key == Qt::Key_9 && controlOnly)
  {
    this->zoomToFitInternal();
    return true;
  }
  else if (key == Qt::Key_Plus && controlOnly)
  {
    this->zoom(ZoomMode::IN);
    return true;
  }
  else if (key == Qt::Key_BracketRight && controlOnly)
  {
    // This seems to be a bug in the Qt localization routine. On the German keyboard layout this key is returned if Ctrl + is pressed.
    this->zoom(ZoomMode::IN);
    return true;
  }
  else if (key == Qt::Key_Minus && controlOnly)
  {
    this->zoom(ZoomMode::OUT);
    return true;
  }

  return false;
}

void MoveAndZoomableView::resetViewInternal()
{
  this->setMoveOffset(QPoint(0,0));
  this->setZoomFactor(1.0);
  update();
}

void MoveAndZoomableView::updatePaletteIfNeeded()
{
  if (this->paletteNeedsUpdate)
  {
    // load the background color from settings and set it
    QPalette pal(palette());
    QSettings settings;
    pal.setColor(QPalette::Window, settings.value(paletteBackgroundColorSettingsTag).value<QColor>());
    this->setAutoFillBackground(true);
    this->setPalette(pal);
    this->paletteNeedsUpdate = false;
  }
}

/* Zoom in/out by one step or to a specific zoom value
 * 
 * Zooming is performed in steps of ZOOM_STEP_FACTOR. However, the zoom factor can not be a multiple of
 * ZOOM_STEP_FACTOR in two cases: The used pinched the screen or entered a custom zoom value. If this is
 * the case and a normal zoom operation happens, we "snap back" to a multiple of ZOOM_STEP_FACTOR. E.g.:
 * If the zoom factor currently is 1.9 and we zoom in, it will become 2.
 * 
 * \param zoomMode In/out or to a specific value. See ZoomMode.
 * \param zoomPoint After the zoom operation the pixel at zoomPoint shall still be at the same position (zoomPoint)
 * \param newZoomFactor Zoom to this value in case of ZoomMode::TO_VALUE
 */
void MoveAndZoomableView::zoom(MoveAndZoomableView::ZoomMode zoomMode, QPoint zoomPoint, double newZoomFactor)
{
  double newZoom = 1.0;
  if (zoomMode == ZoomMode::IN)
  {
    if (this->zoomFactor > 1.0)
    {
      const double inf = std::numeric_limits<double>::infinity();
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

  if (newZoom < ZOOMINGLIMIT.min || newZoom > ZOOMINGLIMIT.max)
    return;

  // So what is the zoom factor that we use in this step?
  double stepZoomFactor = newZoom / this->zoomFactor;

  auto viewCenter = this->getMoveOffsetCoordinateSystemOrigin(zoomPoint);

  if (zoomPoint.isNull())
    zoomPoint = viewCenter;
  
  // The center point has to be moved relative to the zoomPoint
  auto origin = viewCenter;
  auto centerMoveOffset = origin + this->moveOffset;
  
  auto movementDelta = centerMoveOffset - zoomPoint;
  auto newMoveOffset = this->moveOffset - (1 - stepZoomFactor) * movementDelta;
  
  DEBUG_VIEW("MoveAndZoomableView::zoom point debug zoomPoint " << zoomPoint << " viewCenter " << viewCenter << " this->moveOffset " << this->moveOffset << " centerMoveOffset " << centerMoveOffset << " stepZoomFactor " << stepZoomFactor << " movementDelta " << movementDelta);
  DEBUG_VIEW("MoveAndZoomableView::zoom point " << newMoveOffset);
  this->setZoomFactor(newZoom);
  this->setMoveOffset(newMoveOffset);

  this->update();

  if (newZoom > 1.0)
    this->onZoomIn();
}

void MoveAndZoomableView::wheelEvent(QWheelEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  auto p = event->position().toPoint();
#else
  auto p = event->pos();
#endif

  DEBUG_VIEW("MoveAndZoomableView::wheelEvent delta " << event->angleDelta().y() << " pos " << p);

  auto deltaAbs = std::abs(event->angleDelta().y());
  auto deltaPositive = event->angleDelta().y() > 0;
  auto noAction = (this->viewAction == ViewAction::NONE);
  if (noAction && deltaAbs > 0)
  {
    auto deltaScaled = (double(deltaAbs) / 120);
    auto deltaFactor = (deltaPositive) ? 1.0 + deltaScaled : 1.0 - deltaScaled / 2;
    auto newZoomFactor = this->zoomFactor * deltaFactor;
    this->zoom(ZoomMode::TO_VALUE, p, newZoomFactor);
  }

  event->accept();
}

void MoveAndZoomableView::keyPressEvent(QKeyEvent *event)
{
  if (!handleKeyPress(event))
    // If this widget does not handle the key press event, pass it up to the widget so that
    // it is propagated to the parent.
    QWidget::keyPressEvent(event);
}

void MoveAndZoomableView::resizeEvent(QResizeEvent *event)
{
  Q_UNUSED(event);
  this->update();
}

void MoveAndZoomableView::updateMouseCursor()
{
  this->updateMouseCursor(mapFromGlobal(QCursor::pos()));
}

bool MoveAndZoomableView::updateMouseCursor(const QPoint &mousePos)
{
  // Check if the position is within the widget
  if (mousePos.x() < 0 || mousePos.x() > width() || mousePos.y() < 0 || mousePos.y() > height())
    return false;

  if (this->viewAction == ViewAction::DRAGGING || this->viewAction == ViewAction::DRAGGING_MOUSE_MOVED)
    // Dragging the view around. Show the closed hand cursor.
    setCursor(Qt::ClosedHandCursor);

  setCursor(Qt::ArrowCursor);
  return true;
}

void MoveAndZoomableView::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->source() == Qt::MouseEventSynthesizedBySystem && (this->viewAction == ViewAction::PINCHING || this->viewAction == ViewAction::DRAGGING_TOUCH))
  {
    DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent ignore system generated touch event");
    mouse_event->ignore();
    return;
  }

  if (mouse_event->buttons() == Qt::NoButton)
  {
    // The mouse is moved, but no button is pressed. This should not be caught here. Maybe a mouse press/release event
    // got lost somewhere. In this case go to the normal mode.
    if (this->viewAction == ViewAction::DRAGGING || this->viewAction == ViewAction::DRAGGING_MOUSE_MOVED || this->viewAction == ViewAction::ZOOM_RECT)
    {
      DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent no Button - abort any action");
      this->viewAction = ViewAction::NONE;
    }
  }

  if (this->viewAction == ViewAction::DRAGGING || this->viewAction == ViewAction::DRAGGING_MOUSE_MOVED)
  {
    // Calculate the new offset from the center position
    this->setMoveOffset(viewDraggingStartOffset + (mouse_event->pos() - this->viewDraggingMousePosStart));
      
    if (this->viewAction == ViewAction::DRAGGING)
    {
      auto mouseMoved = viewDraggingMousePosStart - mouse_event->pos();
      if (mouseMoved.manhattanLength() > 3)
      {
        this->viewAction = ViewAction::DRAGGING_MOUSE_MOVED;
        DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent mouse was moved > 3 pxixels");
      }
    }

    DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent dragging pos " << mouse_event->pos());
    mouse_event->accept();
    this->update();
  }
  else if (this->viewAction == ViewAction::ZOOM_RECT)
  {
    this->viewZoomingMousePos = mouse_event->pos();
    DEBUG_VIEW("MoveAndZoomableView::mouseMoveEvent zooming pos " << viewZoomingMousePos);
    mouse_event->accept();
    this->update();
  }
  else
    this->updateMouseCursor(mouse_event->pos());
}

void MoveAndZoomableView::mousePressEvent(QMouseEvent *mouse_event)
{
  if (this->viewAction != ViewAction::NONE)
  {
    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent ignore - action inprogress");
    return;
  }

  if ((mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
     (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE))
  {
    this->viewAction = ViewAction::DRAGGING;

    // Save the position where the user grabbed the item (screen), and the current value of
    // the moveOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen). The moveOffset will be updated once the user releases
    // the mouse button and the action is finished.
    this->viewDraggingMousePosStart = mouse_event->pos();
    this->viewDraggingStartOffset = this->moveOffset;

    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent start dragging posStart " << this->viewDraggingMousePosStart << " startOffset " << this->viewDraggingStartOffset);
    mouse_event->accept();
  }
  else if ((mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_RIGHT_MOVE))
  {
    this->viewAction = ViewAction::ZOOM_RECT;

    // Save the position of the mouse where the user started the zooming.
    this->viewZoomingMousePosStart = mouse_event->pos();
    this->viewZoomingMousePos = mouse_event->pos();

    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent start zoombox posStart " << this->viewZoomingMousePosStart << " mousePos " << this->viewZoomingMousePos);
    mouse_event->accept();
  }

  this->updateMouseCursor(mouse_event->pos());
}

void MoveAndZoomableView::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if ((this->viewAction == ViewAction::DRAGGING || this->viewAction == ViewAction::DRAGGING_MOUSE_MOVED) && 
    ((mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
     (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    if (mouse_event->button() == Qt::RightButton && this->viewAction == ViewAction::DRAGGING)
    {
      QMenu menu(this);
      this->addContextMenuActions(&menu);
      menu.exec(mouse_event->globalPos());
    }

    this->setMoveOffset(this->viewDraggingStartOffset + (mouse_event->pos() - this->viewDraggingMousePosStart));
    this->viewAction = ViewAction::NONE;
    this->update();
    
    DEBUG_VIEW("MoveAndZoomableView::mousePressEvent end dragging posEnd " << mouse_event->pos());
    mouse_event->accept();
  }
  else if (this->viewAction == ViewAction::ZOOM_RECT && 
          ((mouse_event->button() == Qt::RightButton  && mouseMode == MOUSE_LEFT_MOVE) ||
           (mouse_event->button() == Qt::LeftButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    DEBUG_VIEW("MoveAndZoomableView::mouseReleaseEvent end zoomRect posEnd " << mouse_event->pos());
    mouse_event->accept();

    // Zoom so that the whole rectangle is visible and center it in the view.
    auto zoomRect = QRectF(viewZoomingMousePosStart, mouse_event->pos());
    if (std::abs(zoomRect.width()) < 2.0 && std::abs(zoomRect.height()) < 2.0)
    {
      // The user just pressed the button without moving the mouse.
      this->viewAction = ViewAction::NONE;
      update();
      return;
    }

    // Now we zoom in as far as possible
    auto additionalZoomFactor = 1.0;
    while (std::abs(zoomRect.width())  * additionalZoomFactor * ZOOM_STEP_FACTOR <= width() &&
           std::abs(zoomRect.height()) * additionalZoomFactor * ZOOM_STEP_FACTOR <= height())
      additionalZoomFactor *= ZOOM_STEP_FACTOR;

    this->onZoomRectUpdateOffsetAndZoom(zoomRect, additionalZoomFactor);
    this->viewAction = ViewAction::NONE;
    this->update();
  }
}

bool MoveAndZoomableView::event(QEvent *event)
{
  // IMPORTANT:
  // We are not using the QPanGesture as this uses two fingers. This is not documented in the Qt documentation.

  bool isTouchScreenEvent = false;
  if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel)
  {
    auto touchEvent = static_cast<QTouchEvent*>(event);
    auto device = touchEvent->device();
    isTouchScreenEvent = (device->type() == QTouchDevice::TouchScreen);
  }

  if (event->type() == QEvent::Gesture)
  {
    auto gestureEvent = static_cast<QGestureEvent*>(event);

    if (QGesture *swipeGesture = gestureEvent->gesture(Qt::SwipeGesture))
    {
      auto swipe = static_cast<QSwipeGesture*>(swipeGesture);
      DEBUG_VIEW("MoveAndZoomableView::event swipe gesture");

      if (swipe->state() == Qt::GestureStarted)
        // The gesture was just started. This will prevent (generated) mouse events from being interpreted.
        this->viewAction = ViewAction::PINCHING;

      if (swipe->state() == Qt::GestureFinished)
      {
        if (swipe->horizontalDirection() == QSwipeGesture::NoDirection && swipe->verticalDirection() == QSwipeGesture::Up)
          this->onSwipeUp();
        else if (swipe->horizontalDirection() == QSwipeGesture::NoDirection && swipe->verticalDirection() == QSwipeGesture::Down)
          this->onSwipeDown();
        else if (swipe->horizontalDirection() == QSwipeGesture::Left && swipe->verticalDirection() == QSwipeGesture::NoDirection)
          this->onSwipeLeft();
        else if (swipe->horizontalDirection() == QSwipeGesture::Right && swipe->verticalDirection() == QSwipeGesture::NoDirection)
          this->onSwipeRight();
        else
        {
          // The swipe was both horizontal and vertical. What is the dominating direction?
          double a = swipe->swipeAngle();
          if (a < 45 || a > 315)
            this->onSwipeRight();
          else if (a >= 45 && a < 135)
            this->onSwipeUp();
          else if (a >= 135 && a < 225)
            this->onSwipeLeft();
          else
            this->onSwipeDown();
        }

        this->viewAction = ViewAction::NONE;
      }

      event->accept();
      update();
    }
    if (auto pinchGesture = gestureEvent->gesture(Qt::PinchGesture))
    {
      auto pinch = static_cast<QPinchGesture*>(pinchGesture);
      DEBUG_VIEW("MoveAndZoomableView::event swipe pinch");

      if (pinch->state() == Qt::GestureStarted)
      {
        // The gesture was just started. This will prevent (generated) mouse events from being interpreted.
        this->viewAction = ViewAction::PINCHING;
        this->pinchStartMoveOffset = this->moveOffset;
        this->pinchStartZoomFactor = this->zoomFactor;
        this->pinchStartCenterPoint = pinch->centerPoint();
      }

      const auto newZoom = this->pinchStartZoomFactor * pinch->totalScaleFactor();
      bool doZoom = true;
      if (pinch->state() == Qt::GestureStarted || pinch->state() == Qt::GestureUpdated)
      {
        // See what changed in this pinch gesture (the scale factor and/or the position)
        auto changeFlags = pinch->changeFlags();
        if (changeFlags & QPinchGesture::ScaleFactorChanged || changeFlags & QPinchGesture::CenterPointChanged)
        {
          if (newZoom < ZOOMINGLIMIT.min || newZoom > ZOOMINGLIMIT.max)
            doZoom = false;
          else
          {
            setZoomFactor(newZoom);
            setMoveOffset((this->pinchStartMoveOffset * pinch->totalScaleFactor() + pinch->centerPoint() - this->pinchStartCenterPoint).toPoint());
          }
        }
      }
      else if (pinch->state() == Qt::GestureFinished)
      {
        if (newZoom < ZOOMINGLIMIT.min || newZoom > ZOOMINGLIMIT.max)
          doZoom = false;
        else
        {
          setZoomFactor(this->pinchStartZoomFactor * pinch->totalScaleFactor());
          setMoveOffset((this->pinchStartMoveOffset * pinch->totalScaleFactor() + pinch->centerPoint() - this->pinchStartCenterPoint).toPoint());
        }
        
        this->pinchStartMoveOffset = QPointF();
        this->pinchStartZoomFactor = 1.0;
        this->pinchStartCenterPoint = QPointF();
        this->viewAction = ViewAction::NONE;
      }

      event->accept();
      if (doZoom)
        this->update();
    }
  }
  else if (isTouchScreenEvent)
  {
    auto touchEvent = static_cast<QTouchEvent*>(event);
    const auto &touchPoints = touchEvent->touchPoints();
    if (touchPoints.size() >= 1)
    {
      DEBUG_VIEW("MoveAndZoomableView::event handle touch even " << event->type());
      auto pos = touchPoints[0].pos().toPoint();
      if (this->viewAction == ViewAction::NONE && event->type() == QEvent::TouchBegin)
      {
        this->viewAction = ViewAction::DRAGGING_TOUCH;

        // Save the position where the user grabbed the item (screen), and the current value of
        // the moveOffset. So when the user moves the mouse, the new offset is just the old one
        // plus the difference between the position of the mouse and the position where the
        // user grabbed the item (screen). The moveOffset will be updated once the user releases
        // the mouse button and the action is finished.
        this->viewDraggingMousePosStart = pos;
        this->viewDraggingStartOffset = this->moveOffset;

        DEBUG_VIEW("MoveAndZoomableView::event start touch dragging posStart " << this->viewDraggingMousePosStart << " startOffset " << this->viewDraggingStartOffset);
      }
      else if (event->type() == QEvent::TouchUpdate && this->viewAction == ViewAction::DRAGGING_TOUCH)
      {
        DEBUG_VIEW("MoveAndZoomableView::event touch dragging pos " << pos);
        this->setMoveOffset(viewDraggingStartOffset + (pos - this->viewDraggingMousePosStart));
        this->update();
      }
      else if (this->viewAction == ViewAction::DRAGGING_TOUCH)
      {
        DEBUG_VIEW("MoveAndZoomableView::event touch dragging end pos " << pos);
        this->setMoveOffset(this->viewDraggingStartOffset + (pos - this->viewDraggingMousePosStart));
        this->viewAction = ViewAction::NONE;
        this->update();
      }
    }
    else
      DEBUG_VIEW("MoveAndZoomableView::event handle touch even no points");

    touchEvent->accept();
  }
  else if (event->type() == QEvent::NativeGesture)
  {
    // TODO #195 - For pinching on mac this would have to be added here...
    // QNativeGestureEvent
    DEBUG_VIEW("QNativeGestureEvent");

    return false;
  }
  else
  {
    //DEBUG_VIEW("MoveAndZoomableView::event unhandled event type " << event->type());
    return QWidget::event(event);
  }

  return true;
}

void MoveAndZoomableView::update()
{
  if (this->enableLink)
  {
    if (isMasterView)
    {
      for (auto v : this->slaveViews)
        v->slaveUpdateWidget();

      DEBUG_VIEW("MoveAndZoomableView::update master");
      QWidget::update();
    }
    else
    {
      assert(this->masterView);
      DEBUG_VIEW("MoveAndZoomableView::update forward update to master");
      this->masterView->update();
    }
  }
  else // !this->enableLink
  {
    DEBUG_VIEW("MoveAndZoomableView::update link off");
    QWidget::update();
  }
}

void MoveAndZoomableView::setZoomFactor(double zoom)
{
  if (this->enableLink)
  {
    if (isMasterView)
    {
      for (auto v : this->slaveViews)
        v->slaveSetZoomFactor(zoom);

      DEBUG_VIEW("MoveAndZoomableView::setZoomFactor master " << zoom);
      this->zoomFactor = zoom;
      this->updateLinkedViews = true;
    }
    else
    {
      assert(this->masterView);
      DEBUG_VIEW("MoveAndZoomableView::setZoomFactor forward to master " << zoom);
      this->masterView->setZoomFactor(zoom);
    }
  }
  else // !this->enableLink
  {
    DEBUG_VIEW("MoveAndZoomableView::setZoomFactor link off " << zoom);
    this->zoomFactor = zoom;
  }
}

void MoveAndZoomableView::drawZoomRect(QPainter &painter) const
{
  if (this->viewAction != ViewAction::ZOOM_RECT)
    return;

  painter.setPen(ZOOM_RECT_PEN);
  painter.setBrush(ZOOM_RECT_BRUSH);
  painter.drawRect(QRectF(viewZoomingMousePosStart, viewZoomingMousePos));
}

void MoveAndZoomableView::setMoveOffset(QPointF offset)
{
  if (this->enableLink)
  {
    if (isMasterView)
    {
      for (auto v : this->slaveViews)
        v->slaveSetMoveOffset(offset);

      DEBUG_VIEW("MoveAndZoomableView::setMoveOffset master " << offset);
      this->updateLinkedViews = true;
      this->moveOffset = offset;
    }
    else
    {
      assert(this->masterView);
      DEBUG_VIEW("MoveAndZoomableView::setMoveOffset forward to master " << offset);
      this->masterView->setMoveOffset(offset);
    }
  }
  else // !this->enableLink
  {
    DEBUG_VIEW("MoveAndZoomableView::setMoveOffset link off " << offset);
    this->moveOffset = offset;
  }
}

/** The common settings might have changed. Reload all settings from the QSettings.
  */
void MoveAndZoomableView::updateSettings()
{
  QSettings settings;

  // Update the palette in the next draw event.
  // We don't do this here because Qt overwrites the setting if the theme is changed.
  paletteNeedsUpdate = true;

  // Load the mouse mode
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    this->mouseMode = MOUSE_RIGHT_MOVE;
  else
    this->mouseMode = MOUSE_LEFT_MOVE;
}

void MoveAndZoomableView::zoomToFitInternal()
{
  this->setMoveOffset(QPoint(0,0));
  this->setZoomFactor(1.0);
  update();
}

void MoveAndZoomableView::zoomToCustom(bool checked)
{
  Q_UNUSED(checked);
  bool ok;
  int newValue = QInputDialog::getInt(this, "Zoom to custom value", "Please select a zoom factor in percent", 100, 1, 2147483647, 1, &ok);
  if (ok)
    zoom(ZoomMode::TO_VALUE, QPoint(), double(newValue) / 100);
}

void MoveAndZoomableView::setLinkState(bool enabled)
{
  if (this->isMasterView)
  {
    for (auto v : this->slaveViews)
      v->slaveSetLinkState(enabled);
    
    DEBUG_VIEW("MoveAndZoomableView::setLinkState master set link state " << enabled);
    this->updateLinkedViews = true;
    this->enableLink = enabled;
  }
  else
  {
    Q_ASSERT_X(this->masterView, Q_FUNC_INFO, "Master not set for slave");
    DEBUG_VIEW("MoveAndZoomableView::setLinkState slave send link state " << enabled << " to master");
    this->masterView->setLinkState(enabled);
  }
}

void MoveAndZoomableView::slaveSetLinkState(bool enabled)
{
  Q_ASSERT_X(!this->isMasterView, Q_FUNC_INFO, "Not a slave item");
  this->enableLink = enabled;

  if (enabled)
    this->getStateFromMaster();
}

void MoveAndZoomableView::slaveSetMoveOffset(QPointF offset)
{
  Q_ASSERT_X(!this->isMasterView, Q_FUNC_INFO, "Not a slave item");
  this->moveOffset = offset;
}

void MoveAndZoomableView::slaveSetZoomFactor(double zoom)
{
  Q_ASSERT_X(!this->isMasterView, Q_FUNC_INFO, "Not a slave item");
  this->zoomFactor = zoom;
}

void MoveAndZoomableView::slaveUpdateWidget()
{
  Q_ASSERT_X(!this->isMasterView, Q_FUNC_INFO, "Not a slave item");
  QWidget::update();
}

void MoveAndZoomableView::getStateFromMaster()
{
  this->moveOffset = this->masterView->moveOffset;
  this->zoomFactor = this->masterView->zoomFactor;
}