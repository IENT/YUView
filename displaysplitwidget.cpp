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

void DisplaySplitWidget::setActiveDisplayObjects( DisplayObject* newPrimaryDisplayObject, DisplayObject* newSecondaryDisplayObject )
{
    p_primaryDisplayWidget->setDisplayObject(newPrimaryDisplayObject);
    p_secondaryDisplayWidget->setDisplayObject(newSecondaryDisplayObject);
}

// triggered from timer in application
void DisplaySplitWidget::drawFrame(unsigned int frameIdx)
{
    // propagate the draw request to worker widgets
    p_primaryDisplayWidget->drawFrame(frameIdx);
    p_secondaryDisplayWidget->drawFrame(frameIdx);
}
