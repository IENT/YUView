#include "renderer.h"

#include "glew.h"

#ifdef Q_OS_MAC
#include <OpenGL/glu.h>
#endif

Renderer::Renderer():
    p_leftTextureUnit(-1),
    p_rightTextureUnit(-1),
    p_renderWidth(0),
    p_renderHeight(0),
    p_renderY(1.0),
    p_renderU(1.0),
    p_renderV(1.0)
{
}

void Renderer::setInputTextureUnits(int LeftUnit, int RightUnit) {
    p_leftTextureUnit = LeftUnit;
    p_rightTextureUnit = RightUnit;
}

void Renderer::setRenderSize( int widthOffset, int heightOffset, int width, int height) {
    p_renderOffsetWidth = widthOffset;
    p_renderOffsetHeight = heightOffset;
    p_renderWidth = width;
    p_renderHeight = height;
    resetViewPort();
}

void Renderer::getRenderSize( int &widthOffset, int &heightOffset, int &width, int &height) {
    widthOffset = p_renderOffsetWidth;
    heightOffset = p_renderOffsetHeight;
    width = p_renderWidth;
    height = p_renderHeight;
}

void Renderer::setRenderOffset( int widthOffset, int heightOffset) {
    p_renderOffsetWidth = widthOffset;
    p_renderOffsetHeight = heightOffset;
    resetViewPort();
}

void Renderer::getRenderOffset(int &horizontal, int &vertical) {
    horizontal = p_renderOffsetWidth;
    vertical = p_renderOffsetHeight;
}

void Renderer::setVideoSize(int videoWidth, int videoHeight) {
    p_videoWidth = videoWidth;
    p_videoHeight = videoHeight;
}

void Renderer::resetViewPort() const {
    glViewport(p_renderOffsetWidth, p_renderOffsetHeight, p_renderWidth, p_renderHeight);
}

void Renderer::setRenderYUV(bool y, bool u, bool v) {
    p_renderY = y ? 1.0f : 0.0f;
    p_renderU = u ? 1.0f : 0.0f;
    p_renderV = v ? 1.0f : 0.0f;
}
