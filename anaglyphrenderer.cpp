#include "anaglyphrenderer.h"

#include "glew.h"
#include <QGLWidget>
#include <QGLFramebufferObject>
#include <QGLPixelBuffer>
#include <QGLBuffer>

#include "renderwidget.h"

#ifdef Q_OS_MAC
#include <OpenGL/glu.h>
#endif

const char* AnaglyphRenderer::fragmentShaderCode[] = {
    "uniform sampler2DRect textureYUV_L, textureYUV_R;\n",
    "uniform float useY, useU, useV;",
    "void main()\n",
    "{\n",
        "float y_l = useY * 1.1643 * ( texture2DRect(textureYUV_L, gl_TexCoord[0].st).r - 0.0625);",
        "float u_l = useU * (texture2DRect(textureYUV_L, gl_TexCoord[0].st).g - 0.5);",
        "float v_l = useV * (texture2DRect(textureYUV_L, gl_TexCoord[0].st).b - 0.5);",
        "float y_r = useY * 1.1643 * ( texture2DRect(textureYUV_R, gl_TexCoord[1].st).r - 0.0625);",
        "float u_r = useU * (texture2DRect(textureYUV_R, gl_TexCoord[1].st).g - 0.5);",
        "float v_r = useV * (texture2DRect(textureYUV_R, gl_TexCoord[1].st).b - 0.5);",

        "float r_l = y_l + 1.5958 * v_l;",
        "float g_r = y_r - 0.39173 * u_r - 0.81290 * v_r;",
        "float b_r = y_r + 2.017 * u_r;",

        "gl_FragColor = vec4(r_l, g_r, b_r, 1.0);\n",
     "}\n"
 };

const char* AnaglyphRenderer::vertexShaderCode[] = {
  "void main(void) {\n",
  "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n",
  "  gl_TexCoord[0] = gl_MultiTexCoord1;\n",
  "  gl_TexCoord[1] = gl_MultiTexCoord2;\n",
  "}\n"};

AnaglyphRenderer::AnaglyphRenderer() : Renderer(), p_shaderProgramHandle(0)
{
}

bool AnaglyphRenderer::render() const {
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

bool AnaglyphRenderer::activate() {
    GLint fragmentShaderHandle, vertexShaderHandle;

    // create fragment shader
    fragmentShaderHandle = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragmentShaderHandle, 15, fragmentShaderCode, 0);
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

    GLint textureSamplerL = glGetUniformLocation(p_shaderProgramHandle, "textureYUV_L");
    glUniform1i(textureSamplerL, 1);

    GLint textureSamplerR = glGetUniformLocation(p_shaderProgramHandle, "textureYUV_R");
    glUniform1i(textureSamplerR, 2);

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

bool AnaglyphRenderer::deactivate() {
    GLsizei count;
    GLuint shaders[10];

    if (p_shaderProgramHandle == 0)
        return false;
    if (glIsProgram(p_shaderProgramHandle) != GL_TRUE)
        return false;

    glGetAttachedShaders(p_shaderProgramHandle,  10, &count, shaders);
    for (int i=0; i<count; i++)
        glDeleteShader(shaders[i]);
    glDeleteProgram(p_shaderProgramHandle);
    p_shaderProgramHandle = 0;

    return true;
}

void AnaglyphRenderer::setRenderYUV(bool y, bool u, bool v) {
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

void AnaglyphRenderer::drawRect() const
{
    glUseProgram(p_shaderProgramHandle);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_leftTextureUnit);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_rightTextureUnit);

    glBegin(GL_QUADS);
        glMultiTexCoord2f(GL_TEXTURE1, 0, p_videoHeight);
        glMultiTexCoord2f(GL_TEXTURE2, 0, p_videoHeight);
        glVertex3f(0.0, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, p_videoWidth, p_videoHeight);
        glMultiTexCoord2f(GL_TEXTURE2, p_videoWidth, p_videoHeight);
        glVertex3f( p_videoWidth, 0.0, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, p_videoWidth, 0);
        glMultiTexCoord2f(GL_TEXTURE2, p_videoWidth, 0);
        glVertex3f( p_videoWidth, p_videoHeight, 0.5f);

        glMultiTexCoord2f(GL_TEXTURE1, 0, 0);
        glMultiTexCoord2f(GL_TEXTURE2, 0, 0);
       glVertex3f(0.0, p_videoHeight, 0.5f);
    glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glUseProgram(0);
}
