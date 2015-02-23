#include "displaywidget.h"
#include "frameobject.h"

#include <QPainter>
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

DisplayWidget::DisplayWidget(QWidget *parent) : QWidget(parent)
{
    selectionMode_ = NONE;

    p_zoomFactor = 1.0;
    p_boxZoomFactor = ZOOMBOX_MIN_FACTOR;
    p_zoomBoxEnabled = false;
    p_customViewport = false;

    p_drawGrid =  false;
    p_gridSize = 64;

    p_displayObject = NULL;
    p_overlayStatisticsObject = NULL;

    p_drawStatisticsOverlay = false;

    QPalette Pal(palette());
    // TODO: load color from preferences
    Pal.setColor(QPalette::Background, Qt::black);
    setAutoFillBackground(true);
    setPalette(Pal);
}

DisplayWidget::~DisplayWidget() {
}

void DisplayWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QWidget::dragEnterEvent(event);
}

void DisplayWidget::dropEvent(QDropEvent *event)
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

                if( fi.isDir() || ext == "yuv" || ext == "xml" || ext == "csv" )
                    fileList.append(fileName);
            }

            event->acceptProposedAction();

            mainWindow->loadFiles(fileList);
        }
    }
    QWidget::dropEvent(event);
}

void DisplayWidget::drawFrame(unsigned int frameIdx)
{
    // make sure that frame objects contain requested frame
    if( p_displayObject != NULL )
        p_displayObject->loadImage(frameIdx);
    if( p_overlayStatisticsObject )
        p_overlayStatisticsObject->loadImage(frameIdx);

    // redraw -- CHECK: redraw() might be an alternative here?!
    update();
}

void DisplayWidget::drawFrame()
{
    QImage image = p_displayObject->displayImage();

    QPainter painter(this);
    int offsetX = (width() - image.width())/2;
    int offsetY = (height() - image.height())/2;
    QPoint topLeft(offsetX, offsetY);
    QPoint bottomRight(image.width() + offsetX, image.height() + offsetY);
    QRect imageRect(topLeft, bottomRight);

    //draw Frame
    painter.drawImage(imageRect, image, image.rect());
}

void DisplayWidget::drawRegularGrid()
{
    QImage image = p_displayObject->displayImage();

    QPainter painter(this);
    int offsetX = (width() - image.width())/2;
    int offsetY = (height() - image.height())/2;
    QPoint topLeft(offsetX, offsetY);
    QPoint bottomRight(image.width() + offsetX, image.height() + offsetY);
    QRect imageRect(topLeft, bottomRight);

    // draw regular grid
    for (int i=0; i<imageRect.width(); i+=p_gridSize)
    {
        QPoint start = imageRect.topLeft() + QPoint(i,0);
        QPoint end = imageRect.bottomLeft() + QPoint(i,0);
        painter.drawLine(start, end);
    }
    for (int i=0; i<imageRect.height(); i+=p_gridSize)
    {
        QPoint start = imageRect.bottomLeft() - QPoint(0,i);
        QPoint end = imageRect.bottomRight() - QPoint(0,i);
        painter.drawLine(start, end);
    }
}

void DisplayWidget::drawStatisticsOverlay()
{
    QImage overlayImage = p_overlayStatisticsObject->displayImage();

    QPainter painter(this);
    QPoint topLeft ((this->width()- overlayImage.width())/2, ((this->height()- overlayImage.height())/2));
    QPoint bottomRight ((this->width()- overlayImage.width())/2+overlayImage.width(), ((this->height()- overlayImage.height())/2 - overlayImage.height()));
    QRect imageRect(topLeft, bottomRight);

    //draw Frame
    painter.drawImage(imageRect, overlayImage, overlayImage.rect());
}

void DisplayWidget::clear()
{
    // TODO: still necessary?
}

void DisplayWidget::paintEvent(QPaintEvent * event)
{
    // check if we have at least one object to draw
    if( p_displayObject != NULL )
    {
        drawFrame();

        if(p_drawGrid)
        {
            drawRegularGrid();
        }

        // draw overlay, if requested
        if(p_overlayStatisticsObject)
        {
            drawStatisticsOverlay();
        }

        // draw rectangular selection area.
        if (selectionMode_ == SELECT)
        {
            drawSelectionRectangle();
        }

        // draw Zoombox
        if (p_zoomBoxEnabled)
        {
            drawZoomBox(p_currentMousePosition);
        }
    }
}

