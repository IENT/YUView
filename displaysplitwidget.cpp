#include "displaysplitwidget.h"

DisplaySplitWidget::DisplaySplitWidget(QWidget *parent)
{
    p_primaryDisplayWidget = new DisplayWidget(this);
    p_secondaryDisplayWidget = new DisplayWidget(this);

    this->addWidget(p_primaryDisplayWidget);
    this->addWidget(p_secondaryDisplayWidget);
}

DisplaySplitWidget::~DisplaySplitWidget()
{
    delete p_primaryDisplayWidget;
    delete p_secondaryDisplayWidget;
}

void DisplaySplitWidget::setActiveFrameObjects( YUVObject* newPrimaryFrameObject, YUVObject* newSecondaryFrameObject )
{
    p_primaryDisplayWidget->setFrameObject(newPrimaryFrameObject);
    p_secondaryDisplayWidget->setFrameObject(newSecondaryFrameObject);
}

// triggered from timer in application
void DisplaySplitWidget::drawFrame(unsigned int frameIdx)
{
    // propagate the draw request to worker widgets
    p_primaryDisplayWidget->drawFrame(frameIdx);
    p_secondaryDisplayWidget->drawFrame(frameIdx);
}
