
#include "openGLViewWidget.h"

#include <QFile>

#include <QVector2D>
#include <QVector3D>

#include <cmath>
// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>


OpenGLViewWidget::OpenGLViewWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_frameWidth(1),
      m_frameHeight(1),
      m_vertice_indices_Vbo(QOpenGLBuffer::IndexBuffer),
      m_texture_Ydata(0),
      m_texture_Udata(0),
      m_texture_Vdata(0),
      m_program(0)
{
  Q_INIT_RESOURCE(shaders);
//    QSizePolicy p(sizePolicy());
////    p.setHorizontalPolicy(QSizePolicy::Fixed);
////    p.setVerticalPolicy(QSizePolicy::Fixed);
//    p.setHeightForWidth(true);
//    setSizePolicy(p);


//  uncommenting this enables update on the vertical refresh of the monitor. however it somehow interferes with the file dialog
//    connect(this,SIGNAL(frameSwapped()),this,SLOT(update()));

}

OpenGLViewWidget::~OpenGLViewWidget()
{
    // Make sure the context is current when deleting the texture
    // and the buffers.
    makeCurrent();
    delete texture;
    doneCurrent();
}



void OpenGLViewWidget::handleOepnGLLoggerMessages( QOpenGLDebugMessage message )
{
    qDebug() << message;
}

void OpenGLViewWidget::resizeGL(int w, int h)
{
    // Calculate aspect ratio
    qreal aspect = qreal(w) / qreal(h ? h : 1);

    // Set near plane to 3.0, far plane to 7.0, field of view 45 degrees
    const qreal zNear = 3.0, zFar = 7.0, fov = 45.0;

    // Reset projection
    projection.setToIdentity();

    // Set perspective projection
    projection.perspective(fov, aspect, zNear, zFar);
}




void OpenGLViewWidget::updateFrame(const QByteArray &textureData)
{
    if(!m_pixelFormat.isValid() || textureData.isEmpty())
    {
        qDebug() << "Unvalid PixelFormat or empty texture Array";
        return;
    }

//    QElapsedTimer timer;
//    timer.start();

    m_swapUV = 0; // todo


    const unsigned char *srcY = (unsigned char*)textureData.data();
    const unsigned char *srcU = srcY + m_componentLength + (m_swapUV * (m_componentLength / m_pixelFormat.getSubsamplingHor()) / m_pixelFormat.getSubsamplingVer());
    const unsigned char *srcV = srcY + m_componentLength + ((1 - m_swapUV) * (m_componentLength / m_pixelFormat.getSubsamplingHor()) / m_pixelFormat.getSubsamplingVer());

    // transmitting the YUV data as three different textures
    // Y on unit 0
    m_texture_Ydata->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, srcY);
    // U on unit 1
    m_texture_Udata->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, srcU);
    // V on unit 2
    m_texture_Vdata->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, srcV);


//    qDebug() << "Moving data to graphics card took" << timer.elapsed() << "milliseconds";

    update();
}

void OpenGLViewWidget::updateFormat(int frameWidth, int frameHeight, YUV_Internals::yuvPixelFormat PxlFormat)
{
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    m_textureFormat = QImage::Format_RGB32;

    m_pixelFormat = PxlFormat;


//    m_swapUV = 0;
//    if(m_pixelFormat == YUVC_422YpCrCb8PlanarPixelFormat || m_pixelFormat == YUVC_444YpCrCb8PlanarPixelFormat)
//        m_swapUV = 1;

    m_componentLength = m_frameWidth*m_frameHeight;

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);



    // just put video on two triangles forming a rectangle
    // compute frame vertices and copy to buffer

    m_videoFrameTriangles_vertices.clear();
    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,-1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,-1,0));


    // triangle definition using indices.
    // we use GL_TRIANGLE_STRIP: Every group of 3 adjacent vertices forms a triangle.
    // Hence need only 4 indices to make our rectangle for the video.
    m_videoFrameTriangles_indices.clear();
    //First Triangle
    m_videoFrameTriangles_indices.push_back(2);
    m_videoFrameTriangles_indices.push_back(0);
    m_videoFrameTriangles_indices.push_back(1);
    // second triangle
    m_videoFrameTriangles_indices.push_back(3);

    m_videoFrameDataPoints_Luma.clear();
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_vertices_Vbo.create();
    m_vertices_Vbo.bind();
    m_vertices_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertices_Vbo.allocate(&m_videoFrameTriangles_vertices[0], m_videoFrameTriangles_vertices.size()* sizeof(glm::vec3));
    // compute indices of vertices and copy to buffer
