#include "renderwidget.h"
#include "cameraparameter.h"

#include "glew.h"
#include <QMessageBox>
#include <QUrl>
#include <QTextDocument>
#include <QMimeData>
#include "mainwindow.h"
#define _USE_MATH_DEFINES
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define ZOOMBOX_MIN_FACTOR 5.0
#define ZOOMBOX_MAX_FACTOR 20.0

static AnaglyphRenderer* p_anaglyphRenderer = NULL;
static LeftRightLineRenderer* p_leftrightlineRenderer = NULL;
static ShutterRenderer* p_shutterRenderer = NULL;

RenderWidget::RenderWidget(QWidget *parent) : QGLWidget(parent)
{
    p_defaultRenderer=NULL;
    p_currentRenderObjectLeft = NULL;
    p_currentDepthObjectLeft = NULL;
    p_currentRenderObjectRight = NULL;
    p_currentDepthObjectRight = NULL;

    p_renderMode = unspecifiedRenderMode;

    p_currentFrameIdx = 0;

    selectionMode_ = NONE;

    p_zoomFactor = 1.0;
    p_boxZoomFactor = ZOOMBOX_MIN_FACTOR;
    p_zoomBoxEnabled = false;
    p_customViewport = false;

    p_viewInterpolationMode = unspecifiedInterpolation;
    p_interpolationFactor = 0.0;
    p_eyeDistance = 0.0;

    p_drawGrid =  false;
    p_gridSize = 64;

    p_currentViewInterpolator = NULL;
    p_currentRenderer = NULL;

    p_gl_initialized = false;
    p_currentStatsParser = 0;
    p_simplifyStats = false;
    p_simplificationTheshold = 0;

    setAcceptDrops(true);
    setAutoFillBackground(false);
    setAutoBufferSwap(false);

    p_noInterpolator = NULL;
    p_fullInterpolator = NULL;
    p_defaultRenderer = NULL;
    p_RenderWidget2 = NULL;
}

RenderWidget::~RenderWidget() {
}

void RenderWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QGLWidget::dragEnterEvent(event);
}

void RenderWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QUrl url;
            QStringList fileList;

            // use our main window to open this file
            MainWindow* mainWindow = (MainWindow*)this->window();

            foreach (url, urls)
            {
                QString fileName = url.toLocalFile();

                QFileInfo fi(fileName);
                QString ext = fi.suffix();
                ext = ext.toLower();

                if( fi.isDir() || ext == "yuv" || ext == "xml" )
                    fileList.append(fileName);
                else if( ext == "csv" )
                    mainWindow->loadStatsFile(fileName);
            }

            event->acceptProposedAction();

            mainWindow->loadFiles(fileList);
        }
    }
    QGLWidget::dropEvent(event);
}

void RenderWidget::setCurrentRenderObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight )
{
    if (!p_gl_initialized)
        return;

    makeCurrent();

    p_currentRenderObjectLeft = newObjectLeft;
    p_currentRenderObjectRight = newObjectRight;

    // if we don't have at least a left render object, return
    if(newObjectLeft == NULL)
        return;

    // update dimensions
    p_videoHeight = p_currentRenderObjectLeft->height();
    p_videoWidth = p_currentRenderObjectLeft->width();

    // update rendering mode, if necessary
    if(p_renderMode == unspecifiedRenderMode)
    {
        setRenderMode(normalRenderMode);
    }
    else if( p_renderMode != normalRenderMode && p_currentRenderObjectRight == NULL )
    {
        // if we don't have a right eye view, render normally
        setRenderMode(normalRenderMode);
    }

    // also update interpolation mode, if necessary
    if (p_viewInterpolationMode == unspecifiedInterpolation)
        setViewInterpolationMode(noInterpolation);

    // update viewinterpolator
    p_currentViewInterpolator->setCurrentRenderObjects(newObjectLeft, newObjectRight);

    // update renderer
    p_currentRenderer->setVideoSize(p_videoWidth, p_videoHeight);

    // allways reset, just in case....
    p_currentRenderer->setInputTextureUnits(p_renderTextures[0], p_renderTextures[1]);

    // make sure that viewport is updated
    if( !p_customViewport)
        resizeGL(width(), height());

    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, p_videoWidth, 0, p_videoHeight, -1., 1.);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
}

