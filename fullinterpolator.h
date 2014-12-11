#ifndef FULLINTERPOLATOR_H
#define FULLINTERPOLATOR_H

#include "viewinterpolator.h"

class FullInterpolator : public ViewInterpolator
{
public:
    /*! The NoInterpolator doesn't do any view interpolation. It's a dummy
        class which is used in the case where no view interpolation is wanted.
        However, it does load the YUV-File to textures and converts them to one
        RGB texture, so the subsequent shaders (in the Renderers) can handle them.
    */
    FullInterpolator();

    void setCurrentRenderObjects(YUVObject *newObjectLeft, YUVObject *newObjectRight);
    void setCurrentDepthObjects(YUVObject *newObjectLeft, YUVObject *newObjectRight);
    virtual bool activate();
    virtual bool deactivate();
    virtual bool interpolate(int currentFrameIdx, bool renderStereo);
    virtual void setOutputTextureUnits(int LeftUnit, int RightUnit);

private:
    // view synthesis functions
    bool DoViewWarping(YUVObject* sourceObject, YUVObject* depthObject, int outputTexture1, int outputTexture2);
    bool DoViewInpainting(YUVObject* sourceObject, int inputTexture, int outputTexture);
    bool DoViewBlending(YUVObject *sourceObject, int inputTexture1, int inputTexture2, int outputTexture);

    void initializeBuffers();
    bool prepareFramebufferTextures();

    bool loadFrame(int currentFrameIdx, rightOrLeft_t rightOrLeft, int depthBufferUnit);

    GLuint p_gl_buffers[2];

    GLuint p_gl_textures[6];

    GLuint p_fbo, gl_renderbuffer;

    GLuint  p_pointCloudShaderProgram, p_blendingShaderProgram, p_inpaintingShaderProgram;
    static const char* warpingFragShaderCode[];
    static const char* warpingVertShaderCode[];
    static const char* inpaintFragShaderCode[];
    static const char* blendingFragShaderCode[];
    static const char* noBlendingFragShaderCode[];
    static const char* passthroughVertShaderCode[];

    float p_perspectiveCorrectedInterpolationFactor;
};

#endif // FULLINTERPOLATOR_H
