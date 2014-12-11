#ifndef ANAGLYPHRENDERER_H
#define ANAGLYPHRENDERER_H

#include "renderer.h"

class AnaglyphRenderer : public Renderer
{
public:
    AnaglyphRenderer();

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

#endif // ANAGLYPHRENDERER_H