void RenderWidget::setCurrentDepthObjects( YUVObject* newObjectLeft, YUVObject* newObjectRight ) {
    if (!p_gl_initialized)
        return;

    p_currentDepthObjectLeft = newObjectLeft;
    p_currentDepthObjectRight = newObjectRight;

    p_currentViewInterpolator->setCurrentDepthObjects(newObjectLeft, newObjectRight);
}

void RenderWidget::setCurrentStatistics(StatisticsParser* stats, QVector<StatisticsRenderItem> &renderTypes) {
    p_currentStatsParser = stats;
    p_renderStatsTypes = renderTypes;
    update();
}

int RenderWidget::getRenderMode() {
    return p_renderMode;
}

void RenderWidget::setRenderMode(int newMode)
{
    int w=0, h=0, offsW=0, offsH=0;

    if (!p_gl_initialized)
        return;

    // don't do anything if rendermode is already up-to-date
    if (newMode == p_renderMode)
        return;

    // first save new stereo mode
    if (newMode < 0)
        p_renderMode = unspecifiedRenderMode;
    else
        p_renderMode = (render_mode_t)newMode;

    if (p_currentRenderer != NULL) {
        p_currentRenderer->getRenderSize(offsW, offsH, w, h); // restore previous zooming parameters
        p_currentRenderer->deactivate();
    }

    switch (p_renderMode) {
    case shutterRenderMode:
        if( p_shutterRenderer == NULL )
            p_shutterRenderer = new ShutterRenderer();
        p_currentRenderer = p_shutterRenderer;
        break;
    case anaglyphRenderMode:
        if( p_anaglyphRenderer == NULL )
            p_anaglyphRenderer = new AnaglyphRenderer();
        p_currentRenderer = p_anaglyphRenderer;
        break;
    case leftrightlineRenderMode:
        if( p_leftrightlineRenderer == NULL )
            p_leftrightlineRenderer = new LeftRightLineRenderer();
        p_leftrightlineRenderer->setRenderWidget(this);
        p_currentRenderer = p_leftrightlineRenderer;
        break;
    case normalRenderMode:
    case unspecifiedRenderMode:
        if( p_defaultRenderer == NULL )
            p_defaultRenderer = new DefaultRenderer();
        p_currentRenderer = p_defaultRenderer;
        break;
    }

    if (!p_currentRenderer->activate())
        QMessageBox::critical(this, tr("YUView"), tr("Could not activate the current Renderer"));
    p_currentRenderer->setInputTextureUnits(p_renderTextures[0], p_renderTextures[1]);
    p_currentRenderer->setVideoSize(p_videoWidth, p_videoHeight);
    p_currentRenderer->setRenderSize(offsW, offsH, w, h);

    // make changes happen
    update();
}


void RenderWidget::initializeGL()
{
    makeCurrent();

    // Setup some basic OpenGL stuff
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, p_maxViewportDims);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClearColor (0.5, 0.5, 0.5, 0.0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);

    glShadeModel (GL_FLAT);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //glEnable(GL_CULL_FACE);
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    glewInit();

    if (!glewIsSupported("GL_VERSION_1_4 GL_ARB_fragment_shader GL_ARB_vertex_shader GL_ARB_framebuffer_object GL_ARB_texture_rectangle"))
        QMessageBox::critical(this, tr("YUView"), tr("Not all needed OpenGL extensions are available on your machine. This could result in unexspected behavior or crashes. Sorry, your graphic card might be to old for all this fancy stuff."));

    // init interpolator output textures
    glGenTextures(2, p_renderTextures);

    glClear(GL_COLOR_BUFFER_BIT);
    setAutoBufferSwap(false);
    p_gl_initialized = true;
}

void RenderWidget::drawFrame(unsigned int frameIdx)
{
    makeCurrent();

    p_currentFrameIdx = frameIdx;

    // now redraw our context
    update();
}

