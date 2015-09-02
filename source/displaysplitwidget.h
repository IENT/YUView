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

#ifndef DISPLAYSPLITWIDGET_H
#define DISPLAYSPLITWIDGET_H

#include <QObject>
#include <QSplitter>
#include <QTouchEvent>
#include <QGesture>
#include <QGestureEvent>
#include "displaywidget.h"
#include "frameobject.h"

#define NUM_VIEWS   2
#define LEFT_VIEW   0
#define RIGHT_VIEW  1

enum ViewMode {SIDE_BY_SIDE, COMPARISON};


class DisplaySplitWidget : public QSplitter
{
    Q_OBJECT
public:
    DisplaySplitWidget(QWidget *parent);
    ~DisplaySplitWidget();

    void setActiveDisplayObjects(DisplayObject *newPrimaryDisplayObject, DisplayObject *newSecondaryDisplayObject );
    void setActiveStatisticsObjects( StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject );

    // external draw methods for a particular index with the video
    void drawFrame(unsigned int frameIdx);
    void drawStatistics(unsigned int frameIdx);

    void clear();

    void setRegularGridParameters(bool show, int size, QColor color);

    void setSplitEnabled(bool enableSplit);
    void setZoomBoxEnabled(bool enabled);

    ViewMode viewMode() {return viewMode_;}
    void setViewMode(ViewMode viewMode) {viewMode_ = viewMode; updateView();}

    QPixmap captureScreenshot();

public slots:
    void splitterMovedTo(int pos, int);

    void zoomIn(QPoint* to=NULL);
    void zoomOut(QPoint* to=NULL);
    void zoomToFit();
    void zoomToStandard();
    void updateView();
    void resetViews();

private:

    void zoomToPoint(DisplayWidget* targetWidget, QPoint zoomPoint, float zoomFactor, bool center);

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void panDisplay(QPoint delta);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void wheelEvent (QWheelEvent *e);
    virtual void resizeEvent(QResizeEvent *);
    bool event(QEvent *event);
    bool gestureEvent(QGestureEvent *event);

    DisplayWidget* p_displayWidgets[NUM_VIEWS];

    int p_LastSplitPos;
    QPoint p_TouchPoint;
    qreal p_TouchScale;
    bool p_BlockMouse;

    // Current rectangular selection
    QPoint p_selectionStartPoint;
    QPoint p_selectionEndPoint;

    // Different selection modes
    enum SelectionMode { NONE, SELECT, DRAG };

    SelectionMode selectionMode_;
    ViewMode viewMode_;
    bool p_zoomBoxEnabled;
    bool p_enableSplit;

    DisplayObject* p_displayObjects[NUM_VIEWS];
    StatisticsObject* p_overlayObjects[NUM_VIEWS];
};

#endif // DISPLAYSPLITWIDGET_H
