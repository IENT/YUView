#ifndef DIFFERENCEOBJECT_H
#define DIFFERENCEOBJECT_H

#include <QObject>
#include "frameobject.h"

class DifferenceObject : public FrameObject
{
public:
    DifferenceObject(QObject* parent = NULL);
    ~DifferenceObject();

    void setFrameObjects(FrameObject* firstObject, FrameObject* secondObject);

    void loadImage(int frameIdx);
    ValuePairList getValuesAt(int x, int y);

    void setInternalScaleFactor(int internalScaleFactor) {}    // no internal scaling

private:
    FrameObject* p_frameObjects[2];

    void subtractYUV444(QByteArray *srcBuffer0, QByteArray *srcBuffer1, QByteArray *outBuffer, YUVCPixelFormatType srcPixelFormat);
};

#endif // DIFFERENCEOBJECT_H
