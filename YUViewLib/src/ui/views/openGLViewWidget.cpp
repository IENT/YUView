
#include "openGLViewWidget.h"

#include <QMouseEvent>


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
//    m_core = QCoreApplication::arguments().contains(QStringLiteral("--coreprofile"));
//    // --transparent causes the clear color to be transparent. Therefore, on systems that
//    // support it, the widget will become transparent apart from the logo.
//    m_transparent = QCoreApplication::arguments().contains(QStringLiteral("--transparent"));
//    if (m_transparent)
//        setAttribute(Qt::WA_TranslucentBackground);

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

//! [0]
void OpenGLViewWidget::mousePressEvent(QMouseEvent *e)
{
    // Save mouse press position
    mousePressPosition = QVector2D(e->localPos());
}

void OpenGLViewWidget::mouseReleaseEvent(QMouseEvent *e)
{
    // Mouse release position - mouse press position
    QVector2D diff = QVector2D(e->localPos()) - mousePressPosition;

    // Rotation axis is perpendicular to the mouse position difference
    // vector
    QVector3D n = QVector3D(diff.y(), diff.x(), 0.0).normalized();

    // Accelerate angular speed relative to the length of the mouse sweep
    qreal acc = diff.length() / 100.0;

    // Calculate new rotation axis as weighted sum
    rotationAxis = (rotationAxis * angularSpeed + n * acc).normalized();

    // Increase angular speed
    angularSpeed += acc;
}
//! [0]

//! [1]
void OpenGLViewWidget::timerEvent(QTimerEvent *)
{
    // Decrease angular speed (friction)
    angularSpeed *= 0.99;

    // Stop rotation when speed goes below threshold
    if (angularSpeed < 0.01) {
        angularSpeed = 0.0;
    } else {
        // Update rotation
        rotation = QQuaternion::fromAxisAndAngle(rotationAxis, angularSpeed) * rotation;

        // Request an update
        update();
    }
}
//! [1]

void OpenGLViewWidget::handleOepnGLLoggerMessages( QOpenGLDebugMessage message )
{
    qDebug() << message;
}

////! [3]
//void OpenGLViewWidget::initShaders()
//{

////    file:///home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/fshader.glsl
////    file:///home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/vshader.glsl

////    QFile file(":/vshader.glsl");
//    QFile file("/home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/vshader.glsl");

////    bool t = file.exists();
////    QByteArray test =  file.readAll();

//    // Compile vertex shader
////    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/vshader.glsl"))
//    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, "/home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/vshader.glsl"))
//        close();

//    // Compile fragment shader
//    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, "/home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/fshader.glsl"))
////    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fshader.glsl"))
//        close();

//    // Link shader pipeline
//    if (!program.link())
//        close();

//    // Bind shader pipeline for use
//    if (!program.bind())
//        close();
//}
////! [3]

////! [4]
//void OpenGLViewWidget::initTextures()
//{
//    // Load cube.png image
////    texture = new QOpenGLTexture(QImage(":/cube.png").mirrored());
//    texture = new QOpenGLTexture(QImage("/home/sauer/Software/YUViewStuff/YUView/YUViewLib/images/cube.png").mirrored());
////    file:///home/sauer/Software/YUViewStuff/YUView/YUViewLib/images/cube.png

//    // Set nearest filtering mode for texture minification
//    texture->setMinificationFilter(QOpenGLTexture::Nearest);

//    // Set bilinear filtering mode for texture magnification
//    texture->setMagnificationFilter(QOpenGLTexture::Linear);

//    // Wrap texture coordinates by repeating
//    // f.ex. texture coordinate (1.1, 1.2) is same as (0.1, 0.2)
//    texture->setWrapMode(QOpenGLTexture::Repeat);
//}
//! [4]

//! [5]
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
//! [5]

//void OpenGLViewWidget::paintGL()
//{
//    // Clear color and depth buffer
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//    texture->bind();

