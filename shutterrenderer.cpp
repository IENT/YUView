#include "shutterrenderer.h"

#include "glew.h"
#include <QGLWidget>
#include <QGLFramebufferObject>
#include <QGLPixelBuffer>
#include <QGLBuffer>

#include "renderwidget.h"

#ifdef Q_OS_MAC
#include <OpenGL/glu.h>
#endif

const char* ShutterRenderer::fragmentShaderCode[] = {
    "uniform sampler2DRect textureYUV;",
    "uniform float useY, useU, useV;",
    "void main()",
    "{",
        "float y = useY * 1.1643 * ( texture2DRect(textureYUV, gl_TexCoord[0].st).r - 0.0625);",
        "float u = useU * (texture2DRect(textureYUV, gl_TexCoord[0].st).g - 0.5);",
        "float v = useV * (texture2DRect(textureYUV, gl_TexCoord[0].st).b - 0.5);",

        "float r = y + 1.5958 * v;",
        "float g = y - 0.39173 * u - 0.81290 * v;",
        "float b = y + 2.017 * u;",

        "gl_FragColor = vec4(r,g,b,1.0);",
     "}"
 };

const char* ShutterRenderer::vertexShaderCode[] = {
    "void main()",
    "{",
        "gl_TexCoord[0] = gl_MultiTexCoord1;",
        "gl_Position = ftransform();",
    "}"
};


ShutterRenderer::ShutterRenderer() : Renderer(), p_shaderProgramHandle(0)
{
}

bool ShutterRenderer::render() const {

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDrawBuffer(GL_BACK);

    resetViewPort();

    glMatrixMode (GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, p_videoWidth, 0, p_videoHeight, -1., 1.);
    glMatrixMode (GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glClearColor (0.5, 0.5, 0.5, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    drawRect();

    glPopMatrix();
    glMatrixMode (GL_PROJECTION);
    glPopMatrix();

    return true;
}

bool ShutterRenderer::activate() {
    GLint fragmentShaderHandle, vertexShaderHandle;

    // create fragment shader
    fragmentShaderHandle = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragmentShaderHandle, 12, fragmentShaderCode, 0);
    glCompileShader(fragmentShaderHandle);

    //create vertex shader
    vertexShaderHandle = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertexShaderHandle, 5, vertexShaderCode, 0);
    glCompileShader(vertexShaderHandle);

    // now create the shader program
    p_shaderProgramHandle = glCreateProgram();

    glAttachShader(p_shaderProgramHandle, fragmentShaderHandle);
    glAttachShader(p_shaderProgramHandle, vertexShaderHandle);

    glLinkProgram(p_shaderProgramHandle);
    glUseProgram(p_shaderProgramHandle);

    GLint textureSampler = glGetUniformLocation(p_shaderProgramHandle, "textureYUV");
    glUniform1i(textureSampler, 1);

    setRenderYUV(true, true, true);

    int status;
    glGetShaderiv(vertexShaderHandle, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
        return false;
    glGetShaderiv(fragmentShaderHandle, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
        return false;
    glGetShaderiv(p_shaderProgramHandle, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
        return false;

    return true;
}

bool ShutterRenderer::deactivate() {
    GLsizei count;
    GLuint shaders[10];

    if (p_shaderProgramHandle == 0)
        return false;
    if (glIsProgram(p_shaderProgramHandle) != GL_TRUE)
        return false;

    glGetAttachedShaders(p_shaderProgramHandle,  10, &count, shaders);
    for (int i=0; i< count; i++)
        glDeleteShader(shaders[i]);
    glDeleteProgram(p_shaderProgramHandle);
    p_shaderProgramHandle = 0;

    return true;
}

void ShutterRenderer::setRenderYUV(bool y, bool u, bool v) {
    Renderer::setRenderYUV(y,u,v);

    glUseProgram(p_shaderProgramHandle);

    GLint useY = glGetUniformLocation(p_shaderProgramHandle, "useY");
    glUniform1f(useY, p_renderY);

    GLint useU = glGetUniformLocation(p_shaderProgramHandle, "useU");
    glUniform1f(useU, p_renderU);

    GLint useV = glGetUniformLocation(p_shaderProgramHandle, "useV");
    glUniform1f(useV, p_renderV);

    glUseProgram(0);
}

void ShutterRenderer::drawRect() const
{
    glUseProgram(p_shaderProgramHandle);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_leftTextureUnit);

    glDrawBuffer(GL_BACK_LEFT);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
        glMultiTexCoord2f(GL_TEXTURE1, 0, (p_videoHeight));
        glVertex3f(0.0, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, (p_videoWidth), (p_videoHeight));
        glVertex3f( p_videoWidth, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, (p_videoWidth), 0);
        glVertex3f( p_videoWidth, p_videoHeight, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
       glVertex3f(0.0, p_videoHeight, 0.5f);
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_rightTextureUnit);

    glDrawBuffer(GL_BACK_RIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
        glMultiTexCoord2f(GL_TEXTURE1, 0, p_videoHeight);
        glVertex3f(0.0, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, p_videoWidth, p_videoHeight);
        glVertex3f( p_videoWidth, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, p_videoWidth, 0);
        glVertex3f( p_videoWidth, p_videoHeight, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
       glVertex3f(0.0, p_videoHeight, 0.5f);
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glUseProgram(0);
}
