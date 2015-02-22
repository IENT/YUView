#include "displayobject.h"

DisplayObject::DisplayObject(QObject *parent) : QObject(parent)
{
    // preset internal values
    p_startFrame = 0;
    p_sampling = 1;
    p_numFrames = -1;
    p_frameRate = -1;

    p_width = -1;
    p_height = -1;

    p_lastIdx = INT_MAX;    // initialize with magic number ;)

    // listen to signal
    QObject::connect(this, SIGNAL(informationChanged(uint)), this, SLOT(refreshDisplayImage()));
}

DisplayObject::~DisplayObject()
{

}