void RenderWidget::clear()
{
    makeCurrent();

    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderWidget::drawFrame()
{
    clear();

    // check if we have at least one object to draw
    if( (p_currentRenderObjectLeft == NULL) || (p_currentRenderer == NULL) || (p_currentViewInterpolator == NULL) )
        return;

    // get current frames from file and optionally interpolate
    GLint leftTextureHandle = p_currentRenderObjectLeft?p_currentRenderObjectLeft->textureHandle():-1;
    GLint rightTextureHandle = p_currentRenderObjectRight?p_currentRenderObjectRight->textureHandle():-1;
    if( p_currentViewInterpolator->interpolate(p_currentFrameIdx, (p_renderMode != normalRenderMode)) )
    {
        leftTextureHandle = p_currentRenderObjectLeft?p_renderTextures[0]:-1;
        rightTextureHandle = p_currentRenderObjectRight?p_renderTextures[1]:-1;
    }

    // render scene to OpenGL context
    makeCurrent();
    p_currentRenderer->setInputTextureUnits(leftTextureHandle, rightTextureHandle);
    p_currentRenderer->render();
}

void RenderWidget::drawStatisticsOverlay()
{
    // draw statistics
    if (p_currentStatsParser) {
        for(int i=p_renderStatsTypes.size()-1; i>=0; i--) {
            if (!p_renderStatsTypes[i].render) continue;
            Statistics stats;
            if (p_simplifyStats) {
                //calculate threshold
                int threshold=0;
                unsigned int v = (p_zoomFactor < 1) ? 1/p_zoomFactor : p_zoomFactor;
                // calc next power of two
                v--;
                v |= v >> 1;
                v |= v >> 2;
                v |= v >> 4;
                v |= v >> 8;
                v |= v >> 16;
                v++;
                threshold = (p_zoomFactor < 1) ? v * p_simplificationTheshold : p_simplificationTheshold / v;

                stats = p_currentStatsParser->getSimplifiedStatistics(p_currentFrameIdx, p_renderStatsTypes[i].type_id, threshold, p_simplifiedGridColor);
            } else
                stats = p_currentStatsParser->getStatistics(p_currentFrameIdx, p_renderStatsTypes[i].type_id);

            drawStatistics(stats, p_renderStatsTypes[i]);
        }
    }

    // draw regular grid
    if (p_drawGrid) {
        glLineWidth(1.0);
        glBegin(GL_LINES);
        glColor4ubv(p_gridColor);
        for (int i=0; i<p_videoWidth; i+=p_gridSize) {
            glVertex2i(i, 0);
            glVertex2i(i, p_videoHeight);
        }
        for (int j=0; j<p_videoHeight; j+=p_gridSize) {
            glVertex2i(0, p_videoHeight - j);
            glVertex2i(p_videoWidth, p_videoHeight - j);
        }
        glEnd();
    }
}

void RenderWidget::paintGL() {

    if (!p_gl_initialized)
        return;

    makeCurrent();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    drawFrame();
    drawStatisticsOverlay();

    // draw rectangular selection area.
    if (selectionMode_ == SELECT)
        drawSelectionRectangle();

    // draw Zoombox
    if (p_zoomBoxEnabled)
        drawZoomBox(p_currentMousePosition);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glFlush();
    swapBuffers();
}

void RenderWidget::drawStatistics(Statistics& stats, StatisticsRenderItem &item) {
    if (!p_gl_initialized)
        return;

    makeCurrent();

    glPushMatrix();

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_RECTANGLE_EXT);
    glLineWidth(1.0);

    Statistics::iterator it;
    for (it = stats.begin(); it != stats.end(); it++) {
        switch (it->type) {
        case arrowType:
            //draw an arrow
            float x,y, nx, ny, a;

            // start vector at center of the box
            x = (float)it->position[0]+(float)it->size[0]/2.0;
            y = (float)it->position[1]+(float)it->size[1]/2.0;

            glColor4ub(it->color[0], it->color[1], it->color[2], it->color[3] * ((float)item.alpha / 100.0));
            glBegin(GL_LINES);
            glVertex3f(x, p_videoHeight - y, 0.6f);
            glVertex3f(x + it->direction[0], p_videoHeight - (y + it->direction[1]), 0.6f);
            glEnd();

            // draw arrow head
            a = 2.5;
            glBegin(GL_TRIANGLES);
            // arrow head
            glVertex3f(x + it->direction[0], p_videoHeight - (y + it->direction[1]), 0.6f);
            // arrow head right
            rotateVector(5.0/6.0*M_PI, it->direction[0], it->direction[1], nx, ny);
            glVertex3f(x + it->direction[0] + nx * a, p_videoHeight- y - it->direction[1] - ny * a, 0.6f);
            // arrow head left
            rotateVector(-5.0/6.0*M_PI, it->direction[0], it->direction[1], nx, ny);
            glVertex3f(x + it->direction[0] + nx * a, p_videoHeight- y - it->direction[1] - ny * a, 0.6f);
            glEnd();

            break;
        case blockType:
            //draw a rectangle
            glColor4ub(it->color[0], it->color[1], it->color[2], it->color[3] * ((float)item.alpha / 100.0));
            //glColor4ubv(it->color);
            glBegin(GL_QUADS);
            glVertex3f(it->position[0], p_videoHeight- it->position[1], 0.6f);
            glVertex3f(it->position[0], p_videoHeight- it->position[1]-it->size[1], 0.6f);
            glVertex3f(it->position[0]+it->size[0], p_videoHeight- it->position[1]-it->size[1], 0.6f);
            glVertex3f(it->position[0]+it->size[0], p_videoHeight- it->position[1], 0.6f);
            glEnd();

            break;
        }
        if (item.renderGrid) {
            //draw a rectangle
            glColor3ubv(it->gridColor);
            glBegin(GL_LINE_LOOP);
            glVertex3f(it->position[0], p_videoHeight- it->position[1], 0.6f);
            glVertex3f(it->position[0], p_videoHeight- it->position[1]-it->size[1], 0.6f);
            glVertex3f(it->position[0]+it->size[0], p_videoHeight- it->position[1]-it->size[1], 0.6f);
            glVertex3f(it->position[0]+it->size[0], p_videoHeight- it->position[1], 0.6f);
            glEnd();
        }
    }
    glPopMatrix();

    //glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);
    glDisable(GL_BLEND);

    return;
}

