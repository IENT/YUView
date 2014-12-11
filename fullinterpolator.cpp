#include "fullinterpolator.h"

#include <sstream>
#include <iostream>

const char* FullInterpolator::warpingFragShaderCode[] = {
    "uniform sampler2DRect textureYUV;",
      "varying float d;",
    "void main(void) {",
    "  float y,u,v;",
    "  y=texture2DRect(textureYUV,gl_TexCoord[0].st).r;",
    "  u=texture2DRect(textureYUV,gl_TexCoord[0].st).g;",
    "  v=texture2DRect(textureYUV,gl_TexCoord[0].st).b;",
    "  gl_FragColor=vec4(y,u,v,d);",
    "}"};

const char* FullInterpolator::warpingVertShaderCode[] = {
    "uniform float fx,fy,Z_near,Z_far;",
  "attribute float depth;",
    "varying float d;",
  "void main(void) {",
    "d = 1.0-depth/255.0;",
  "  float temp1 = 1.0/Z_near - 1.0/Z_far;",
  "  float temp2 = 1.0/Z_far;",
  "  float Z = 1.0 / (depth/255.0*temp1 + temp2 );",
  "  float X = (gl_Vertex.x) * Z / fx;",
  "  float Y = (gl_Vertex.y) * Z / fy;",
  "  gl_TexCoord[0].st = gl_Vertex.xy;",
  "  gl_Position = gl_ModelViewProjectionMatrix * vec4(X,Y,-Z,1.0);",
  "}"};

const char* FullInterpolator::inpaintFragShaderCode[] = {
    "uniform sampler2DRect tex;",
    "uniform float width;",
  "void main(void) {",
    "  gl_FragColor = texture2DRect(tex,gl_TexCoord[0].st);",
        "  if (gl_FragColor.a == 1.0) { ",
        "  vec2 c = vec2(0.0);",
        "  vec4 color1 = vec4(1.0), color2 = vec4(1.0);",
        "  while ((color1.a == 1.0) && (gl_TexCoord[0].s-c.s > 0.0)) { c.s++; ",
        "      color1 = texture2DRect(tex,gl_TexCoord[0].st-c); }",
        "  c = vec2(0.0);",
        "  while ((color2.a == 1.0) && (gl_TexCoord[0].s+c.s < width)) { c.s++; ",
        "      color2 = texture2DRect(tex,gl_TexCoord[0].st+c); }",
        "  if (((color2.a < color1.a) && (color2.a != 1.0)) || (color1.a == 1.0))",
        "    gl_FragColor = color2;",
        "  if (((color2.a >= color1.a) && (color1.a != 1.0)) || (color2.a == 1.0))",
        "    gl_FragColor = color1;",
        "  }",
  "}"};

const char* FullInterpolator::noBlendingFragShaderCode[] = {
    "uniform sampler2DRect tex1;",
    "uniform sampler2DRect tex2;",
  "void main(void) {",
    "  vec4 color1, color2;",
    "  vec4 d1,d2;",
    "  color2 = texture2DRect(tex2, gl_TexCoord[0].st);",
    "  color1 = texture2DRect(tex1, gl_TexCoord[0].st);",
    "  if (color1.a == 1.0)",
    "    gl_FragColor = color2;",
    "  else if (color2.a == 1.0)",
    "    gl_FragColor = color1;",
    "  else",
    "    if (color2.a < color1.a)"
    "      gl_FragColor = color2;",
    "    else",
    "      gl_FragColor = color1;",
  "}"};

const char* FullInterpolator::blendingFragShaderCode[] = {
      "uniform sampler2DRect tex1;\n",
      "uniform sampler2DRect tex2;\n",
    "void main(void) {\n",
      "  vec4 color1, color2;\n",
      "  vec4 d1,d2;\n",
      "  color2 = texture2DRect(tex2, gl_TexCoord[0].st);\n",
      "  color1 = texture2DRect(tex1, gl_TexCoord[0].st);\n",
      "  if (color1.a == 1.0)\n",
      "    gl_FragColor = color2;\n",
      "  else if (color2.a == 1.0)\n",
      "    gl_FragColor = color1;\n",
      "  else\n",
      "    gl_FragColor = (color1+color2)/2.0;\n",
    "}\n"};

const char* FullInterpolator::passthroughVertShaderCode[] = {
    "void main(void) {\n",
    "  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n",
    "  gl_TexCoord[0].st = vec2(gl_MultiTexCoord0);\n",
    "}\n"};

FullInterpolator::FullInterpolator()
{
    p_perspectiveCorrectedInterpolationFactor = 0;
}