//    std::cout << "nr of triangles:" << m_videoFrameTriangles_indices.size() / 3 << std::endl;
    m_vertice_indices_Vbo.create();
    m_vertice_indices_Vbo.bind();
    m_vertice_indices_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_vertice_indices_Vbo.allocate(&m_videoFrameTriangles_indices[0], m_videoFrameTriangles_indices.size() * sizeof(unsigned int));
    m_textureLuma_coordinates_Vbo.create();
    m_textureLuma_coordinates_Vbo.bind();
    m_textureLuma_coordinates_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_textureLuma_coordinates_Vbo.allocate(&m_videoFrameDataPoints_Luma[0], m_videoFrameDataPoints_Luma.size() * sizeof(float));

    //recompute opengl matrices

    setupMatricesForCamViewport();

    // setup texture objects

    // transmitting the YUV data as three different textures
    // Y on unit 0
    if(m_texture_Ydata)
    {
        m_texture_Ydata->destroy();
    }
    m_texture_Ydata = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_texture_Ydata->create();
    m_texture_Ydata->setSize(m_frameWidth,m_frameHeight);
    m_texture_Ydata->setFormat(QOpenGLTexture::R8_UNorm);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Ydata->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);
#else
    m_texture_Ydata->allocateStorage();
#endif
    // Set filtering modes for texture minification and  magnification
    m_texture_Ydata->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture_Ydata->setMagnificationFilter(QOpenGLTexture::Linear);
    // Wrap texture coordinates by repeating the border values. GL_CLAMP_TO_EDGE: the texture coordinate is clamped to the [0, 1] range.
    m_texture_Ydata->setWrapMode(QOpenGLTexture::ClampToEdge);




    // U on unit 1
    if(m_texture_Udata)
    {
        m_texture_Udata->destroy();
    }
    m_texture_Udata = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_texture_Udata->create();
    m_texture_Udata->setSize(m_frameWidth/m_pixelFormat.getSubsamplingHor(),m_frameHeight/m_pixelFormat.getSubsamplingVer());
    m_texture_Udata->setFormat(QOpenGLTexture::R8_UNorm);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Udata->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);
#else
    m_texture_Udata->allocateStorage();
#endif
    // Set filtering modes for texture minification and  magnification
    m_texture_Udata->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture_Udata->setMagnificationFilter(QOpenGLTexture::Linear);
    // Wrap texture coordinates by repeating the border values. GL_CLAMP_TO_EDGE: the texture coordinate is clamped to the [0, 1] range.
    m_texture_Udata->setWrapMode(QOpenGLTexture::ClampToEdge);


    // V on unit 2
    if(m_texture_Vdata)
    {
        m_texture_Vdata->destroy();
    }

    m_texture_Vdata = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_texture_Vdata->create();
    m_texture_Vdata->setSize(m_frameWidth/m_pixelFormat.getSubsamplingHor(), m_frameHeight/m_pixelFormat.getSubsamplingVer());
    m_texture_Vdata->setFormat(QOpenGLTexture::R8_UNorm);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Vdata->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);
#else
    m_texture_Vdata->allocateStorage();
#endif
    //Set filtering modes for texture minification and  magnification
    m_texture_Vdata->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture_Vdata->setMagnificationFilter(QOpenGLTexture::Linear);
    // Wrap texture coordinates by repeating the border values. GL_CLAMP_TO_EDGE: the texture coordinate is clamped to the [0, 1] range.
    m_texture_Vdata->setWrapMode(QOpenGLTexture::ClampToEdge);

    update();

}



void OpenGLViewWidget::initializeGL()
{
    // In this example the widget's corresponding top-level window can change
    // several times during the widget's lifetime. Whenever this happens, the
    // QOpenOpenGLViewWidget's associated context is destroyed and a new one is created.
    // Therefore we have to be prepared to clean up the resources on the
    // aboutToBeDestroyed() signal, instead of the destructor. The emission of
    // the signal will be followed by an invocation of initializeGL() where we
    // can recreate all resources.
//    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &OpenGLViewWidget::cleanup);

    logger = new QOpenGLDebugLogger(this);

    logger->initialize(); // initializes in the current context, i.e. ctx

    connect(logger, &QOpenGLDebugLogger::messageLogged, this, &OpenGLViewWidget::handleOepnGLLoggerMessages);
    logger->startLogging();


    initializeOpenGLFunctions();
    const bool is_transparent = false;
    glClearColor(0, 0, 0, is_transparent ? 0 : 1);
    // Dark blue background
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    setupMatricesForCamViewport();

    // Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
    // implementations this is optional and support may not be present
    // at all. Nonetheless the below code works in all cases and makes
    // sure there is a VAO when one is needed.
    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    m_program = new QOpenGLShaderProgram;
    m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":vertexshader.glsl");
    m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":fragmentYUV2RGBshader.glsl");

    m_program->link();
    m_program->bind();

    m_matMVP_Loc = m_program->uniformLocation("MVP");

    m_program->setUniformValue("textureSamplerRed", 0);
    m_program->setUniformValue("textureSamplerGreen", 1);
    m_program->setUniformValue("textureSamplerBlue", 2);

    m_vertices_Loc = m_program->attributeLocation("vertexPosition_modelspace");
    m_textureLuma_Loc = m_program->attributeLocation("vertexLuma");
    m_textureChroma_Loc = m_program->attributeLocation("vertexChroma");
    /*m_vertices_Loc = m_program->attributeLocation("vertexPosition_modelspace");
    m_texture_Loc = m_program->attributeLocation("vertexUV");*/

    // Create vertex buffer objects
    m_vertices_Vbo.create();
    m_vertice_indices_Vbo.create();
    // m_texture_coordinates_Vbo.create();
    m_textureLuma_coordinates_Vbo.create();
    m_textureChroma_coordinates_Vbo.create();

    // Store the vertex attribute bindings for the program.
    setupVertexAttribs();

    m_program->release();    
}

