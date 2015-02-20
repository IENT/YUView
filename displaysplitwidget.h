#ifndef DISPLAYSPLITWIDGET_H
#define DISPLAYSPLITWIDGET_H

#include <QObject>
#include <QSplitter>
#include "displaywidget.h"
#include "yuvobject.h"

class DisplaySplitWidget : public QSplitter
{
public:
    DisplaySplitWidget(QWidget *parent);
    ~DisplaySplitWidget();

    void setActiveFrameObjects( YUVObject* newPrimaryFrameObject, YUVObject* newSecondaryFrameObject );
    //void setActiveStatisticsObjects( StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject );

    // external draw methods for a particular index with the video
    void drawFrame(unsigned int frameIdx);
    void drawStatistics(unsigned int frameIdx);

private:
    DisplayWidget* p_primaryDisplayWidget;
    DisplayWidget* p_secondaryDisplayWidget;
};

#endif // DISPLAYSPLITWIDGET_H
