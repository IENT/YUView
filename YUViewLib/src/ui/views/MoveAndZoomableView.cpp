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
#include <QGestureEvent>
#include <QInputDialog>
#include <QPinchGesture>
#include <QSwipeGesture>

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
  this->grapMouse

  // Grab some touch gestures
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PinchGesture);

  this->updateSettings();
  this->createMenuActions();
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

void MoveAndZoomableView::addMenuActions(QMenu *menu)
{
  QMenu *zoomMenu = menu->addMenu("Zoom");
  zoomMenu->addAction("Zoom to 1:1", this, &MoveAndZoomableView::resetView, Qt::CTRL + Qt::Key_0);
  zoomMenu->addAction("Zoom to Fit", this, &MoveAndZoomableView::zoomToFit, Qt::CTRL + Qt::Key_9);
  zoomMenu->addAction("Zoom in", this, &MoveAndZoomableView::zoomIn, Qt::CTRL + Qt::Key_Plus);
  zoomMenu->addAction("Zoom out", this, &MoveAndZoomableView::zoomOut, Qt::CTRL + Qt::Key_Minus);
  zoomMenu->addSeparator();
  zoomMenu->addAction("Zoom to 50%", this, &MoveAndZoomableView::zoomTo50);
  zoomMenu->addAction("Zoom to 100%", this, &MoveAndZoomableView::zoomTo100);
  zoomMenu->addAction("Zoom to 200%", this, &MoveAndZoomableView::zoomTo200);
  zoomMenu->addAction("Zoom to ...", this, &MoveAndZoomableView::zoomToCustom);
  
  menu->addAction(&actionFullScreen);
}

void MoveAndZoomableView::resetView(bool checked)
{
  Q_UNUSED(checked);
  this->setMoveOffset(QPoint(0,0));
  this->setZoomFactor(1.0);

  update();
}

void MoveAndZoomableView::zoom(MoveAndZoomableView::ZoomMode zoomMode, QPoint zoomPoint, double newZoomFactor)
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

  auto viewCenter = this->getMoveOffsetCoordinateSystemOrigin(zoomPoint);

  if (zoomPoint.isNull())
    zoomPoint = viewCenter;

  if (!zoomPoint.isNull())
  {
    // The center point has to be moved relative to the zoomPoint
    auto origin = viewCenter;
    auto centerMoveOffset = origin + this->moveOffset;
    
    auto movementDelta = centerMoveOffset - zoomPoint;
    QPoint newMoveOffset;
    if (stepZoomFactor >= 1)
      newMoveOffset = this->moveOffset + movementDelta;
    else
      newMoveOffset = this->moveOffset - stepZoomFactor * movementDelta;

    DEBUG_VIEW("MoveAndZoomableView::zoom point " << newMoveOffset);
    this->setMoveOffset(newMoveOffset);
  }

  this->setZoomFactor(newZoom);
  this->update();

  // TODO: Split view:
  // if (newZoom > 1.0)
  //   update(false, true);  // We zoomed in. Check if one of the items now needs loading
  // else
  //   update();
}

