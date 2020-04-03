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

const int ZOOM_STEP_FACTOR = 2;

MoveAndZoomableView::MoveAndZoomableView(QWidget *parent) 
  : QWidget(parent)
{
  
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
