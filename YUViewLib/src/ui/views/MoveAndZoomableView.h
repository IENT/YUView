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
#pragma once

#include <QtWidgets/QWidget>

#include <QAction>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QMenu>

class MoveAndZoomableView : public QWidget
{
  Q_OBJECT

public:
  MoveAndZoomableView(QWidget *parent = 0);

  /* Add a slave view to this. This makes this view a master and the added one
   * a slave.
   */
  void addSlaveView(MoveAndZoomableView *view);

  // Add the split views menu items to the given menu. Used for the main menu bar and the context menu.
  virtual void addMenuActions(QMenu *menu);

signals:
  void signalToggleFullScreen();

public slots:
  virtual void resetView(bool checked = false);
  void toggleFullScreenAction() { actionFullScreen.trigger(); }
  void setLinkState(bool enabled);

protected slots:
  virtual void zoomToFit(bool checked = false);
  void zoomIn(bool checked) { Q_UNUSED(checked); zoom(ZoomMode::IN); }
  void zoomOut(bool checked) { Q_UNUSED(checked); zoom(ZoomMode::OUT); }
  void zoomTo50(bool checked) { Q_UNUSED(checked); zoom(ZoomMode::TO_VALUE, QPoint(), 0.5); }
  void zoomTo100(bool checked) { Q_UNUSED(checked); zoom(ZoomMode::TO_VALUE, QPoint(), 1.0); }
  void zoomTo200(bool checked) { Q_UNUSED(checked); zoom(ZoomMode::TO_VALUE, QPoint(), 2.0); }
  void zoomToCustom(bool checked);

  void toggleFullScreen(bool checked);

protected:

  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void mouseReleaseEvent(QMouseEvent *event) override;
  virtual void wheelEvent (QWheelEvent *event) override;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) override { actionFullScreen.trigger(); event->accept(); }
  virtual void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
  virtual void resizeEvent(QResizeEvent *event) override;
  virtual bool event(QEvent *event) override; ///< Handle touch event

  void update();

  void createMenuActions();
  QAction actionZoom[8];
  QAction actionFullScreen;

  void updateMouseCursor();
  virtual bool updateMouseCursor(const QPoint &srcMousePos);
  
  enum class ZoomMode {IN, OUT, TO_VALUE};
  void zoom(ZoomMode zoomMode, QPoint zoomPoint = QPoint(), double newZoomFactor = 0.0);
  virtual void setZoomFactor(double zoom);
  void drawZoomRect(QPainter &painter) const;
  virtual void onZoomIn() {}

  virtual void setMoveOffset(QPoint offset);
  virtual QPoint getMoveOffsetCoordinateSystemOrigin(const QPoint zoomPoint = {}) const = 0;
  virtual void onZoomRectUpdateOffsetAndZoom(QRect zoomRect, double additionalZoomFactor) = 0;

  enum class ViewAction
  {
    NONE,
    DRAGGING,
    DRAGGING_MOUSE_MOVED,
    DRAGGING_TOUCH,
    PINCHING,
    ZOOM_RECT
  };
  
  virtual void updateSettings();

  virtual void onSwipeLeft() {}
  virtual void onSwipeRight() {}
  virtual void onSwipeUp() {}
  virtual void onSwipeDown() {}

  double zoomFactor {1.0};
  const int ZOOM_STEP_FACTOR = 2;

  QPoint moveOffset;                 //!< The offset that the view was moved
  QPoint viewZoomingMousePosStart;
  QPoint viewZoomingMousePos;

  ViewAction viewAction {ViewAction::NONE};

  // Two modes of mouse operation can be set for the view:
  // 1: The right mouse button moves the view, the left one draws the zoom box
  // 2: The other way around
  enum mouseModeEnum {MOUSE_RIGHT_MOVE, MOUSE_LEFT_MOVE};
  mouseModeEnum mouseMode;

  /* Views can be linked together. There is always one mater view and a list of 
   * slave views. If the link is enabled, all interactive commands are pushed to 
   * the master and that will forward them to all slaves.
   */
  bool enableLink {false};
  bool isMasterView {true};
  bool updateLinkedViews {false};
  QList<QPointer<MoveAndZoomableView>> slaveViews;
  QPointer<MoveAndZoomableView> masterView;

  const QPen   ZOOM_RECT_PEN   = QPen(QColor(50, 50, 255, 150));
  const QBrush ZOOM_RECT_BRUSH = QBrush(QColor(50, 50, 255, 50));

  virtual void getStateFromMaster();

private:
  QPoint viewDraggingMousePosStart;
  QPoint viewDraggingStartOffset;

  double  pinchStartZoomFactor {1.0};
  QPointF pinchStartMoveOffset;
  QPointF pinchStartCenterPoint;

  void slaveSetLinkState(bool enabled);
  void slaveSetMoveOffset(QPoint offset);
  void slaveSetZoomFactor(double zoom);
  //void slaveSetPinchValues(double scaleFactor, QPointF centerPointOffset);
  void slaveUpdateWidget();
};