bool FullInterpolator::activate()
{
    // Framebuffer to capture output textures
    glGenFramebuffers(1, &p_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
    glGenRenderbuffers(1, &gl_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, gl_renderbuffer);

    /////////////////// blending Shaders //////////////////
    // create fragment shader

    GLuint blendingFragShader = glCreateShader(GL_FRAGMENT_SHADER);

    if (p_currentBlendingMode == weighted_blending) {
        glShaderSource(blendingFragShader, 14, blendingFragShaderCode, 0);
        glCompileShader(blendingFragShader);
    } else {
        glShaderSource(blendingFragShader, 16, noBlendingFragShaderCode, 0);
        glCompileShader(blendingFragShader);
    }

    GLuint passthroughVertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(passthroughVertShader, 4, passthroughVertShaderCode, 0);
    glCompileShader(passthroughVertShader);

    p_blendingShaderProgram = glCreateProgram();

    glAttachShader(p_blendingShaderProgram, blendingFragShader);
    glAttachShader(p_blendingShaderProgram, passthroughVertShader);

    glLinkProgram(p_blendingShaderProgram);
    glUseProgram(p_blendingShaderProgram);

    GLint textureSampler = glGetUniformLocation(p_blendingShaderProgram, "tex1");
    glUniform1i(textureSampler, 0);
    textureSampler = glGetUniformLocation(p_blendingShaderProgram, "tex2");
    glUniform1i(textureSampler, 1);

    /////////////////// WARP Shaders //////////////////
    // create fragment shader
    GLuint pointCloudFragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(pointCloudFragShader, 9, warpingFragShaderCode, 0);
    glCompileShader(pointCloudFragShader);

    GLuint pointCloudVertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(pointCloudVertShader, 13, warpingVertShaderCode, 0);
    glCompileShader(pointCloudVertShader);

    p_pointCloudShaderProgram = glCreateProgram();

    glAttachShader(p_pointCloudShaderProgram, pointCloudFragShader);
    glAttachShader(p_pointCloudShaderProgram, pointCloudVertShader);

    glLinkProgram(p_pointCloudShaderProgram);
    glUseProgram(p_pointCloudShaderProgram);

    textureSampler = glGetUniformLocation(p_pointCloudShaderProgram, "textureYUV");
    glUniform1i(textureSampler, 1);

    /////////////////// inpainting Shaders //////////////////
    // create fragment shader
    GLuint inpaintingFragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(inpaintingFragShader, 18, inpaintFragShaderCode, 0);
    glCompileShader(inpaintingFragShader);

    p_inpaintingShaderProgram = glCreateProgram();

    glAttachShader(p_inpaintingShaderProgram, inpaintingFragShader);
    glAttachShader(p_inpaintingShaderProgram, passthroughVertShader);

    glLinkProgram(p_inpaintingShaderProgram);

    // create frame-/renderbuffers
    glEnableClientState(GL_VERTEX_ARRAY);
    glGenBuffers(2, p_gl_buffers);

    glGenTextures(6, p_gl_textures);

#ifndef QT_NO_DEBUG
     int i=0;
     GLchar log[100];
     glGetShaderInfoLog(pointCloudFragShader, 100, &i, log);
     glGetShaderInfoLog(pointCloudVertShader, 100, &i, log);
     glGetShaderInfoLog(blendingFragShader, 100, &i, log);
     glGetShaderInfoLog(passthroughVertShader, 100, &i, log);
     glGetShaderInfoLog(inpaintingFragShader, 100, &i, log);
     glGetProgramInfoLog(p_pointCloudShaderProgram, 100, &i, log);
     glGetProgramInfoLog(p_blendingShaderProgram, 100, &i, log);
     glGetProgramInfoLog(p_inpaintingShaderProgram, 100, &i, log);
#endif
    return true;
}


bool FullInterpolator::deactivate()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteFramebuffers(1, &p_fbo);
    glDeleteRenderbuffers(1, &gl_renderbuffer);

    GLsizei count=0;
    GLuint shaders[10];

    glGetAttachedShaders(p_blendingShaderProgram,  10, &count, shaders);
    for (int i=0; i<= count; i++)
        glDeleteShader(shaders[i]);
    glDeleteProgram(p_blendingShaderProgram);

    glGetAttachedShaders(p_pointCloudShaderProgram,  10, &count, shaders);
    for (int i=0; i<= count; i++)
        glDeleteShader(shaders[i]);
    glDeleteProgram(p_pointCloudShaderProgram);

    glGetAttachedShaders(p_inpaintingShaderProgram,  10, &count, shaders);
    for (int i=0; i<= count; i++)
        glDeleteShader(shaders[i]);
    glDeleteProgram(p_inpaintingShaderProgram);

    glDeleteBuffers(2, p_gl_buffers);
    glDeleteTextures(6, p_gl_textures);

    return true;
}

