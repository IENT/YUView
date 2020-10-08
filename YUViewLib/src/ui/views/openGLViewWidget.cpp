
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
      // during rendering, for each index in the IndexBuffer, a vertex from
      // the vertex buffer (with that index) is selected
      m_vertice_indices_Vbo(QOpenGLBuffer::IndexBuffer),
      m_texture_Ydata(0),
      m_texture_Udata(0),
      m_texture_Vdata(0),
      m_program(0)
{
  // needed in order to ensure the shadres are available as Qresources
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


    doneCurrent();
}



void OpenGLViewWidget::handleOepnGLLoggerMessages( QOpenGLDebugMessage message )
{
    qDebug() << message;
}

void OpenGLViewWidget::resizeGL(int w, int h)
{
    // Calculate aspect ratio
    qreal aspect_widget = qreal(w) / qreal(h ? h : 1);
    qreal aspect_frame = qreal(m_frameWidth) / qreal(m_frameHeight ? m_frameHeight : 1);

    // Reset projection
    projection.setToIdentity();

    // compare apsect ratio of widget and frame and decide how to scale
    if( aspect_widget > aspect_frame )
    {
        // need to scale width
        qreal scale = aspect_frame / aspect_widget;
        projection.scale(scale, 1.0f);
    }
    else
    {
        // need to scale width
        qreal scale = aspect_widget / aspect_frame;
        projection.scale(1.0f, scale);
    }

    // send updated matrix to the shader
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();
    // Set modelview-projection matrix
    m_program->setUniformValue("mvp_matrix", projection);
    m_program->release();
}




void OpenGLViewWidget::updateFrame(const QByteArray &textureData)
{
    if(!m_pixelFormat.isValid() || textureData.isEmpty())
    {
        qDebug() << "Unvalid PixelFormat or empty texture Array";
        return;
    }


    m_swapUV = 0; // todo, this could also be easily done in the fragment shader


    const unsigned char *srcY = (unsigned char*)textureData.data();
    const unsigned char *srcU = srcY + m_componentLength + (m_swapUV * (m_componentLength / m_pixelFormat.getSubsamplingHor()) / m_pixelFormat.getSubsamplingVer());
    const unsigned char *srcV = srcY + m_componentLength + ((1 - m_swapUV) * (m_componentLength / m_pixelFormat.getSubsamplingHor()) / m_pixelFormat.getSubsamplingVer());

    // transmitting the YUV data as three different textures
    m_texture_Ydata->setData(QOpenGLTexture::Red_Integer, m_openGLTexturePixelType, srcY); // Y on unit 0
    m_texture_Udata->setData(QOpenGLTexture::Red_Integer, m_openGLTexturePixelType, srcU); // U on unit 1
    m_texture_Vdata->setData(QOpenGLTexture::Red_Integer, m_openGLTexturePixelType, srcV); // V on unit 2

    update();
}

void OpenGLViewWidget::updateFormat(int frameWidth, int frameHeight, YUV_Internals::yuvPixelFormat PxlFormat)
{
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;

    m_pixelFormat = PxlFormat;


//    m_swapUV = 0;
//    if(m_pixelFormat == YUVC_422YpCrCb8PlanarPixelFormat || m_pixelFormat == YUVC_444YpCrCb8PlanarPixelFormat)
//        m_swapUV = 1;

    const auto bytesPerSample = (m_pixelFormat.bitsPerSample + 7) / 8; // Round to bytes

    m_componentLength = m_frameWidth*m_frameHeight*bytesPerSample;

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);


    // setup texture objects, the following will only allocate
    // the objects/storage. upload happens in updateFrame
    if(m_pixelFormat.bitsPerSample == 8)
    {
        m_openGLTexturePixelType = QOpenGLTexture::UInt8;
        m_openGLTextureFormat = QOpenGLTexture::R8U;
    }
    else if (m_pixelFormat.bitsPerSample == 10)
    {
        m_openGLTexturePixelType = QOpenGLTexture::UInt16;
        m_openGLTextureFormat = QOpenGLTexture::R16U;
    }

    m_program->bind();
    m_program->setUniformValue("sampleMaxVal", float(pow(2,m_pixelFormat.bitsPerSample)));
    m_program->release();

    // transmitting the YUV data as three different textures
    // Y on unit 0
    if(m_texture_Ydata)
    {
        m_texture_Ydata->destroy();
    }
    m_texture_Ydata = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
    m_texture_Ydata->create();
    m_texture_Ydata->setSize(m_frameWidth,m_frameHeight);
    m_texture_Ydata->setFormat(m_openGLTextureFormat);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Ydata->allocateStorage(QOpenGLTexture::Red_Integer,m_openGLTexturePixelType);
#else
    m_texture_Ydata->allocateStorage();
#endif
    // Set filtering modes for texture minification and  magnification
    m_texture_Ydata->setMinificationFilter(QOpenGLTexture::Nearest);
    // need to use Nearest, see https://stackoverflow.com/questions/35705682/qt-5-with-qopengltexture-and-16-bits-integers-images
    m_texture_Ydata->setMagnificationFilter(QOpenGLTexture::Nearest);
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
    m_texture_Udata->setFormat(m_openGLTextureFormat);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Udata->allocateStorage(QOpenGLTexture::Red_Integer,m_openGLTexturePixelType);
#else
    m_texture_Udata->allocateStorage();