void RenderWidget::drawSelectionRectangle()
{
    if (!p_gl_initialized)
        return;
    //startScreenCoordinatesSystem();

    makeCurrent();

    GLdouble objbl_x, objbr_x, objtr_x, objtl_x;
    GLdouble objbl_y, objbr_y, objtr_y, objtl_y;
    GLdouble objbl_z, objbr_z, objtr_z, objtl_z;
    GLint viewport[4];
    GLdouble mvmatrix[16], projmatrix[16];

    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLint real_top = height() - (GLint) p_selectionRect.top();
    GLint real_bot = height() - (GLint) p_selectionRect.bottom();

    gluUnProject( p_selectionRect.left(), real_bot, 0, mvmatrix, projmatrix, viewport, &objbl_x, &objbl_y, &objbl_z );
    gluUnProject( p_selectionRect.right(), real_bot, 0, mvmatrix, projmatrix, viewport, &objbr_x, &objbr_y, &objbr_z );
    gluUnProject( p_selectionRect.right(), real_top, 0, mvmatrix, projmatrix, viewport, &objtr_x, &objtr_y, &objtr_z );
    gluUnProject( p_selectionRect.left(), real_top, 0, mvmatrix, projmatrix, viewport, &objtl_x, &objtl_y, &objtl_z );

    glPushMatrix();

    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_RECTANGLE_EXT);

    glColor4f(0.95f, 0.49f, 0.07f, 0.60f);
    glBegin(GL_QUADS);
    glVertex3f(objtl_x,  objtl_y, 0.7f);
    glVertex3f(objtr_x, objtr_y, 0.7f);
    glVertex3f(objbr_x, objbr_y, 0.7f);
    glVertex3f(objbl_x,  objbl_y, 0.7f);
    glEnd();

    glLineWidth(2.0);
    glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(objtl_x,  objtl_y, 0.6f);
    glVertex3f(objtr_x, objtr_y, 0.6f);
    glVertex3f(objbr_x, objbr_y, 0.6f);
    glVertex3f(objbl_x,  objbl_y, 0.6f);
    glEnd();

    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);
    //stopScreenCoordinatesSystem();
}

