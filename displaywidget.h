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

public slots:

    void setDisplayObject(DisplayObject* newDisplayObject) { p_displayObject = newDisplayObject; }
    void setOverlayStatisticsObject(StatisticsObject* newStatisticsObject) { p_overlayStatisticsObject = newStatisticsObject; }
    DisplayObject* displayObject() {return p_displayObject;}
    void setDisplayRect(QRect displayRect) { p_displayRect = displayRect; update(); }

    // drawing methods
    void drawFrame(unsigned int frameIdx);
    void clear();

    void setRegularGridParameters(bool show, int size, QColor gridColor);
    void setSelectionRect(QRect selectionRect) { p_selectionRect = selectionRect; update(); }
    void setZoomBoxPoint(QPoint zoomBoxPoint) { p_zoomBoxPoint = zoomBoxPoint; update(); }
    void centerView();

protected:
     void paintEvent(QPaintEvent * event);

private:

     void drawFrame();
     void drawRegularGrid();
     void drawSelectionRectangle();
     void drawZoomBox();
     void drawStatisticsOverlay();

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
