#ifndef LEFTRIGHTLINERENDERER_H
#define LEFTRIGHTLINERENDERER_H

#include "renderer.h"

class LeftRightLineRenderer : public Renderer
{
public:
    LeftRightLineRenderer();

    virtual bool render() const;
    virtual bool activate();
    virtual bool deactivate();

    void setRenderYUV(bool y, bool u, bool v);

    void setRenderWidget(RenderWidget* w) { p_renderWidget = w; }

private:
    void drawRect() const;
    unsigned int p_shaderProgramHandle;

    static const char* fragmentShaderCode[];
    static const char* vertexShaderCode[];

    RenderWidget* p_renderWidget;
};

#endif // LEFTRIGHTLINERENDERER_H