void DisplayWidget::drawSelectionRectangle()
{
    // TODO: draw with QPainter instead of OpenGL

//    GLdouble objbl_x, objbr_x, objtr_x, objtl_x;
//    GLdouble objbl_y, objbr_y, objtr_y, objtl_y;
//    GLdouble objbl_z, objbr_z, objtr_z, objtl_z;
//    GLint viewport[4];
//    GLdouble mvmatrix[16], projmatrix[16];

//    glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
//    glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
//    glGetIntegerv(GL_VIEWPORT, viewport);

//    GLint real_top = height() - (GLint) p_selectionRect.top();
//    GLint real_bot = height() - (GLint) p_selectionRect.bottom();

//    gluUnProject( p_selectionRect.left(), real_bot, 0, mvmatrix, projmatrix, viewport, &objbl_x, &objbl_y, &objbl_z );
//    gluUnProject( p_selectionRect.right(), real_bot, 0, mvmatrix, projmatrix, viewport, &objbr_x, &objbr_y, &objbr_z );
//    gluUnProject( p_selectionRect.right(), real_top, 0, mvmatrix, projmatrix, viewport, &objtr_x, &objtr_y, &objtr_z );
//    gluUnProject( p_selectionRect.left(), real_top, 0, mvmatrix, projmatrix, viewport, &objtl_x, &objtl_y, &objtl_z );

//    glPushMatrix();

//    glEnable(GL_BLEND);
//    glDisable(GL_TEXTURE_2D);
//    glDisable(GL_TEXTURE_RECTANGLE_EXT);

//    glColor4f(0.95f, 0.49f, 0.07f, 0.60f);
//    glBegin(GL_QUADS);
//    glVertex3f(objtl_x,  objtl_y, 0.7f);
//    glVertex3f(objtr_x, objtr_y, 0.7f);
//    glVertex3f(objbr_x, objbr_y, 0.7f);
//    glVertex3f(objbl_x,  objbl_y, 0.7f);
//    glEnd();

//    glLineWidth(2.0);
//    glColor4f(1.0f, 1.0f, 1.0f, 0.75f);
//    glBegin(GL_LINE_LOOP);
//    glVertex3f(objtl_x,  objtl_y, 0.6f);
//    glVertex3f(objtr_x, objtr_y, 0.6f);
//    glVertex3f(objbr_x, objbr_y, 0.6f);
//    glVertex3f(objbl_x,  objbl_y, 0.6f);
//    glEnd();

//    glPopMatrix();

//    glDisable(GL_BLEND);
//    glEnable(GL_TEXTURE_2D);
//    glEnable(GL_TEXTURE_RECTANGLE_EXT);
//    //stopScreenCoordinatesSystem();
}

void DisplayWidget::drawZoomBox(QPoint mousePos)
{
    // check if we have at least one object to draw
    if( p_displayObject == NULL )
        return;

    QPainter painter(this);

    // TODO: Get the current draw buffer
    //QImage img = grabFrameBuffer();
    QImage img;

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
    //p_currentRenderer->getRenderSize(xOffset, yOffset, w, h);
    const int pixelSize = qRound((float)size / (float)scaled_size);
    QRect pixelRect = QRect((size -pixelSize)/ 2, (size -pixelSize)/ 2, pixelSize, pixelSize);
    painter.drawRect(pixelRect);

    // draw pixel info
    int x = qRound((float)(mousePos.x() - xOffset));
    int y = qRound((float)(mousePos.y() - (height() - h - yOffset)));
    int value = 0; //TODO: p_displayObject->getPixelValue(x,y);
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
}

void DisplayWidget::resizeEvent(QResizeEvent * event)
{

}

void DisplayWidget::mousePressEvent(QMouseEvent* e)
{
    if(p_displayObject == NULL)
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

        //p_currentRenderer->getRenderOffset(p_dragRenderOffsetStartX, p_dragRenderOffsetStartY);
        selectionMode_ = DRAG;
        break;
    default:
        QWidget::mousePressEvent(e);
    }
}

void DisplayWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (p_zoomBoxEnabled) {
        setCurrentMousePosition(e->pos());
        update();
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
        p_customViewport = true;
        //p_currentRenderer->setRenderOffset(p_dragRenderOffsetStartX + e->pos().x() - p_selectionStartPoint.x(), p_dragRenderOffsetStartY -e->pos().y() + p_selectionStartPoint.y());
        update();
        break;

    default:
        QWidget::mouseMoveEvent(e);
    }
}

void DisplayWidget::mouseReleaseEvent(QMouseEvent* e)
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
//            if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
//                return;

            p_zoomFactor *= ratio;

            int widthOffset = 0;
            int heightOffset = 0;
            //p_currentRenderer->getRenderOffset(widthOffset, heightOffset);

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
            //p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

            update();
        }
        else if (p_zoomFactor > 1)
        {
            zoomToStandard();
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
        QWidget::mouseReleaseEvent(e);
    }
}

void DisplayWidget::wheelEvent (QWheelEvent *e) {
    QPoint p = e->pos();
    e->accept();

    if (e->delta() > 0)
    {
        zoomIn(&p);
    }
    else
    {
        zoomOut(&p);
    }
}

void DisplayWidget::zoomIn(QPoint *to, float factor)
{
    if (p_zoomBoxEnabled)
    {
        p_boxZoomFactor = MIN(p_boxZoomFactor*factor, ZOOMBOX_MAX_FACTOR);
    }
    else
    {
        int widthOffset = 0;
        int heightOffset = 0;

        float viewWidth = p_videoWidth;
        float viewHeight = p_videoHeight;

        float newWidth = p_zoomFactor * factor * viewWidth;
        float newHeight = p_zoomFactor * factor * viewHeight;

//        if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
//            return;

        p_zoomFactor *= factor;

        if (to == 0) {
            widthOffset = ( (int)size().width() - (int)newWidth ) / 2 ;
            heightOffset = ( (int)size().height() - (int)newHeight ) / 2 ;
        } else {
            //p_currentRenderer->getRenderOffset(widthOffset, heightOffset);
            float mousex = to->x() - widthOffset;
            float mousey = to->y() - height() + heightOffset;
            widthOffset += mousex - mousex * factor;
            heightOffset -= mousey - mousey * factor;
        }

        // update our viewport
        p_customViewport = true;
        //p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
    }
    update();
}

void DisplayWidget::zoomOut(QPoint *to, float factor)
{
    if (p_zoomBoxEnabled)
    {
        p_boxZoomFactor = MAX(p_boxZoomFactor/factor, ZOOMBOX_MIN_FACTOR);
    }
    else
    {
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
            //p_currentRenderer->getRenderOffset(widthOffset, heightOffset);
            float mousex = to->x() - widthOffset;
            float mousey = to->y() - height() + heightOffset;
            widthOffset += mousex - mousex / 1.2;
            heightOffset -= mousey - mousey / 1.2;
        }

        // update our viewport
        p_customViewport = true;
        //p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);
    }
    update();
}

void DisplayWidget::zoomToFit()
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

//    if ((newWidth > p_maxViewportDims[0]) || (newHeight > p_maxViewportDims[1]))
//        return;

    p_zoomFactor = ratio;

    widthOffset = ( (int)width() - (int)newWidth ) / 2 ;
    heightOffset = ( (int)height() - (int)newHeight ) / 2 ;

    // update our viewport
    p_customViewport = true;
    //p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

    update();
}

void DisplayWidget::zoomToStandard()
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
    //p_currentRenderer->setRenderSize( widthOffset, heightOffset, (int)newWidth, (int)newHeight);

    update();
}

void DisplayWidget::setZoomBoxEnabled(bool enabled) {
    p_zoomBoxEnabled = enabled;
    zoomToStandard();
}

//bool RenderWidget::getRenderSize(int &xOffset, int &yOffset, int &width, int &height) {
//    if (p_currentRenderer != 0) {
//        p_currentRenderer->getRenderSize(xOffset, yOffset, width, height);
//        return true;
//    }
//    else
//        return false;
//}

void DisplayWidget::setRegularGridParameters(bool show, int size, unsigned char color[]) {
    p_drawGrid = show;
    p_gridSize = size;
    p_gridColor[0] = color[0];
    p_gridColor[1] = color[1];
    p_gridColor[2] = color[2];
    p_gridColor[3] = color[3];

    update();
}

