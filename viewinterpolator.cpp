#include "viewinterpolator.h"

ViewInterpolator::ViewInterpolator():
    p_currentInpaintingMode(no_inpainting),
    p_currentBlendingMode(no_blending),
    p_currentRenderObjectLeft(0),
    p_currentRenderObjectRight(0),
    p_currentDepthObjectLeft(0),
    p_currentDepthObjectRight(0),
    p_renderFromRightFrame(false),
    p_leftOutputTextureUnit(0),
    p_rightOutputTextureUnit(0),
    p_interpolationFactor(0.0)
{
}

void ViewInterpolator::setOutputTextureUnits(int LeftUnit, int RightUnit) {
    p_leftOutputTextureUnit = LeftUnit;
    p_rightOutputTextureUnit = RightUnit;
}

bool ViewInterpolator::loadFrame(int currentFrameIdx, rightOrLeft_t rightOrLeft)
{
    // Just load the frames to the texture units
    if (rightOrLeft == leftView)
    {
        if (p_currentRenderObjectLeft)
        {
            p_currentRenderObjectLeft->loadFrameToTexture(currentFrameIdx);
            return true;
        }
        else
            return false;
    }
    else
    {
        if ((p_currentRenderObjectRight) && (p_renderFromRightFrame))
        {
            p_currentRenderObjectRight->loadFrameToTexture(currentFrameIdx);
            p_renderFromRightFrame = true;
            return true;
        }
        else
        {
            p_renderFromRightFrame = false;
            return false;
        }
    }
    return false;
}

void ViewInterpolator::setCurrentRenderObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight ) {
    p_currentRenderObjectLeft = newObjectLeft;
    p_currentRenderObjectRight = newObjectRight;
    p_renderFromRightFrame = (p_currentRenderObjectRight != 0);
}

void ViewInterpolator::setCurrentDepthObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight ) {
    p_currentDepthObjectLeft = newObjectLeft;
    p_currentDepthObjectRight = newObjectRight;
}

void ViewInterpolator::setInterpolationFactor(float factor) {
    p_interpolationFactor = factor;
}

void ViewInterpolator::setEyeDistance(double dist) {
    p_eyeDistance = dist;
}

void ViewInterpolator::setInpainting(inpainting_t inpaintingMode) {
    p_currentInpaintingMode = inpaintingMode;
}

void ViewInterpolator::setBlendingMode(blending_t blendingMode) {
    p_currentBlendingMode = blendingMode;
    deactivate();
    activate();
}