////! [6]
//    // Calculate model view transformation
//    QMatrix4x4 matrix;
//    matrix.translate(0.0, 0.0, -5.0);
//    matrix.rotate(rotation);

//    // Set modelview-projection matrix
//    program.setUniformValue("mvp_matrix", projection * matrix);
////! [6]

//    // Use texture unit 0 which contains cube.png
//    program.setUniformValue("texture", 0);

//    // Draw cube geometry
//    geometries->drawCubeGeometry(&program);
//}









struct VertexData
{
    QVector3D position;
    QVector2D texCoord;
};


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

    setupMatrices();

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
    // Wrap texture coordinates by repeating
    m_texture_Ydata->setWrapMode(QOpenGLTexture::ClampToBorder);




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
    // Wrap texture coordinates by repeating
    m_texture_Udata->setWrapMode(QOpenGLTexture::ClampToBorder);


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
    // Wrap texture coordinates by repeating
    m_texture_Vdata->setWrapMode(QOpenGLTexture::ClampToBorder);

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

    setupMatrices();

    // Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
    // implementations this is optional and support may not be present
    // at all. Nonetheless the below code works in all cases and makes
    // sure there is a VAO when one is needed.
    m_vao.create();
    QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

    m_program = new QOpenGLShaderProgram;
//    m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,":/vertexshader.glsl");
//    m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,":fragmentYUV2RGBshader.glsl");
    m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,"/home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/vertexshader.glsl");
    m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,"/home/sauer/Software/YUViewStuff/YUView/YUViewLib/shaders/fragmentYUV2RGBshader.glsl");

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

    // for debugging. use first frame of RaceHorsesL_832x480_30fps_8bit_420pf
    // /home/sauer/Videos/SD/RaceHorsesL_832x480_30fps_8bit_420pf.yuv

    updateFormat(832,480, YUV_Internals::yuvPixelFormat(YUV_Internals::Subsampling::YUV_420, 8, YUV_Internals::PlaneOrder::YUV)); // YUV 4:2:0)

    QFile raceHorsesSeq("/home/sauer/Videos/SD/RaceHorsesL_832x480_30fps_8bit_420pf.yuv");

    if (!raceHorsesSeq.open(QIODevice::ReadOnly ))
    {
        qDebug() << "could not open file:" << raceHorsesSeq.fileName();
    }
    QByteArray firstFrame = raceHorsesSeq.read(599040);

    updateFrame(firstFrame);
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

void OpenGLViewWidget::setupMatrices()
{
    glm::mat3 K_view = glm::mat3(1.f);

    // Our ModelViewProjection : projection of model to different view and then to image
    m_MVP = getProjectionFromCamCalibration(K_view,1000,1);

    //    std::cout << "MVP: " << glm::to_string(m_MVP) << std::endl;
}

glm::mat4 OpenGLViewWidget::getProjectionFromCamCalibration(glm::mat3 &calibrationMatrix, float clipFar, float clipNear)
{
    calibrationMatrix[0] = -1.f * calibrationMatrix[0];
    calibrationMatrix[1] = -1.f * calibrationMatrix[1];
    calibrationMatrix[2] = -1.f * calibrationMatrix[2];
    clipFar = -1.0 * clipFar;
    clipNear = -1.0 * clipNear;

    glm::mat4 perspective(calibrationMatrix);
    perspective[2][2] = clipNear + clipFar;
    perspective[3][2] = clipNear * clipFar;
    perspective[2][3] = -1.f;
    perspective[3][3] = 0.f;

    glm::mat4 toNDC = glm::ortho(0.f,(float) m_frameWidth,(float) m_frameHeight,0.f,clipNear,clipFar);

    glm::mat4 Projection2 = toNDC * perspective;


//    std::cout << "perspective: " << glm::to_string(perspective) << std::endl;
//    std::cout << "toNDC: " << glm::to_string(toNDC) << std::endl;
//    std::cout << "Projection2: " << glm::to_string(Projection2) << std::endl;

    return Projection2;
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

// function to generate the vertices corresponding to the pixels of a frame. Each vertex is centered at a pixel, borders are padded.
void OpenGLViewWidget::computeFrameVertices(int frameWidth, int frameHeight)
{

    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,-1,0));

    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,1,0));

    m_videoFrameTriangles_vertices.push_back(glm::vec3(-1,1,0));

    m_videoFrameTriangles_vertices.push_back(glm::vec3(1,-1,0));


    /*for( int h = 0; h < frameHeight; h++)
    {
        for(int w = 0; w < frameWidth; w++)
        {

            m_videoFrameTriangles_vertices.push_back(glm::vec3(w,h,0));
        }
    }*/
}

