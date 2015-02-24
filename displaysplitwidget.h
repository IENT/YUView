#ifndef DISPLAYSPLITWIDGET_H
#define DISPLAYSPLITWIDGET_H

#include <QObject>
#include <QSplitter>
#include "displaywidget.h"
#include "frameobject.h"

class DisplaySplitWidget : public QSplitter
{
public:
    DisplaySplitWidget(QWidget *parent);
    ~DisplaySplitWidget();

    void setActiveDisplayObjects(DisplayObject *newPrimaryDisplayObject, DisplayObject *newSecondaryDisplayObject );
    void setActiveStatisticsObjects( StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject );

    // external draw methods for a particular index with the video
    void drawFrame(unsigned int frameIdx);
    void drawStatistics(unsigned int frameIdx);

    void clear();

    void setRegularGridParameters(bool show, int size, unsigned char color[4]);

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private:
    DisplayWidget* p_primaryDisplayWidget;
    DisplayWidget* p_secondaryDisplayWidget;
};

#endif // DISPLAYSPLITWIDGET_H
