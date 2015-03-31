/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "displaywidget.h"
#include "frameobject.h"

#include <QPainter>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>
#include <QTextDocument>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
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

void DisplayWidget::drawFrame(int frameIdx)
{
    // make sure that frame objects contain requested frame
    if( p_displayObject != NULL )
        p_displayObject->loadImage(frameIdx);
    if( p_overlayStatisticsObject )
        p_overlayStatisticsObject->loadImage(frameIdx);

    // redraw -- CHECK: repaint() might be an alternative here?!
    repaint();
}

QPixmap DisplayWidget::captureScreenshot()
{
    QPixmap pixmap;
    QImage tmpImage(p_displayRect.size(), QImage::Format_ARGB32);
    tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
    pixmap.convertFromImage(tmpImage);

    // TODO: check if it is possible to _not_ draw the widget's background
    render(&pixmap, QPoint(), QRegion(p_displayRect), RenderFlags(DrawChildren));

    return pixmap;
}

void DisplayWidget::resetView()
{
    p_displayRect.setWidth(displayObject()->width());
    p_displayRect.setHeight(displayObject()->height());
}

void DisplayWidget::drawFrame()
{
    //draw Frame
    QPainter painter(this);
    QPixmap image = p_displayObject->displayImage();
    painter.drawPixmap(p_displayRect, image, image.rect());
}

void DisplayWidget::drawRegularGrid()
{
    // draw regular grid
    QPainter painter(this);
    float stepSize = (float)p_gridSize*zoomFactor();
    for (float i=0; i<=p_displayRect.width(); i+=stepSize)
    {
        QPointF start = p_displayRect.topLeft() + QPointF(i,0);
        QPointF end = p_displayRect.bottomLeft() + QPointF(i,0);
        painter.drawLine(start, end);
    }
    for (float i=0; i<=p_displayRect.height(); i+=stepSize)
    {
        QPointF start = p_displayRect.topLeft() + QPointF(0,i);
        QPointF end = p_displayRect.topRight() + QPointF(0,i);
        painter.drawLine(start, end);
    }
}

void DisplayWidget::drawStatisticsOverlay()
{
    //draw Frame
    QPainter painter(this);
    QPixmap overlayImage = p_overlayStatisticsObject->displayImage();
    painter.drawPixmap(p_displayRect, overlayImage, overlayImage.rect());
}

void DisplayWidget::drawZoomFactor()
{
    QString zoomString;

    if( zoomFactor() >= 1.0 )
        zoomString = QString::number((int)zoomFactor()) + " ×";
    else
        zoomString = "1/" + QString::number((int)floor(1.0/zoomFactor())) + " ×";

    QFont font = QFont("Helvetica", 24);
    QFontMetrics fm(font);
    int height = fm.height()*(zoomString.count("\n")+1);

    QPoint targetPoint( 10, 10+height );

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen( QColor(Qt::black) );
    painter.setFont( font );
    painter.drawText(targetPoint, zoomString);
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

    // actually clear by drawing an invalid frame index
    drawFrame(INT_INVALID);
}