void RenderWidget::drawZoomBox(QPoint mousePos)
{
    if (!p_gl_initialized)
        return;

    // check if we have at least one object to draw
    if( (p_currentRenderObjectLeft == NULL) || (p_currentRenderer == NULL) || (p_currentViewInterpolator == NULL) )
        return;

    makeCurrent();

    //glDisable( GL_DEPTH_TEST );
    //glDisable( GL_BLEND );
    //glDisable( GL_LINE_SMOOTH );
    //glEnable(GL_MULTISAMPLE);
    GLint* glMagFilterParam = new GLint;
    GLint* glMinFilterParam = new GLint;

    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glMagFilterParam);
    if (*glMagFilterParam==GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glMinFilterParam);
    if (*glMinFilterParam==GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    QPainter painter(this);

    // Get the current draw buffer
    glReadBuffer(GL_BACK);

    QImage img = grabFrameBuffer();

    // zoom in
    const int size = 200;
    const int Margin = 11;
    const int Padding = 6;
    int scaled_size = qRound((float)size / p_boxZoomFactor);
    scaled_size += (scaled_size % 2); scaled_size--; // make scaled_size odd
    QRect copyRect = QRect( (mousePos.x()-scaled_size/2),
                            (mousePos.y()-scaled_size/2),
                            scaled_size, scaled_size);
    img = img.copy(copyRect).scaledToWidth(size);

    // fill zoomed image into rect
    painter.translate(width() -size -Margin, height() -size -Margin);
    QRect zoomRect = QRect(0, 0, size, size);
    painter.drawImage(zoomRect, img);
    painter.setPen(QColor(255, 255, 255));
    painter.drawRect(zoomRect);

    // mark pixel under cursor
    int xOffset, yOffset, w, h;
    p_currentRenderer->getRenderSize(xOffset, yOffset, w, h);
    const int pixelSize = qRound((float)size / (float)scaled_size);
    QRect pixelRect = QRect((size -pixelSize)/ 2, (size -pixelSize)/ 2, pixelSize, pixelSize);
    painter.drawRect(pixelRect);

    // draw pixel info
    int x = qRound((float)(mousePos.x() - xOffset));
    int y = qRound((float)(mousePos.y() - (height() - h - yOffset)));
    int value = p_currentRenderObjectLeft->getPixelValue(x,y);
    unsigned char *components = reinterpret_cast<unsigned char*>(&value);

    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(QString("<h4>Coordinates:</h4>"
                                 "<table width=\"100%\">"
                                 "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
                                 "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
                                 "</table><br />"
                                 "<h4>Values:</h4>"
                                 "<table width=\"100%\">"
                                 "<tr><td>Y:</td><td align=\"right\">%3</td></tr>"
                                 "<tr><td>U:</td><td align=\"right\">%4</td></tr>"
                                 "<tr><td>V:</td><td align=\"right\">%5</td></tr>"
                                 "</table>"
                                 ).arg(x).arg(y).arg((unsigned int)components[3]).arg((unsigned int)components[2]).arg((unsigned int)components[1])
            );
    textDocument.setTextWidth(textDocument.size().width());

    QRect rect(QPoint(0, 0), textDocument.size().toSize()
               + QSize(2 * Padding, 2 * Padding));
    painter.translate(-rect.width(), size - rect.height());
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawRect(rect);
    painter.translate(Padding, Padding);
    textDocument.drawContents(&painter);
    painter.end();

    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,glMagFilterParam);
    if (*glMagFilterParam==GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);

    glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glMinFilterParam);
    if (*glMinFilterParam==GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
}

void RenderWidget::resizeGL(int width, int height)
{
    if (!p_gl_initialized)
        return;

    if(p_currentRenderObjectLeft == NULL)
        return;

    if( p_customViewport  )
        return;

    int widthOffset = 0;
    int heightOffset = 0;

    float viewWidth = p_videoWidth;
    float viewHeight = p_videoHeight;

    float newWidth = p_zoomFactor * viewWidth;
    float newHeight = p_zoomFactor * viewHeight;

    widthOffset = ( (int)width - (int)newWidth ) / 2 ;
    heightOffset = ( (int)height - (int)newHeight ) / 2 ;

    // update our viewport
    p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
}

