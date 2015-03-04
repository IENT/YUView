#include "displaywidget.h"
#include "frameobject.h"

#include <QPainter>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>
#include <QTextDocument>
#define _USE_MATH_DEFINES
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

DisplayWidget::DisplayWidget(QWidget *parent) : QWidget(parent)
{
    p_drawGrid =  false;
    p_gridSize = 64;

    p_displayObject = NULL;
    p_overlayStatisticsObject = NULL;

    p_displayRect = QRect();

    p_selectionRect = QRect();
    p_zoomBoxPoint = QPoint();

    QPalette Pal(palette());
    // load color from preferences
    QSettings settings;
    QColor bgColor = settings.value("Background/Color").value<QColor>();
    Pal.setColor(QPalette::Background, bgColor);
    setAutoFillBackground(true);
    setPalette(Pal);
}

DisplayWidget::~DisplayWidget() {
}

void DisplayWidget::drawFrame(unsigned int frameIdx)
{
    // make sure that frame objects contain requested frame
    if( p_displayObject != NULL )
        p_displayObject->loadImage(frameIdx);
    if( p_overlayStatisticsObject )
        p_overlayStatisticsObject->loadImage(frameIdx);

    // redraw -- CHECK: repaint() might be an alternative here?!
    repaint();
}

void DisplayWidget::drawFrame()
{
    QImage image = p_displayObject->displayImage();

    if(p_displayRect.isEmpty())
    {
        int offsetX = (width() - image.width())/2;
        int offsetY = (height() - image.height())/2;
        QPoint topLeft(offsetX, offsetY);
        QPoint bottomRight(image.width() + offsetX, image.height() + offsetY);
        p_displayRect = QRect(topLeft, bottomRight);
    }

    //draw Frame
    QPainter painter(this);
    painter.drawImage(p_displayRect, image, image.rect());
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

    if(p_displayRect.isEmpty())
    {
        int offsetX = (width() - overlayImage.width())/2;
        int offsetY = (height() - overlayImage.height())/2;
        QPoint topLeft(offsetX, offsetY);
        QPoint bottomRight(overlayImage.width() + offsetX, overlayImage.height() + offsetY);
        p_displayRect = QRect(topLeft, bottomRight);
    }

    //draw Frame
    QPainter painter(this);
    painter.drawImage(p_displayRect, overlayImage, overlayImage.rect());
}

void DisplayWidget::clear()
{
    QPalette Pal(palette());
    // load color from preferences
    QSettings settings;
    QColor bgColor = settings.value("Background/Color").value<QColor>();
    Pal.setColor(QPalette::Background, bgColor);
    setAutoFillBackground(true);
    setPalette(Pal);

    // TODO: clear?!
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
        if (!p_selectionRect.isEmpty())
        {
            drawSelectionRectangle();
        }

        // draw Zoombox
        if (!p_zoomBoxPoint.isNull())
        {
            drawZoomBox();
        }
    }
}

void DisplayWidget::drawSelectionRectangle()
{
    QPainter painter(this);
    painter.drawRect(p_selectionRect);
}

void DisplayWidget::drawZoomBox()
{
    // check if we have at least one object to draw
    if( p_displayObject == NULL )
        return;

//    QPainter painter(this);

//    // TODO: Get the current draw buffer
//    //QImage img = grabFrameBuffer();
//    QImage img;

//    // zoom in
//    const int size = 200;
//    const int Margin = 11;
//    const int Padding = 6;
//    int scaled_size = qRound((float)size / p_boxZoomFactor);
//    scaled_size += (scaled_size % 2); scaled_size--; // make scaled_size odd
//    QRect copyRect = QRect( (mousePos.x()-scaled_size/2),
//                            (mousePos.y()-scaled_size/2),
//                            scaled_size, scaled_size);
//    img = img.copy(copyRect).scaledToWidth(size);

//    // fill zoomed image into rect
//    painter.translate(width() -size -Margin, height() -size -Margin);
//    QRect zoomRect = QRect(0, 0, size, size);
//    painter.drawImage(zoomRect, img);
//    painter.setPen(QColor(255, 255, 255));
//    painter.drawRect(zoomRect);

//    // mark pixel under cursor
//    int xOffset, yOffset, w, h;
//    //p_currentRenderer->getRenderSize(xOffset, yOffset, w, h);
//    const int pixelSize = qRound((float)size / (float)scaled_size);
//    QRect pixelRect = QRect((size -pixelSize)/ 2, (size -pixelSize)/ 2, pixelSize, pixelSize);
//    painter.drawRect(pixelRect);

//    // draw pixel info
//    int x = qRound((float)(mousePos.x() - xOffset));
//    int y = qRound((float)(mousePos.y() - (height() - h - yOffset)));
//    int value = 0; //TODO: p_displayObject->getPixelValue(x,y);
//    unsigned char *components = reinterpret_cast<unsigned char*>(&value);

//    QTextDocument textDocument;
//    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
//    textDocument.setHtml(QString("<h4>Coordinates:</h4>"
//                                 "<table width=\"100%\">"
//                                 "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
//                                 "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
//                                 "</table><br />"
//                                 "<h4>Values:</h4>"
//                                 "<table width=\"100%\">"
//                                 "<tr><td>Y:</td><td align=\"right\">%3</td></tr>"
//                                 "<tr><td>U:</td><td align=\"right\">%4</td></tr>"
//                                 "<tr><td>V:</td><td align=\"right\">%5</td></tr>"
//                                 "</table>"
//                                 ).arg(x).arg(y).arg((unsigned int)components[3]).arg((unsigned int)components[2]).arg((unsigned int)components[1])
//            );
//    textDocument.setTextWidth(textDocument.size().width());

//    QRect rect(QPoint(0, 0), textDocument.size().toSize()
//               + QSize(2 * Padding, 2 * Padding));
//    painter.translate(-rect.width(), size - rect.height());
//    painter.setBrush(QColor(0, 0, 0, 70));
//    painter.drawRect(rect);
//    painter.translate(Padding, Padding);
//    textDocument.drawContents(&painter);
//    painter.end();
}

void DisplayWidget::setRegularGridParameters(bool show, int size, QColor gridColor) {
    p_drawGrid = show;
    p_gridSize = size;
    p_gridColor = gridColor;

    update();
}