void DisplayWidget::paintEvent(QPaintEvent*)
{
    // check if we have at least one object to draw
    if( p_displayObject != NULL )
    {
        drawFrame();

        // if there is a zoom, show it
        if( zoomFactor() != 1.0 )
        {
            drawZoomFactor();
        }

        // draw overlay, if requested
        if(p_overlayStatisticsObject)
        {
            drawStatisticsOverlay();
        }

        if(p_drawGrid)
        {
            drawRegularGrid();
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
    // position within image with range [0:width-1;0:height-1]
    int srcX = ((double)p_zoomBoxPoint.x() - (double)p_displayRect.left() + 0.5)/zoomFactor();
    int srcY = ((double)p_zoomBoxPoint.y() - (double)p_displayRect.top() + 0.5)/zoomFactor();
    QPoint srcPoint = QPoint(srcX, srcY);

    ValuePairList valuesAtPos = p_displayObject->getValuesAt( srcPoint.x(), srcPoint.y() );

    // zoom in
    const int zoomBoxFactor = 32;
    const int srcSize = 5;
    const int targetSize = srcSize*zoomBoxFactor;
    const int margin = 11;
    const int padding = 6;    

    QPainter painter(this);
    QSettings settings;
    QColor bgColor = settings.value("Background/Color").value<QColor>();

    // mark pixel under cursor
    int zoomFactorInt = (int)zoomFactor();
    // restrict this marker to pixel grid
    int xPixel = (srcX*zoomFactorInt) + p_displayRect.left();//((p_zoomBoxPoint.x()-(zoomFactorInt>>1))/zoomFactorInt)*zoomFactorInt;
    int yPixel = (srcY*zoomFactorInt) + p_displayRect.top();//((p_zoomBoxPoint.y()-(zoomFactorInt>>1))/zoomFactorInt)*zoomFactorInt;
    QRect pixelRect = QRect(xPixel, yPixel, zoomFactorInt, zoomFactorInt);
    painter.drawRect(pixelRect);

    // translate to lower right corner
    painter.translate(width()-targetSize-margin, height()-targetSize-margin);

    // fill zoomed image into rect
    QPixmap image = p_displayObject->displayImage();
    int internalScaleFactor = p_displayObject->internalScaleFactor();
    QRect srcRect = QRect((srcPoint.x()-(srcSize>>1))*internalScaleFactor, (srcPoint.y()-(srcSize>>1))*internalScaleFactor, srcSize*internalScaleFactor, srcSize*internalScaleFactor);
    QRect targetRect = QRect(0, 0, targetSize, targetSize);

    painter.fillRect(targetRect, bgColor);
    painter.drawPixmap(targetRect, image, srcRect);

    // if we have an overlayed statistics image, draw it also and get pixel value from there...
    if(p_overlayStatisticsObject)
    {
        QPixmap overlayImage = p_overlayStatisticsObject->displayImage();

        // draw overlay
        painter.drawPixmap(targetRect, overlayImage, srcRect);

        // use overlay raw value
        valuesAtPos = p_overlayStatisticsObject->getValuesAt( srcPoint.x(), srcPoint.y() );
    }

    // draw border
    painter.drawRect(targetRect);

    // mark pixel under cursor in zoom box
    const int srcPixelSize = 1;
    const int targetPixelSize = srcPixelSize*zoomBoxFactor;
    QRect pixelRectZoomed = QRect((targetSize-targetPixelSize)/2, (targetSize-targetPixelSize)/2, targetPixelSize, targetPixelSize);
    painter.drawRect(pixelRectZoomed);

    // draw pixel info
    QString pixelInfoString = QString("<h4>Coordinates</h4>"
                              "<table width=\"100%\">"
                              "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
                              "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
                              "</table>"
                              ).arg(srcPoint.x()).arg(srcPoint.y());

    // if we have some values, show them
    if( valuesAtPos.size() > 0 )
    {
        pixelInfoString.append( "<h4>Values</h4>"
                                "<table width=\"100%\">" );
        for (int i = 0; i < valuesAtPos.size(); ++i)
        {
             pixelInfoString.append( QString("<tr><td>%1:</td><td align=\"right\">%2</td></tr>").arg(valuesAtPos[i].first).arg(valuesAtPos[i].second) );
        }
        pixelInfoString.append( "</table>" );
    }

    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(pixelInfoString);
    textDocument.setTextWidth(textDocument.size().width());

    QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2*padding, 2*padding));
    painter.translate(-rect.width(), targetSize-rect.height());
    painter.setBrush(QColor(0, 0, 0, 70));
    painter.drawRect(rect);
    painter.translate(padding, padding);
    textDocument.drawContents(&painter);

    painter.end();
}

void DisplayWidget::setRegularGridParameters(bool show, int size, QColor gridColor) {
    p_drawGrid = show;
    p_gridSize = size;
    p_gridColor = gridColor;

    update();
}