void FullInterpolator::initializeBuffers() {
    const int width = p_currentRenderObjectLeft->width();
    const int height = p_currentRenderObjectLeft->height();

    // create Vertex Buffer
    int x,y;
    float *xyz = (float*)malloc(width*height*2*sizeof(float));
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            xyz[2*(width*y+x)] = x+0.5f;
            xyz[2*(width*y+x)+1] = y+0.5f;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, p_gl_buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, width*height*2*sizeof(float), xyz, GL_STATIC_DRAW);
    glVertexPointer(2, GL_FLOAT, 0, 0);

    int i = glGetAttribLocation(p_pointCloudShaderProgram, "depth");
    glEnableVertexAttribArray(i);
    glBindBuffer(GL_ARRAY_BUFFER, p_gl_buffers[1]);
    glVertexAttribPointer(i, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, 0);

    for (int i=0; i<6; i++) {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_gl_textures[i]);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    }

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
}

void FullInterpolator::setOutputTextureUnits(int LeftUnit, int RightUnit)
{
    ViewInterpolator::setOutputTextureUnits(LeftUnit, RightUnit);

    prepareFramebufferTextures();
}

bool FullInterpolator::prepareFramebufferTextures() {
    if (p_currentRenderObjectLeft == 0)
        return false;

    float rWidth = p_currentRenderObjectLeft->getCameraParameter().width;
    float rHeight = p_currentRenderObjectLeft->getCameraParameter().height;

    glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, gl_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, rWidth, rHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl_renderbuffer);

    if (p_leftOutputTextureUnit > 0) {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_leftOutputTextureUnit);
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, rWidth, rHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    } else
        return false; // we failed if we couldn't at least prepare the left texture

    if (p_rightOutputTextureUnit > 0) {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, p_rightOutputTextureUnit);
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA8, rWidth, rHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    }
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return true;
}

bool FullInterpolator::loadFrame(int currentFrameIdx, rightOrLeft_t rightOrLeft, int depthBufferUnit)
{
    // first, load texture frame for active view
    if( !ViewInterpolator::loadFrame(currentFrameIdx, rightOrLeft) )
        return false;

    // additionally, load depth frames
    if (rightOrLeft == leftView)
    {
        if (p_currentDepthObjectLeft)
        {
            p_currentDepthObjectLeft->loadDepthmapToTexture(currentFrameIdx, depthBufferUnit);
            return true;
        }
    }
    else
    {
        if (p_currentDepthObjectRight)
        {
            p_currentDepthObjectRight->loadDepthmapToTexture(currentFrameIdx, depthBufferUnit);
            return true;
        }
    }
    return false;
}

bool FullInterpolator::interpolate(int currentFrameIdx, bool renderStereo) {
    if (p_currentRenderObjectLeft == 0)
        return false;

    if (!loadFrame(currentFrameIdx, leftView, p_gl_buffers[1]))
        return false; // if we cannot load at least the left frame, we fail...

    // warp left view
    DoViewWarping(p_currentRenderObjectLeft, p_currentDepthObjectLeft, p_gl_textures[0], renderStereo?p_gl_textures[1]:-1);

    // warp right view
    if (loadFrame(currentFrameIdx, rightView, p_gl_buffers[1]))
        DoViewWarping(p_currentRenderObjectRight, p_currentDepthObjectRight, p_gl_textures[2], renderStereo?p_gl_textures[3]:-1);

    // blending of left eye's view
    DoViewBlending(p_currentRenderObjectLeft, p_gl_textures[0], p_gl_textures[2], p_gl_textures[4]);
    // inpainting of left eye's view
    DoViewInpainting(p_currentRenderObjectLeft, p_gl_textures[4], p_leftOutputTextureUnit);

    if(renderStereo)
    {
        // blending of right eye's view
        DoViewBlending(p_currentRenderObjectLeft, p_gl_textures[1], p_gl_textures[3], p_gl_textures[5]);
        // inpainting of right eye's view
        DoViewInpainting(p_currentRenderObjectLeft, p_gl_textures[5], p_rightOutputTextureUnit);
    }

    return true;
}