void OpenGLViewWidget::setupVertexAttribs()
{

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // 1rst attribute buffer : vertices
    m_vertices_Vbo.bind();
    f->glEnableVertexAttribArray(m_vertices_Loc);
    f->glVertexAttribPointer(
        m_vertices_Loc,     // attribute.
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
    );

    // 2nd attribute buffer : UVs
    m_textureLuma_coordinates_Vbo.bind();
    f->glEnableVertexAttribArray(m_textureLuma_Loc);
    f->glVertexAttribPointer(
        m_textureLuma_Loc,                    // attribute.
        2,                                // size : U+V => 2
        GL_FLOAT,                         // type
        GL_FALSE,                         // normalized?
        0,                                // stride
        (void*)0                          // array buffer offset
    );

//    // 3nd attribute buffer : UVs Chroma
//    m_textureChroma_coordinates_Vbo.bind();
//    f->glEnableVertexAttribArray(m_textureChroma_Loc);
//    f->glVertexAttribPointer(
//        m_textureChroma_Loc,              // attribute.
//        2,                                // size : U+V => 2
//        GL_FLOAT,                         // type
//        GL_FALSE,                         // normalized?
//        0,                                // stride
//        (void*)0                          // array buffer offset
//    );

}

void OpenGLViewWidget::setupMatricesForCamViewport()
{
    // the video is mapped to a rectangle in 3d space with corners
    // (-1,-1,0),    (1,1,0),     (-1,1,0),    (1,-1,0)
    // here we set up the view/camera of openGl such that it is looking
    // exaclty at this rectangle -> the renderend openGl viewport will be
    // only our video

    glm::mat3 K_view = glm::mat3(1.f);
    float clipFar = 1000;
    float clipNear = 1;
    // Our ModelViewProjection : projection of model to different view and then to image

    K_view[0] = -1.f * K_view[0];
    K_view[1] = -1.f * K_view[1];
    K_view[2] = -1.f * K_view[2];
    clipFar = -1.0 * clipFar;
    clipNear = -1.0 * clipNear;

    glm::mat4 perspective(K_view);
    perspective[2][2] = clipNear + clipFar;
    perspective[3][2] = clipNear * clipFar;
    perspective[2][3] = -1.f;
    perspective[3][3] = 0.f;

    glm::mat4 toNDC = glm::ortho(0.f,(float) m_frameWidth,(float) m_frameHeight,0.f,clipNear,clipFar);

    m_MVP = toNDC * perspective;
}

void OpenGLViewWidget::paintGL()
{
//    qDebug() << "Function Name: " << Q_FUNC_INFO;
//    QElapsedTimer timer;
//    timer.start();

    // nothing loaded yet
    if(m_texture_Ydata == NULL || m_texture_Udata == NULL || m_texture_Vdata == NULL) return;
//    if(m_texture_red_data == NULL ) return;

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);

    m_texture_Ydata->bind(0);
    m_texture_Udata->bind(1);
    m_texture_Vdata->bind(2);

//    m_program->setUniformValue("textureSamplerRed", 0);
//    m_program->setUniformValue("textureSamplerGreen", 1);
//    m_program->setUniformValue("textureSamplerBlue", 2);



    glUniformMatrix4fv(m_matMVP_Loc, 1, GL_FALSE, &m_MVP[0][0]);


    m_vertice_indices_Vbo.bind();
    m_vertices_Vbo.bind();
    m_textureLuma_coordinates_Vbo.bind();
    //m_textureChroma_coordinates_Vbo.bind();

    glDrawElements(
        GL_TRIANGLE_STRIP,      // mode
        m_videoFrameTriangles_indices.size(),    // count
        GL_UNSIGNED_INT,   // type
        (void*)0           // element array buffer offset
    );

    m_program->release();

//    qDebug() << "Painting took" << timer.elapsed() << "milliseconds";
//    int msSinceLastPaint = m_measureFPSTimer.restart();
//    emit msSinceLastPaintChanged(msSinceLastPaint);
}
