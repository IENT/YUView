#ifndef DEFAULTRENDERER_H
#define DEFAULTRENDERER_H

#include "renderer.h"

class DefaultRenderer : public Renderer
{
public:
    DefaultRenderer();

    virtual bool render() const;
    virtual bool activate();
    virtual bool deactivate();

    void setRenderYUV(bool y, bool u, bool v);

private:
    void drawRect() const;
    unsigned int p_shaderProgramHandle;

    static const char* fragmentShaderCode[];
    static const char* vertexShaderCode[];
};

#endif // DEFAULTRENDERER_H