bool FullInterpolator::DoViewWarping(YUVObject* sourceObject, YUVObject* depthObject, int outputTexture1, int outputTexture2) {
        if ((sourceObject == NULL) || (depthObject == NULL))
            return false;

        const CameraParameter camera = sourceObject->getCameraParameter();
        glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_EXT, outputTexture1, 0);

        glUseProgram(p_pointCloudShaderProgram);

        glViewport(0, 0, camera.width, camera.height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glFrustum(0.0,camera.Z_near/camera.fx*camera.width, camera.Z_near/camera.fx*camera.height, 0.0,camera.Z_near, camera.Z_far);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glLoadIdentity();
        glTranslatef(-p_interpolationFactor, 0, 0);
        glTranslatef(camera.position[0],camera.position[1],camera.position[2]);

        glDrawArrays(GL_POINTS, 0, camera.width*camera.height);

        if( outputTexture2 != -1 )
        {
            glLoadIdentity();
            glTranslatef(-p_interpolationFactor-p_eyeDistance, 0, 0);
            glTranslatef(camera.position[0],camera.position[1],camera.position[2]);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_EXT, outputTexture2, 0);

            glClear(GL_COLOR_BUFFER_BIT);

            glDrawArrays(GL_POINTS, 0, camera.width*camera.height);
        }

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);

        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        return true;
}

bool FullInterpolator::DoViewBlending(YUVObject* sourceObject, int inputTexture1, int inputTexture2, int outputTexture) {
    const CameraParameter camera = sourceObject->getCameraParameter();

    glViewport(0, 0, camera.width, camera.height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(-1,1, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_EXT, outputTexture, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_blendingShaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, inputTexture1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, inputTexture2);

    glBegin(GL_QUADS);
        glMultiTexCoord2i(GL_TEXTURE0, 0, camera.height);
        glVertex3f(-1,-1,0);
        glMultiTexCoord2i(GL_TEXTURE0, camera.width, camera.height);
        glVertex3f(1,-1,0);
        glMultiTexCoord2i(GL_TEXTURE0, camera.width, 0);
        glVertex3f(1,1,0);
        glMultiTexCoord2i(GL_TEXTURE0, 0, 0);
        glVertex3f(-1,1,0);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool FullInterpolator::DoViewInpainting(YUVObject* sourceObject, int inputTexture, int outputTexture) {
    const CameraParameter camera = sourceObject->getCameraParameter();
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glUseProgram(p_inpaintingShaderProgram);

    glViewport(0, 0, camera.width, camera.height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glFrustum(-.5, .5, -.5, .5, 1, 10);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBindFramebuffer(GL_FRAMEBUFFER, p_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE_EXT, outputTexture, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(p_inpaintingShaderProgram);


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, inputTexture);

    glTranslatef(0,0,-2);
    glBegin(GL_QUADS);
        glTexCoord2i(0,0);
        glVertex3f(-1,-1,0);
        glTexCoord2i(camera.width, 0);
        glVertex3f(1,-1,0);
        glTexCoord2i(camera.width, camera.height);
        glVertex3f(1,1,0);
        glTexCoord2i(0, camera.height);
        glVertex3f(-1,1,0);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void FullInterpolator::setCurrentRenderObjects(YUVObject *newObjectLeft, YUVObject *newObjectRight) {
    ViewInterpolator::setCurrentRenderObjects(newObjectLeft, newObjectRight);

    prepareFramebufferTextures();

    if (p_currentRenderObjectLeft == 0)
        return;

    // update buffer sizes
    initializeBuffers();

    // update camera parameters
    glUseProgram(p_pointCloudShaderProgram);
    int loc=0;
    loc = glGetUniformLocation(p_pointCloudShaderProgram, "fx");
    glUniform1f(loc, p_currentRenderObjectLeft->getCameraParameter().fx);
    loc = glGetUniformLocation(p_pointCloudShaderProgram, "fy");
    glUniform1f(loc, p_currentRenderObjectLeft->getCameraParameter().fy);
    loc = glGetUniformLocation(p_pointCloudShaderProgram, "Z_near");
    glUniform1f(loc, p_currentRenderObjectLeft->getCameraParameter().Z_near);
    loc = glGetUniformLocation(p_pointCloudShaderProgram, "Z_far");
    glUniform1f(loc, p_currentRenderObjectLeft->getCameraParameter().Z_far);

    loc = glGetUniformLocation(p_pointCloudShaderProgram, "textureYUV");
    glUniform1i(loc, 1);   
}

void FullInterpolator::setCurrentDepthObjects(YUVObject *newObjectLeft, YUVObject *newObjectRight)
{
    ViewInterpolator::setCurrentDepthObjects(newObjectLeft, newObjectRight);

    prepareFramebufferTextures();
}
