#ifndef VIEWINTERPOLATOR_H
#define VIEWINTERPOLATOR_H

#include "yuvobject.h"

enum inpainting_t  {
    no_inpainting = 0,
    horizontal_inpainting
};

enum blending_t {
    no_blending = 0,
    weighted_blending
};

enum rightOrLeft_t {
    leftView = 0,
    rightView
};

class ViewInterpolator
{
public:
    /*!
      A View Interpolator loads a video frame and does view interpolation.
      Output will be one or two texture units.
      */
    ViewInterpolator();
    virtual ~ViewInterpolator() {}

    virtual void setCurrentRenderObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight=0 );
    virtual void setCurrentDepthObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight=0 );

    void setInpainting(inpainting_t inpaintingMode);
    void setBlendingMode(blending_t blendingMode);

    virtual void setInterpolationFactor(float factor);
    virtual void setEyeDistance(double dist);

    //! Set texture units for output images
    //! Parameters: -1 => unset
    virtual void setOutputTextureUnits(int LeftUnit, int RightUnit=-1);

    // initializes opengl stuff, so the context should be ready
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;

    virtual bool interpolate(int currentFrameIdx, bool renderStereo)=0;
protected:

    bool loadFrame(int currentFrameIdx, rightOrLeft_t rightOrLeft=leftView);

    inpainting_t p_currentInpaintingMode;
    blending_t p_currentBlendingMode;

    YUVObject *p_currentRenderObjectLeft, *p_currentRenderObjectRight, *p_currentDepthObjectLeft, *p_currentDepthObjectRight;

    bool p_renderFromRightFrame;

    GLuint p_leftOutputTextureUnit;
    GLuint p_rightOutputTextureUnit;

    float p_interpolationFactor;
    double p_eyeDistance;
};

#endif // VIEWINTERPOLATOR_H
