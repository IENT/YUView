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

#ifndef DISPLAYWIDGET_H
#define DISPLAYWIDGET_H

#include <QWidget>
#include <QMouseEvent>
#include <QTransform>
#include "displayobject.h"
#include "statisticsobject.h"

class DisplayWidget : public QWidget
{
    Q_OBJECT
public:
    DisplayWidget(QWidget *parent);
    ~DisplayWidget();
    int DisplayWidgetWidth() {return width();}
    int DisplayWidgetHeight() {return height();}
signals:

public slots:

    void setDisplayObject(DisplayObject* newDisplayObject) { p_displayObject = newDisplayObject; }
    void setOverlayStatisticsObject(StatisticsObject* newStatisticsObject) { p_overlayStatisticsObject = newStatisticsObject; }
    DisplayObject* displayObject() { return p_displayObject; }

    void setDisplayRect(QRect displayRect)
    {
        p_displayRect = displayRect;
        if(p_displayObject) { p_displayObject->setInternalScaleFactor( zoomFactor() ); }
        if(p_overlayStatisticsObject) { p_overlayStatisticsObject->setInternalScaleFactor( zoomFactor() ); }
        update();
    }
    QRect displayRect() { return p_displayRect; }
    QRect selectionRect() { return p_selectionRect; }

    double zoomFactor() { return (p_displayObject != NULL && p_displayRect.isEmpty() == false)?((double)p_displayRect.width()/(double)p_displayObject->width()):1.0; }

    // drawing methods
    void drawFrame(int frameIdx);
    void clear();

    void setRegularGridParameters(bool show, int size, QColor gridColor);
    void setSelectionRect(QRect selectionRect) { p_selectionRect = selectionRect; update(); }
    void setZoomBoxPoint(QPoint zoomBoxPoint) { p_zoomBoxPoint = zoomBoxPoint; update(); }

    void resetView();

    QPixmap captureScreenshot();

protected:
     void paintEvent(QPaintEvent *);

private:

     void drawFrame();
     void drawRegularGrid();
     void drawSelectionRectangle();
     void drawZoomBox();
     void drawStatisticsOverlay();
     void drawZoomFactor();

     void rotateVector(float angle, float x, float y, float &nx, float &ny) const;

     // object containing frame to draw
     DisplayObject *p_displayObject;
     StatisticsObject *p_overlayStatisticsObject;

     QRect p_displayRect;

     bool p_drawGrid;
     QColor p_gridColor;
     int p_gridSize;

     QRect p_selectionRect;
     QPoint p_zoomBoxPoint;

};

#endif // DISPLAYWIDGET_H
