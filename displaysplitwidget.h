#ifndef DISPLAYSPLITWIDGET_H
#define DISPLAYSPLITWIDGET_H

#include <QObject>
#include <QSplitter>
#include "displaywidget.h"
#include "frameobject.h"

#define NUM_VIEWS 2

enum ViewMode {SIDE_BY_SIDE, STANDARD, COMPARISON};


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

    void setZoomFactor(float zoomFactor) { p_zoomFactor = zoomFactor; }
    void setZoomBoxEnabled(bool enabled);

    void setCurrentMousePosition(QPoint mousePos) { p_currentMousePosition = mousePos; }
    ViewMode viewMode() {return viewMode_;}
    void setViewMode(ViewMode viewMode) {viewMode_ = viewMode; updateView();}

public slots:
    void splitterMovedTo(int pos, int index);

    void zoomIn(QPoint* to=NULL);
    void zoomOut(QPoint* to=NULL);
    void zoomToFit();
    void zoomToStandard();
    void updateView();
    void resetViews();

private:

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void wheelEvent (QWheelEvent *e);

    DisplayWidget* p_displayWidgets[NUM_VIEWS];

    int p_zoomFactor;
    QRect p_zoomedRectPrimary;
    QRect p_zoomedRectSecondary;

    // Current rectangular selection
    QPoint p_selectionStartPoint;
    QPoint p_selectionEndPoint;
    QPoint p_currentMousePosition;

    // Different selection modes
    enum SelectionMode { NONE, SELECT, DRAG };

    SelectionMode selectionMode_;
    ViewMode viewMode_;
    bool p_zoomBoxEnabled;

    DisplayObject* p_displayObjects[NUM_VIEWS];
    StatisticsObject* p_overlayObjects[NUM_VIEWS];
};

#endif // DISPLAYSPLITWIDGET_H
