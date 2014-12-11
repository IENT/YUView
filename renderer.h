#ifndef RENDERER_H
#define RENDERER_H

class RenderWidget;

class Renderer
{
public:
    /*!
      A Renderer renders a video frame on the screen using OpenGL. As inputs it exspects
      one or two textures to contain video frame images, one for each eye.
      */
    Renderer();
    virtual ~Renderer() {};

    //! Set texture units for input images
    //! Parameters: -1 => unset
    void setInputTextureUnits(int LeftUnit, int RightUnit=-1);

    virtual bool render() const=0;
    virtual bool activate() =0;
    virtual bool deactivate()=0;

    void setRenderSize( int widthOffset, int heightOffset, int width, int height);
    void getRenderSize( int &widthOffset, int &heightOffset, int &width, int &height);
    void setRenderOffset(int horizontal, int vertical);
    void getRenderOffset(int &horizontal, int &vertical);

    void setRenderY(bool y) { setRenderYUV(y, p_renderU, p_renderV); }
    void setRenderU(bool u) { setRenderYUV(p_renderY, u, p_renderV); }
    void setRenderV(bool v) { setRenderYUV(p_renderY, p_renderU, v); }
    virtual void setRenderYUV(bool y, bool u, bool v);

    void setVideoSize(int videoWidth, int videoHeight);

    bool renderY() { return p_renderY != 0.0; }
    bool renderU() { return p_renderU != 0.0; }
    bool renderV() { return p_renderV != 0.0; }
protected:
    void resetViewPort() const;
    int p_leftTextureUnit, p_rightTextureUnit;

    int p_renderWidth, p_renderHeight, p_renderOffsetWidth, p_renderOffsetHeight;
    
    int p_videoWidth, p_videoHeight;

    float p_renderY, p_renderU, p_renderV;
};

#endif // RENDERER_H
