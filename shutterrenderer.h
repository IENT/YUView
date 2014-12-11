#ifndef SHUTTERRENDERER_H
#define SHUTTERRENDERER_H

#include "renderer.h"

class ShutterRenderer : public Renderer
{
public:
    ShutterRenderer();

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

#endif // SHUTTERRENDERER_H