void RenderWidget::mousePressEvent(QMouseEvent* e)
{
    if(p_currentRenderObjectLeft == NULL)
        return;

    switch (e->button()) {
    case Qt::LeftButton:
        // Start selection.
        p_selectionStartPoint = e->pos();

        p_selectionRect = QRect();

        selectionMode_ = SELECT;
        break;
    case Qt::MiddleButton:
        p_selectionStartPoint = e->pos();

        p_currentRenderer->getRenderOffset(p_dragRenderOffsetStartX, p_dragRenderOffsetStartY);
        selectionMode_ = DRAG;
        break;
    default:
        QGLWidget::mousePressEvent(e);
    }
}

void RenderWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (p_zoomBoxEnabled) {
        setCurrentMousePosition(e->pos());
        update();

        if (p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
        {
            p_RenderWidget2->setCurrentMousePosition(e->pos());
            p_RenderWidget2->update();
        }
    }

    switch (selectionMode_) {
    case SELECT:
        // Updates rectangle_ coordinates and redraws rectangle
        p_selectionEndPoint = e->pos();

        p_selectionRect.setLeft( MIN( p_selectionStartPoint.x(), p_selectionEndPoint.x() ) );
        p_selectionRect.setRight( MAX( p_selectionStartPoint.x(), p_selectionEndPoint.x() ) );
        p_selectionRect.setTop( MIN( p_selectionStartPoint.y(), p_selectionEndPoint.y() ) );
        p_selectionRect.setBottom( MAX( p_selectionStartPoint.y(), p_selectionEndPoint.y() ) );

        if( p_selectionRect.width() > 1 )
            update();
        break;
    case DRAG:
        if (p_currentRenderer == NULL)
            break;
        p_customViewport = true;
        p_currentRenderer->setRenderOffset(p_dragRenderOffsetStartX + e->pos().x() - p_selectionStartPoint.x(), p_dragRenderOffsetStartY -e->pos().y() + p_selectionStartPoint.y());
        update();
        if (p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
        {
            p_RenderWidget2->currentRenderer()->setRenderOffset(p_dragRenderOffsetStartX + e->pos().x() - p_selectionStartPoint.x(), p_dragRenderOffsetStartY -e->pos().y() + p_selectionStartPoint.y());
            p_RenderWidget2->update();
        }
        break;

    default:
        QGLWidget::mouseMoveEvent(e);
    }
}

void RenderWidget::mouseReleaseEvent(QMouseEvent* e)
{
    switch (selectionMode_) {
    case SELECT:
        if( abs(p_selectionRect.width()) > 10 && abs(p_selectionRect.height()) > 10 )
        {
            float viewWidth = p_videoWidth;
            float viewHeight = p_videoHeight;

            float w_ratio = (float)width() / (float)p_selectionRect.width();
            float h_ratio = (float)height() / (float)p_selectionRect.height();

            float ratio = MIN(w_ratio, h_ratio);

            float newWidth = p_zoomFactor * ratio * viewWidth;
            float newHeight = p_zoomFactor * ratio * viewHeight;

            // TODO: zoom to maximum
            if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
                return;

            p_zoomFactor *= ratio;

            int widthOffset = 0;
            int heightOffset = 0;
            p_currentRenderer->getRenderOffset(widthOffset, heightOffset);

            if (w_ratio < h_ratio) {
                widthOffset = (float)(widthOffset - p_selectionRect.x()) * ratio ;
                heightOffset = (float)(heightOffset - height() + p_selectionRect.bottom()) * ratio;
                heightOffset = heightOffset + ( height() - (float)p_selectionRect.height() * ratio ) / 2 ;
            } else {
                heightOffset = (float)(heightOffset - height() + p_selectionRect.bottom()) * ratio;
                widthOffset = (float)(widthOffset - p_selectionRect.x()) * ratio ;
                widthOffset = widthOffset + ( width() - (float)p_selectionRect.width() * ratio ) / 2 ;
            }

            // update our viewport
            p_customViewport = true;
            p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

            update();
            if (p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
            {
                p_RenderWidget2->currentRenderer()->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
                p_RenderWidget2->setZoomFactor(p_zoomFactor);
                p_RenderWidget2->update();
            }
        }
        else if (p_zoomFactor > 1)
        {
            zoomToStandard();
            if(p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
            {
                p_RenderWidget2->zoomToStandard();
            }
        }
        // when mouse is released, we don't draw the selection any longer
        selectionMode_ = NONE;
        update();
        break;
    case DRAG:
        selectionMode_ = NONE;
        update();
        break;
    default:
        QGLWidget::mouseReleaseEvent(e);
    }
}

void RenderWidget::wheelEvent (QWheelEvent *e) {
    QPoint p = e->pos();
    e->accept();


    if (e->delta() > 0)
    {
        zoomIn(&p);
        if(p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
        {
            p_RenderWidget2->zoomIn(&p);
        }
    }
    else
    {
        zoomOut(&p);
        if(p_RenderWidget2!=NULL && p_RenderWidget2->isReady())
        {
            p_RenderWidget2->zoomOut(&p);
        }
    }
}

void RenderWidget::zoomIn(QPoint *to, float factor)
{
    if (p_zoomBoxEnabled)
    {
        p_boxZoomFactor = MIN(p_boxZoomFactor*factor, ZOOMBOX_MAX_FACTOR);
    }
    else
    {
        if (p_currentRenderer != 0) {
            int widthOffset = 0;
            int heightOffset = 0;

            float viewWidth = p_videoWidth;
            float viewHeight = p_videoHeight;

            float newWidth = p_zoomFactor * factor * viewWidth;
            float newHeight = p_zoomFactor * factor * viewHeight;

            if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
                return;

            p_zoomFactor *= factor;

            if (to == 0) {
                widthOffset = ( (int)size().width() - (int)newWidth ) / 2 ;
                heightOffset = ( (int)size().height() - (int)newHeight ) / 2 ;
            } else {
                p_currentRenderer->getRenderOffset(widthOffset, heightOffset);
                float mousex = to->x() - widthOffset;
                float mousey = to->y() - height() + heightOffset;
                widthOffset += mousex - mousex * factor;
                heightOffset -= mousey - mousey * factor;
            }

            // update our viewport
            p_customViewport = true;
            p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
        }
    }
    update();
}

void RenderWidget::zoomOut(QPoint *to, float factor)
{
    if (p_zoomBoxEnabled)
    {
        p_boxZoomFactor = MAX(p_boxZoomFactor/factor, ZOOMBOX_MIN_FACTOR);
    }
    else
    {
        if (p_currentRenderer!=NULL) {

            p_zoomFactor /= factor;

            int widthOffset = 0;
            int heightOffset = 0;

            float viewWidth = p_videoWidth;
            float viewHeight = p_videoHeight;

            float newWidth = p_zoomFactor * viewWidth;
            float newHeight = p_zoomFactor * viewHeight;
            if (to == 0) {
                widthOffset = ( (int)size().width() - (int)newWidth ) / 2 ;
                heightOffset = ( (int)size().height() - (int)newHeight ) / 2 ;
            } else {
                p_currentRenderer->getRenderOffset(widthOffset, heightOffset);
                float mousex = to->x() - widthOffset;
                float mousey = to->y() - height() + heightOffset;
                widthOffset += mousex - mousex / 1.2;
                heightOffset -= mousey - mousey / 1.2;
            }

            // update our viewport
            p_customViewport = true;
            p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
        }
    }
    update();
}

void RenderWidget::zoomToFit()
{
    if (p_zoomBoxEnabled)
        return;

    float viewWidth = p_videoWidth;
    float viewHeight = p_videoHeight;

    float w_ratio = width() / viewWidth;
    float h_ratio = height() / viewHeight;

    float ratio = MIN(w_ratio, h_ratio);

    int widthOffset = 0;
    int heightOffset = 0;

    float newWidth = ratio * viewWidth;
    float newHeight = ratio * viewHeight;

    if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
        return;

    p_zoomFactor = ratio;

    widthOffset = ( (int)width() - (int)newWidth ) / 2 ;
    heightOffset = ( (int)height() - (int)newHeight ) / 2 ;

    // update our viewport
    p_customViewport = true;
    p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

    update();
}

void RenderWidget::zoomToStandard()
{
    p_zoomFactor = 1.0;

    int widthOffset = 0;
    int heightOffset = 0;

    float newWidth = p_videoWidth;
    float newHeight = p_videoHeight;

    widthOffset = ( (int)width() - (int)newWidth ) / 2 ;
    heightOffset = ( (int)height() - (int)newHeight ) / 2 ;

    // update our viewport
    p_customViewport = false;
    p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

    update();
}

void RenderWidget::setZoomBoxEnabled(bool enabled) {
    setViewInterpolationMode(noInterpolation);
    setRenderMode(normalRenderMode);
    p_zoomBoxEnabled = enabled;
    zoomToStandard();
}

bool RenderWidget::getRenderSize(int &xOffset, int &yOffset, int &width, int &height) {
    if (p_currentRenderer != 0) {
        p_currentRenderer->getRenderSize(xOffset, yOffset, width, height);
        return true;
    }
    else
        return false;
}

void RenderWidget::setViewInterpolationMode(int newMode) {
    if (!p_gl_initialized)
        return;

    ViewInterpolator* newInterpolator=NULL;
    // don't do anything if interpolationMode is allready up-to-date
    if (newMode == p_viewInterpolationMode)
        return;

    // first save new stereo mode
    if (newMode < 0)
        p_viewInterpolationMode = unspecifiedInterpolation;
    else
        p_viewInterpolationMode = (interpolation_mode_t)newMode;

    switch (newMode) {
    case noInterpolation:
        if( p_noInterpolator == NULL )
            p_noInterpolator = new NoInterpolator();
        newInterpolator = p_noInterpolator;
        break;
    case fullInterplation:
        if( p_fullInterpolator == NULL )
            p_fullInterpolator = new FullInterpolator();
        newInterpolator = p_fullInterpolator;
        break;
    case unspecifiedInterpolation:
        return;
    }

    if (p_currentViewInterpolator != 0)
        p_currentViewInterpolator->deactivate();

    p_currentViewInterpolator = newInterpolator;
    QSettings settings;
    p_currentViewInterpolator->setBlendingMode((blending_t)settings.value("Interpolation/BlendingMode").toInt());
    p_currentViewInterpolator->activate();

    p_currentViewInterpolator->setInterpolationFactor(p_interpolationFactor);
    p_currentViewInterpolator->setEyeDistance(p_eyeDistance);
    p_currentViewInterpolator->setOutputTextureUnits(p_renderTextures[0], p_renderTextures[1]);
    p_currentViewInterpolator->setCurrentRenderObjects(p_currentRenderObjectLeft, p_currentRenderObjectRight);
    p_currentViewInterpolator->setCurrentDepthObjects(p_currentDepthObjectLeft, p_currentDepthObjectRight);

    update();
}

void RenderWidget::setViewInterpolationFactor(float f) {
    p_interpolationFactor = f;
    p_currentViewInterpolator->setInterpolationFactor(f);
    update();
}

void RenderWidget::setEyeDistance(double dist) {
    p_eyeDistance = dist;
    if (p_currentViewInterpolator != 0)
        p_currentViewInterpolator->setEyeDistance(dist);
    update();
}

void RenderWidget::setGridParameters(bool show, int size, unsigned char color[]) {
    p_drawGrid = show;
    p_gridSize = size;
    p_gridColor[0] = color[0];
    p_gridColor[1] = color[1];
    p_gridColor[2] = color[2];
    p_gridColor[3] = color[3];
    update();
}

void RenderWidget::setStatisticsParameters(bool doSimplify, int threshold, unsigned char color[3]) {
    p_simplifyStats = doSimplify;
    p_simplificationTheshold = threshold;
    p_simplifiedGridColor[0] = color[0];
    p_simplifiedGridColor[1] = color[1];
    p_simplifiedGridColor[2] = color[2];
    update();
}

void RenderWidget::updateTex()
{
    update();
}


void RenderWidget::rotateVector(float angle, float vx, float vy, float &nx, float &ny) const
{
    float s = sinf(angle);
    float c = cosf(angle);

    nx = c * vx - s * vy;
    ny = s * vx + c * vy;

    // normalize vector
    float n_abs = sqrtf( nx*nx + ny*ny );
    nx /= n_abs;
    ny /= n_abs;
}

