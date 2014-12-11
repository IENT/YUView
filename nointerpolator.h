#ifndef NOINTERPOLATOR_H
#define NOINTERPOLATOR_H

#include "viewinterpolator.h"

class NoInterpolator : public ViewInterpolator
{
public:
    /*! The NoInterpolator doesn't do any view interpolation. It's a dummy
        class which is used in the case where no view interpolation is wanted.
    */
    NoInterpolator();

    virtual bool activate();
    virtual bool deactivate();
    virtual bool interpolate(int currentFrameIdx, bool renderStereo);

private:
};

#endif // NOINTERPOLATOR_H