// function to create a mesh by connecting the vertices to triangles
void OpenGLViewWidget::computeFrameMesh(int frameWidth, int frameHeight)
{
    m_videoFrameTriangles_indices.clear();

    m_videoFrameTriangles_indices.push_back(2);
    m_videoFrameTriangles_indices.push_back(0);
    m_videoFrameTriangles_indices.push_back(1);
    // second triangle
    m_videoFrameTriangles_indices.push_back(0);
    m_videoFrameTriangles_indices.push_back(3);
    m_videoFrameTriangles_indices.push_back(1);

    /*for( int h = 0; h < frameHeight; h++)
    {
        for(int w = 0; w < frameWidth; w++)
        {
            //tblr: top bottom left right
            int index_pixel_tl = h*(frameWidth) + w;
            int index_pixel_tr = index_pixel_tl + 1;
            int index_pixel_bl = (h+1)*(frameWidth) + w;
            int index_pixel_br = index_pixel_bl + 1;

            // each pixel generates two triangles, specify them so orientation is counter clockwise
            // first triangle
            m_videoFrameTriangles_indices.push_back(index_pixel_tl);
            m_videoFrameTriangles_indices.push_back(index_pixel_bl);
            m_videoFrameTriangles_indices.push_back(index_pixel_tr);
            // second triangle
            m_videoFrameTriangles_indices.push_back(index_pixel_bl);
            m_videoFrameTriangles_indices.push_back(index_pixel_br);
            m_videoFrameTriangles_indices.push_back(index_pixel_tr);
        }
    }*/
}

// Function to fill Vector which will hold the texture coordinates which maps the texture (the video data) to the frame's vertices.
void OpenGLViewWidget::computeLumaTextureCoordinates(int chromaWidth, int chromaHeight)
{
    m_videoFrameDataPoints_Luma.clear();

    for( int h = 0; h < chromaHeight; h++)
    {
        for(int w = 0; w < chromaWidth; w++)
        {

            float x,y;
            x = (w + 0.5)/chromaWidth;  //center of pixel
            y = (h + 0.5)/chromaHeight;  //center of pixel

            // set u for the pixel
            m_videoFrameDataPoints_Luma.push_back(x);
            // set v for the pixel
            m_videoFrameDataPoints_Luma.push_back(y);
            // set u for the pixel
        }
    }
}

void OpenGLViewWidget::computeChromaTextureCoordinates(int chromaframeWidth, int chromaframeHeight)
{
    m_videoFrameDataPoints_Chroma.clear();

    for( int h = 0; h < chromaframeHeight; h++)
    {
        for(int i = 0; (i < m_pixelFormat.getSubsamplingVer()); i++)
        {
            for(int w = 0; w < chromaframeWidth; w++)
            {
                float x,y;
                x = (w + 0.5)/chromaframeWidth;  //center of pixel
                y = (h + 0.5)/chromaframeHeight;  //center of pixel

                //Load extra coordinates dependent on subsampling
                for(int j = 0; (j < m_pixelFormat.getSubsamplingHor()); j++)
                {
                    // set x for the pixel
                    m_videoFrameDataPoints_Chroma.push_back(x);
                    // set y for the pixel
                    m_videoFrameDataPoints_Chroma.push_back(y);
                }
            }
        }
    }
}