#endif
    // Set filtering modes for texture minification and  magnification
    m_texture_Udata->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture_Udata->setMagnificationFilter(QOpenGLTexture::Nearest);
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
    m_texture_Vdata->setFormat(m_openGLTextureFormat);
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
    m_texture_Vdata->allocateStorage(QOpenGLTexture::Red_Integer,m_openGLTexturePixelType);
#else
    m_texture_Vdata->allocateStorage();
#endif
    //Set filtering modes for texture minification and  magnification
    m_texture_Vdata->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture_Vdata->setMagnificationFilter(QOpenGLTexture::Nearest);
    // Wrap texture coordinates by repeating the border values. GL_CLAMP_TO_EDGE: the texture coordinate is clamped to the [0, 1] range.
    m_texture_Vdata->setWrapMode(QOpenGLTexture::ClampToEdge);

    // need to resize, aspect ratio could have changed
    resizeGL(geometry().width(), geometry().height());
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
    // Gray level background
    glClearColor(0.5f, 0.5f, 0.5f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);


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


    m_program->setUniformValue("textureSamplerRed", 0);
    m_program->setUniformValue("textureSamplerGreen", 1);
    m_program->setUniformValue("textureSamplerBlue", 2);


    // just put video on two triangles forming a rectangle as follows.
    // The trianlges are formed  by the vertice v0 to v3
    // v2------v1
    // |       /|
    // |      / |
    // |     /  |
    // |    /   |
    // |   /    |
    // |  /     |
    // | /      |
    // |/       |
    // v0------v3
    // the vertices are setup to be aligned with openGL clip coordinates.
    // Thus no further coordinate transform will be necessary.
    // Further, textrue coordinates can be obtained by dropping the z-axis coordinate.
    // https://learnopengl.com/Getting-started/Coordinate-Systems

    // creating an array witht the vertices v0 to v3
    m_videoFrameTriangles_vertices.clear();
    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,-1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,1,0));
    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,-1,0));
    // 1rst attribute buffer : vertices
    m_vertices_Vbo.create();
    m_vertices_Vbo.bind();
    int vertices_Loc = m_program->attributeLocation("vertexPosition_modelspace");
    glEnableVertexAttribArray(vertices_Loc);
    glVertexAttribPointer(
        vertices_Loc,     // attribute.
        3,                  // size
        GL_FLOAT,           // type
        GL_FALSE,           // normalized?
        0,                  // stride
        (void*)0            // array buffer offset
    );
    // transmit vertices to vertice buffer object
    m_vertices_Vbo.create();
    m_vertices_Vbo.bind();
    m_vertices_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw); //The data will be set once and used many times for drawing operations.
    m_vertices_Vbo.allocate(&m_videoFrameTriangles_vertices[0], m_videoFrameTriangles_vertices.size()* sizeof(glm::vec3));


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
    // transmit inideces to  buffer object
    m_vertice_indices_Vbo.create();
    m_vertice_indices_Vbo.bind();
    m_vertice_indices_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw); //The data will be set once and used many times for drawing operations.
    m_vertice_indices_Vbo.allocate(&m_videoFrameTriangles_indices[0], m_videoFrameTriangles_indices.size() * sizeof(unsigned int));


    // setting up texture coordinates for each vertex.
    // This links the rectangle corners to the corners of the uploaeded
    // texture / frame.
    m_videoFrameDataPoints_Luma.clear();
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(1.f);
    m_videoFrameDataPoints_Luma.push_back(0.f);
    // 2nd attribute buffer : texture coordinates / UVs
    m_textureLuma_coordinates_Vbo.create();
    m_textureLuma_coordinates_Vbo.bind();
    int textureLuma_Loc = m_program->attributeLocation("vertexLuma");
    glEnableVertexAttribArray(textureLuma_Loc);
    glVertexAttribPointer(
        textureLuma_Loc,                    // attribute.
        2,                                // size : U+V => 2
        GL_FLOAT,                         // type
        GL_FALSE,                         // normalized?
        0,                                // stride
        (void*)0                          // array buffer offset
    );
    // transmit texture coordinates to  buffer object
    m_textureLuma_coordinates_Vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_textureLuma_coordinates_Vbo.allocate(&m_videoFrameDataPoints_Luma[0], m_videoFrameDataPoints_Luma.size() * sizeof(float));

    m_program->release();    
}


void OpenGLViewWidget::paintGL()
{
    // nothing loaded yet, do nothing
    if(m_texture_Ydata == NULL || m_texture_Udata == NULL || m_texture_Vdata == NULL) return;

    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
    m_program->bind();

    // clear remove last frame. might be unnecessary, since we overwrite the whole viewport with the new frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // make the new uploaded frame available
    m_texture_Ydata->bind(0);
    m_texture_Udata->bind(1);
    m_texture_Vdata->bind(2);

    // make vertices, triangles and texture coordinates available
    m_vertice_indices_Vbo.bind();
    m_vertices_Vbo.bind();
    m_textureLuma_coordinates_Vbo.bind();

    // draw the new frame
    // GL_TRIANGLE_STRIP: Every group of 3 adjacent vertices forms a triangle.
    // The face direction of the strip is determined by the winding of the first triangle.
    // Each successive triangle will have its effective face order reversed,
    // so the system compensates for that by testing it in the opposite way.
    // A vertex stream of n length will generate n-2 triangles.
    glDrawElements(
        GL_TRIANGLE_STRIP,      // mode
        m_videoFrameTriangles_indices.size(),    // count
        GL_UNSIGNED_INT,   // data type (use type of m_videoFrameTriangles_indices)
        (void*)0           // element array buffer offset
    );

    m_program->release();

}
