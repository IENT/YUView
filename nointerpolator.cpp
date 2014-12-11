#include "nointerpolator.h"

#include <sstream>
#include <iostream>

NoInterpolator::NoInterpolator()
{
}

bool NoInterpolator::activate() { return true; }
bool NoInterpolator::deactivate() { return true; }

bool NoInterpolator::interpolate(int currentFrameIdx, bool renderStereo)
{
    if (p_currentRenderObjectLeft == 0)
        return false;

    // load left frame
    loadFrame(currentFrameIdx, leftView);

    // load right eye view
    if (renderStereo)
        loadFrame(currentFrameIdx, rightView);

    return false;
}
