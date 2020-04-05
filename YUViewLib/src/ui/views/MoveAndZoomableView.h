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

#include <QtWidgets/QWidget>

#include <QWheelEvent>
#include <QMouseEvent>
#include <QPointer>

class MoveAndZoomableView : public QWidget
{
  Q_OBJECT

public:
  MoveAndZoomableView(QWidget *parent = 0);

protected:

  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent (QWheelEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override { event->ignore(); }
  void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
  void resizeEvent(QResizeEvent *event) override;

  void openContextMenu() { }

  // Use the current mouse position within the widget to update the mouse cursor.
  void updateMouseCursor();
  virtual void updateMouseCursor(const QPoint &srcMousePos);

  double zoomFactor {1.0};
  enum class ZoomDirection {BOTH, HORIZONTAL};
  ZoomDirection zoomDirection {ZoomDirection::HORIZONTAL};
  enum class ZoomMode {IN, OUT, TO_VALUE};
  void zoom(ZoomMode zoomMode, const QPoint &zoomPoint = QPoint(), double newZoomFactor = 0.0);

  // When touching (pinch/swipe) these values are updated to enable interactive zooming
  double  currentStepScaleFactor {1.0};
  QPointF currentStepCenterPointOffset;
  bool    currentlyPinching {false};

  void    setCenterOffset(QPoint offset, bool setOtherViewIfLinked = true, bool callUpdate = false);
  QPoint  centerOffset;                     //!< The offset of the view to the center (0,0)
  bool    viewDragging {false};             //!< True if the user is currently moving the view
  bool    viewDraggingMouseMoved {false};   //!< If the user is moving the view this indicates if the view was actually moved more than a few pixels
  QPoint  viewDraggingMousePosStart;
  QPoint  viewDraggingStartOffset;
  bool    viewZooming {false};              //!< True if the user is currently zooming using the mouse (zoom box)
  QPoint  viewZoomingMousePosStart;
  QPoint  viewZoomingMousePos;

  // Two modes of mouse operation can be set for the view:
  // 1: The right mouse button moves the view, the left one draws the zoom box
  // 2: The other way around
  enum mouseModeEnum {MOUSE_RIGHT_MOVE, MOUSE_LEFT_MOVE};
  mouseModeEnum mouseMode;

  bool enableLink {false};
  QList<QPointer<MoveAndZoomableView>> linkedViews;

  void updateSettings();

  QPoint moveOffset; //!< The offset of the view
};
