#ifndef DISPLAYWIDGET_H
#define DISPLAYWIDGET_H

#include <QWidget>
#include <QMouseEvent>
#include "displayobject.h"
#include "statisticsparser.h"

class DisplayWidget : public QWidget
{
    Q_OBJECT
public:
    DisplayWidget(QWidget *parent);
    ~DisplayWidget();

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

signals:
    void isFocus();

public slots:

    void setDisplayObject(DisplayObject* newDisplayObject) { p_displayObject = newDisplayObject; }
    //void setStatisticsObject(YUVObject* newStatisticsObject) { p_frameObject = newFrameObject; }

    // drawing methods
    void drawFrame(unsigned int frameIdx);

    void setCurrentStatistics(StatisticsParser* stats, QVector<StatisticsRenderItem> &renderTypes);

    void drawSelectionRectangle();
    void drawZoomBox(QPoint mousePos);
    //void drawStatistics(Statistics& stats, StatisticsRenderItem &item);

    void setGridParameters(bool show, int size, unsigned char color[4]);
    //void setStatisticsParameters(bool doSimplify, int threshold, unsigned char color[3]);

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

     //void drawStatistics(Statistics &stats, StatisticsRenderItem &item) const;

     void drawSelectionRectangle() const;

     void rotateVector(float angle, float x, float y, float &nx, float &ny) const;

     // object containing frame to draw
     DisplayObject *p_displayObject;

     // object containting statistics to draw
     //StatisticsObject* p_statisticsObject;

     float p_videoHeight;
     float p_videoWidth;

     float p_zoomFactor;
     float p_boxZoomFactor;

     bool p_drawGrid;
     unsigned char p_gridColor[4];
     int p_gridSize;

     bool p_simplifyStats;
     int p_simplificationTheshold;
     unsigned char p_simplifiedGridColor[3];
     StatisticsParser *p_currentStatsParser;
     QVector<StatisticsRenderItem> p_renderStatsTypes; // contains all type-IDs of stats that should be rendered (in order)

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