void MoveAndZoomableView::wheelEvent(QWheelEvent *event)
{
  QPoint p = event->pos();
  event->accept();
  this->zoom(event->delta() > 0 ? ZoomMode::IN : ZoomMode::OUT, p);
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
  if (this->viewAction == ViewAction::DRAGGING || this->viewAction == ViewAction::DRAGGING_MOUSE_MOVED && 
    ((mouse_event->button() == Qt::LeftButton  && mouseMode == MOUSE_LEFT_MOVE) ||
     (mouse_event->button() == Qt::RightButton && mouseMode == MOUSE_RIGHT_MOVE)))
  {
    if (mouse_event->button() == Qt::RightButton && this->viewAction == ViewAction::DRAGGING)
    {
      QMenu menu(this);
      this->addMenuActions(&menu);
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
    auto zoomRect = QRect(viewZoomingMousePosStart, mouse_event->pos());
    if (abs(zoomRect.width()) < 2 && abs(zoomRect.height()) < 2)
    {
      // The user just pressed the button without moving the mouse.
      this->viewAction = ViewAction::NONE;
      update();
      return;
    }

    // Now we zoom in as far as possible
    auto additionalZoomFactor = 1.0;
    while (abs(zoomRect.width())  * additionalZoomFactor * ZOOM_STEP_FACTOR <= width() &&
           abs(zoomRect.height()) * additionalZoomFactor * ZOOM_STEP_FACTOR <= height())
      additionalZoomFactor *= ZOOM_STEP_FACTOR;

    // Calculate the new center offset
    const QPoint zoomRectCenterOffset = zoomRect.center() - this->getMoveOffsetCoordinateSystemOrigin(this->viewZoomingMousePosStart);
    this->setMoveOffset((this->moveOffset - zoomRectCenterOffset) * additionalZoomFactor);
    this->setZoomFactor(this->zoomFactor * additionalZoomFactor);

    this->viewAction = ViewAction::NONE;

    this->update();
  }
}

bool MoveAndZoomableView::event(QEvent *event)
{
  if (event->type() == QEvent::Gesture)
  {
    QGestureEvent *gestureEvent = static_cast<QGestureEvent*>(event);

    if (QGesture *swipeGesture = gestureEvent->gesture(Qt::SwipeGesture))
    {
      QSwipeGesture *swipe = static_cast<QSwipeGesture*>(swipeGesture);

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
    if (QGesture *pinchGesture = gestureEvent->gesture(Qt::PinchGesture))
    {
      QPinchGesture *pinch = static_cast<QPinchGesture*>(pinchGesture);

      if (pinch->state() == Qt::GestureStarted)
      {
        // The gesture was just started. This will prevent (generated) mouse events from being interpreted.
        this->viewAction = ViewAction::PINCHING;
      }

      // See what changed in this pinch gesture (the scale factor and/or the position)
      QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
      if (changeFlags & QPinchGesture::ScaleFactorChanged)
        currentStepScaleFactor = pinch->totalScaleFactor();
      if (changeFlags & QPinchGesture::CenterPointChanged)
        currentStepCenterPointOffset += pinch->centerPoint() - pinch->lastCenterPoint();

      // Check if the gesture just finished
      if (pinch->state() == Qt::GestureFinished)
      {
        // Set the new position/zoom
        setZoomFactor(this->zoomFactor * currentStepScaleFactor);
        setMoveOffset(QPointF(QPointF(moveOffset) * currentStepScaleFactor + currentStepCenterPointOffset).toPoint());
        
        // Reset the dynamic values
        currentStepScaleFactor = 1;
        currentStepCenterPointOffset = QPointF(0, 0);
        this->viewAction = ViewAction::NONE;
      }

      // TODO: Pinching and linking
      // if (linkViews)
      // {
      //   otherWidget->currentlyPinching = currentlyPinching;
      //   otherWidget->currentStepScaleFactor = currentStepScaleFactor;
      //   otherWidget->currentStepCenterPointOffset = currentStepCenterPointOffset;
      // }

      event->accept();
      this->update();
    }

    return true;
  }
  else if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel)
  {
    QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
    const auto &touchPoints = touchEvent->touchPoints();
    if (touchPoints.size() >= 1)
    {
      DEBUG_VIEW("MoveAndZoomableView::event handle touch even " << event->type());
      auto pos = touchPoints[0].pos().toPoint();
      if (event->type() == QEvent::TouchBegin)
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
      else
      {
        DEBUG_VIEW("MoveAndZoomableView::event touch dragging end pos " << pos);
        this->setMoveOffset(this->viewDraggingStartOffset + (pos - this->viewDraggingMousePosStart));
        this->viewAction = ViewAction::NONE;
        this->update();
      }
    }
    event->accept();
  }
  else if (event->type() == QEvent::NativeGesture)
  {
    // TODO #195 - For pinching on mac this would have to be added here...
    // QNativeGestureEvent

    // return true;
  }
  else
  {
    //DEBUG_VIEW("MoveAndZoomableView::event unhandled event type " << event->type());
  }

  return QWidget::event(event);
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

void MoveAndZoomableView::createMenuActions()
{
  const bool menuActionsNoteCreatedYet = actionFullScreen.text().isEmpty();
  Q_ASSERT_X(menuActionsNoteCreatedYet, Q_FUNC_INFO, "Only call this initialization function once.");

  auto configureCheckableAction = [this](QAction &action, QActionGroup *actionGroup, QString text, bool checked, void(MoveAndZoomableView::*func)(bool), const QKeySequence &shortcut = {}, bool isEnabled = true)
  {
    action.setParent(this);
    action.setCheckable(true);
    action.setChecked(checked);
    action.setText(text);
    action.setShortcut(shortcut);
    if (actionGroup)
      actionGroup->addAction(&action);
    if (!isEnabled)
      action.setEnabled(false);
    connect(&action, &QAction::triggered, this, func);
  };

  configureCheckableAction(actionZoom[0], nullptr, "Zoom to 1:1", false, &MoveAndZoomableView::resetView, Qt::CTRL + Qt::Key_0);
  configureCheckableAction(actionZoom[1], nullptr, "Zoom to Fit", false, &MoveAndZoomableView::zoomToFit, Qt::CTRL + Qt::Key_9);
  configureCheckableAction(actionZoom[2], nullptr, "Zoom in", false, &MoveAndZoomableView::zoomIn, Qt::CTRL + Qt::Key_Plus);
  configureCheckableAction(actionZoom[3], nullptr, "Zoom out", false, &MoveAndZoomableView::zoomOut, Qt::CTRL + Qt::Key_Minus);
  configureCheckableAction(actionZoom[4], nullptr, "Zoom to 50%", false, &MoveAndZoomableView::zoomTo50);
  configureCheckableAction(actionZoom[5], nullptr, "Zoom to 100%", false, &MoveAndZoomableView::zoomTo100);
  configureCheckableAction(actionZoom[6], nullptr, "Zoom to 200%", false, &MoveAndZoomableView::zoomTo200);
  configureCheckableAction(actionZoom[7], nullptr, "Zoom to ...", false, &MoveAndZoomableView::zoomToCustom);

  configureCheckableAction(actionFullScreen, nullptr, "&Fullscreen Mode", false, &MoveAndZoomableView::toggleFullScreen, Qt::CTRL + Qt::Key_F);
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

  painter.setPen(QPen(QColor(50, 50, 255, 150)));
  painter.setBrush(QBrush(QColor(50, 50, 255, 50)));
  painter.drawRect(QRect(viewZoomingMousePosStart, viewZoomingMousePos));
}

void MoveAndZoomableView::setMoveOffset(QPoint offset)
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

QPoint MoveAndZoomableView::getMoveOffsetCoordinateSystemOrigin(const QPoint &zoomPoint) const
{
  Q_UNUSED(zoomPoint);
  return QWidget::rect().center();
}

/** The common settings might have changed. Reload all settings from the QSettings.
  */
void MoveAndZoomableView::updateSettings()
{
  QSettings settings;

  // Load the mouse mode
  QString mouseModeString = settings.value("MouseMode", "Left Zoom, Right Move").toString();
  if (mouseModeString == "Left Zoom, Right Move")
    this->mouseMode = MOUSE_RIGHT_MOVE;
  else
    this->mouseMode = MOUSE_LEFT_MOVE;
}

void MoveAndZoomableView::zoomToFit(bool checked)
{
  Q_UNUSED(checked);

  // TODO: Zoom to fit for graphs
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

void MoveAndZoomableView::toggleFullScreen(bool checked) 
{ 
  Q_UNUSED(checked);
  emit this->signalToggleFullScreen();
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

void MoveAndZoomableView::slaveSetMoveOffset(QPoint offset)
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