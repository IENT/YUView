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

signals:
    void isFocus();

public slots:

    void setDisplayObject(DisplayObject* newDisplayObject) { p_displayObject = newDisplayObject; }
    void setOverlayStatisticsObject(StatisticsObject* newStatisticsObject) { p_overlayStatisticsObject = newStatisticsObject; }

    // drawing methods
    void drawFrame(unsigned int frameIdx);
    void clear();

    void drawZoomBox(QPoint mousePos);

    void setRegularGridParameters(bool show, int size, unsigned char color[4]);

    void zoomIn(QPoint *to=0, float factor=1.2);
    void zoomOut(QPoint *to=0, float factor=1.2);
    void zoomToFit();
    void zoomToStandard();
    void setZoomFactor(float zoomFactor) { p_zoomFactor = zoomFactor; }
    void setZoomBoxEnabled(bool enabled);

    //bool getRenderSize(int &xOffset, int &yOffset, int &width, int &height);

    void setCurrentMousePosition(QPoint mousePos) { p_currentMousePosition = mousePos; }

protected:
     void paintEvent(QPaintEvent * event);
     void resizeEvent(QResizeEvent * event);

     virtual void mousePressEvent(QMouseEvent *e);
     virtual void mouseMoveEvent(QMouseEvent *e);
     virtual void mouseReleaseEvent(QMouseEvent *e);
     virtual void wheelEvent (QWheelEvent *e);

private:

     void drawFrame();
     void drawRegularGrid();
     void drawSelectionRectangle();
     void drawStatisticsOverlay();

     void rotateVector(float angle, float x, float y, float &nx, float &ny) const;

     // object containing frame to draw
     DisplayObject *p_displayObject;
     StatisticsObject *p_overlayStatisticsObject;

     float p_videoHeight;
     float p_videoWidth;

     QTransform p_transform;

     float p_zoomFactor;
     float p_boxZoomFactor;

     bool p_drawStatisticsOverlay;

     bool p_drawGrid;
     unsigned char p_gridColor[4];
     int p_gridSize;

     // Current rectangular selection
     QPoint p_selectionStartPoint;
     QPoint p_selectionEndPoint;
     QPoint p_currentMousePosition;
     QRect p_selectionRect;
     int p_dragRenderOffsetStartX, p_dragRenderOffsetStartY;

     // Different selection modes
     enum SelectionMode { NONE, SELECT, DRAG };
     SelectionMode selectionMode_;
     bool p_zoomBoxEnabled;
     bool p_customViewport;
};

#endif // DISPLAYWIDGET_H
