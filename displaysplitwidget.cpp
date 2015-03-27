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

#include "displaysplitwidget.h"

#include <QMimeData>
#include "mainwindow.h"
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include "math.h"


#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

DisplaySplitWidget::DisplaySplitWidget(QWidget *parent) : QSplitter(parent)
{
    for(int i=0; i<NUM_VIEWS; i++)
    {
        p_displayWidgets[i] = new DisplayWidget(this);
        p_displayWidgets[i]->setMouseTracking(true);
        this->addWidget(p_displayWidgets[i]);
    }

    setAcceptDrops(true);
    setMouseTracking(true);

    selectionMode_ = NONE;
    viewMode_ = SIDE_BY_SIDE;
    p_LastSplitPos=-1;

    p_zoomFactor = 1;
    p_zoomBoxEnabled = false;
    p_selectionStartPoint = QPoint();
    p_selectionEndPoint = QPoint();

    QObject::connect(this, SIGNAL(splitterMoved(int,int)), this, SLOT(splitterMovedTo(int,int)));
}

DisplaySplitWidget::~DisplaySplitWidget()
{
    for(int i=0; i<NUM_VIEWS; i++)
    {
        delete p_displayWidgets[i];
    }
}

void DisplaySplitWidget::resetViews()
{
    for(int i=0; i<NUM_VIEWS; i++)
    {
        if(p_displayWidgets[i]->displayObject())
            p_displayWidgets[i]->resetView();
    }
    updateView();
}

void DisplaySplitWidget::setActiveDisplayObjects( DisplayObject* newPrimaryDisplayObject, DisplayObject* newSecondaryDisplayObject )
{
    p_displayWidgets[LEFT_VIEW]->setDisplayObject(newPrimaryDisplayObject);
    p_displayWidgets[RIGHT_VIEW]->setDisplayObject(newSecondaryDisplayObject);
    resetViews();
}

void DisplaySplitWidget::setActiveStatisticsObjects(StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject)
{
    p_displayWidgets[LEFT_VIEW]->setOverlayStatisticsObject(newPrimaryStatisticsObject);
    p_displayWidgets[RIGHT_VIEW]->setOverlayStatisticsObject(newSecondaryStatisticsObject);
}

// triggered from timer in application
void DisplaySplitWidget::drawFrame(unsigned int frameIdx)
{
    // propagate the draw request to worker widgets
    for( int i=0; i<NUM_VIEWS; i++ )
    {
        p_displayWidgets[i]->drawFrame(frameIdx);
    }
}

QPixmap DisplaySplitWidget::captureScreenshot()
{
    // capture from left widget first
    QPixmap leftScreenshot = p_displayWidgets[LEFT_VIEW]->captureScreenshot();

    if( p_displayWidgets[RIGHT_VIEW]->isHidden() )
        return leftScreenshot;

    QPixmap rightScreenshot = p_displayWidgets[RIGHT_VIEW]->captureScreenshot();

    QSize leftSize = leftScreenshot.size();
    QSize rightSize = rightScreenshot.size();
    QSize mergeSize;

    mergeSize.setWidth( leftSize.width() + rightSize.width() );
    mergeSize.setHeight( MAX( leftSize.height(), rightSize.height() ) );

    QPixmap sideBySideScreenshot(mergeSize);
    QPainter painter(&sideBySideScreenshot);
    painter.drawPixmap(0, 0, leftScreenshot);
    painter.drawPixmap(leftScreenshot.width(), 0, rightScreenshot); // Offset by width of 1st page

    return sideBySideScreenshot;
}

void DisplaySplitWidget::clear()
{
    for( int i=0; i<NUM_VIEWS; i++ )
    {
        p_displayWidgets[i]->clear();
    }
}

void DisplaySplitWidget::setRegularGridParameters(bool show, int size, QColor color)
{
    for( int i=0; i<NUM_VIEWS; i++ )
    {
        p_displayWidgets[i]->setRegularGridParameters(show, size, color);
    }
}

void DisplaySplitWidget::setZoomBoxEnabled(bool enabled)
{
    p_zoomBoxEnabled = enabled;
    if(!p_zoomBoxEnabled)
    {
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            // invalidate the zoomed point
            p_displayWidgets[i]->setZoomBoxPoint(QPoint());
        }
    }
}

bool DisplaySplitWidget::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        if (touchPoints.count()==1)
        {
            const QTouchEvent::TouchPoint &touchPoint = touchPoints.first();
            QPointF currentPoint = touchPoint.pos();
            p_TouchPoint= currentPoint.toPoint();

        }
        if (touchPoints.count()==2)
        {
            const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
            const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
            p_TouchScale = QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
                    / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();
            QPointF pixelPoint0 = touchPoint0.pos();
            QPointF pixelPoint1 = touchPoint1.pos();
            p_TouchPoint = (pixelPoint0.toPoint() + pixelPoint1.toPoint())/2;
        }
        break;
    }
    case QEvent::TouchUpdate:
    {
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
        if (touchPoints.count()==1)
        {
            const QTouchEvent::TouchPoint &touchPoint = touchPoints.first();
            switch (touchPoint.state())
            {
                case Qt::TouchPointStationary:
                {
                    QPointF currentPoint = touchPoint.pos();
                    p_TouchPoint = currentPoint.toPoint();
                }
                default:
                {
                    QPointF currentPoint = touchPoint.pos();
                    QRect currentView1=p_displayWidgets[LEFT_VIEW]->displayRect();
                    QRect currentView2=p_displayWidgets[RIGHT_VIEW]->displayRect();
                    currentView1.translate(currentPoint.toPoint()-p_TouchPoint);
                    currentView2.translate(currentPoint.toPoint()-p_TouchPoint);
                    p_TouchPoint=currentPoint.toPoint();
                    p_displayWidgets[LEFT_VIEW]->setDisplayRect(currentView1);
                    switch (viewMode_)
                    {
                    case SIDE_BY_SIDE:
                        p_displayWidgets[RIGHT_VIEW]->setDisplayRect(currentView2);
                        break;
                    case COMPARISON:
                        int widgetWidth1 = p_displayWidgets[LEFT_VIEW]->width();
                        currentView1.translate(-widgetWidth1,0);
                        p_displayWidgets[RIGHT_VIEW]->setDisplayRect(currentView1);
                        break;
                    }
                }
                    break;
            }
        }
        else if (touchPoints.count()==2)
        {
            const QTouchEvent::TouchPoint &touchPoint0 = touchPoints.first();
            const QTouchEvent::TouchPoint &touchPoint1 = touchPoints.last();
            qreal currentScaleFactor = QLineF(touchPoint0.pos(), touchPoint1.pos()).length()
                    / QLineF(touchPoint0.startPos(), touchPoint1.startPos()).length();

            if (touchEvent->touchPointStates() & Qt::TouchPointMoved)
            {
                if (currentScaleFactor>2.0*p_TouchScale)
                {
                    p_zoomFactor<<=1;
                    zoomIn(&p_TouchPoint);
                    p_TouchScale = 0.9*currentScaleFactor;
                }
                if (currentScaleFactor<=0.5*p_TouchScale)
                {
                    p_zoomFactor>>=1;
                    zoomOut(&p_TouchPoint);
                    p_TouchScale = 1.1*currentScaleFactor;

                }
            }
        }
        break;
    }
    case QEvent::TouchEnd:
    default:
        return QWidget::event(event);
    }
    return true;
}

void DisplaySplitWidget::zoomIn(QPoint* to)
{
    for (int i=0; i<NUM_VIEWS;i++)
    {
        if( p_displayWidgets[i]->isHidden() || p_displayWidgets[i]->displayObject() == NULL )
            continue;

        QSize imageSize = QSize( p_displayWidgets[i]->displayObject()->width(), p_displayWidgets[i]->displayObject()->height() );

        double currentZoomFactor = p_displayWidgets[i]->zoomFactor();
        double newZoomFactor = 1.0;

        if( currentZoomFactor < 1.0 )
            newZoomFactor = 1.0/( floor(1.0/currentZoomFactor - 1.0) );
        else
            newZoomFactor = currentZoomFactor + 1.0;

        QRect currentView = p_displayWidgets[i]->displayRect();

        int currentViewWidth = currentView.width();
        int currentViewHeight= currentView.height();

        int newViewHeight   = (float)imageSize.height()*newZoomFactor;
        int newViewWidth    = (float)imageSize.width()*newZoomFactor;

        int mouseOffsetX=0;
        int mouseOffsetY=0;
        float zoomMouseOffset;
        if (newZoomFactor <= 1.0)
        {
           zoomMouseOffset = 1.0/newZoomFactor;
        }
        else
        {
            zoomMouseOffset = currentZoomFactor;
        }
        QPoint TopLeft=currentView.topLeft();

        if (to)
        {
            mouseOffsetX = (TopLeft.x()-to->x())/zoomMouseOffset;
            mouseOffsetY = (TopLeft.y()-to->y())/zoomMouseOffset;
        }
        else
        {
           mouseOffsetX = -(newViewWidth-currentViewWidth)/2;
           mouseOffsetY = -(newViewHeight-currentViewHeight)/2;
        }
        currentView.setSize(QSize(newViewWidth,newViewHeight));
        currentView.translate(QPoint(mouseOffsetX,mouseOffsetY));
        p_displayWidgets[i]->setDisplayRect(currentView);

        // take special care in comparison mode
        if (viewMode_==COMPARISON)
        {
            currentView.translate(-p_displayWidgets[i%NUM_VIEWS]->width(),0);
            p_displayWidgets[(i+1)%NUM_VIEWS]->setDisplayRect(currentView);
            return;
        }
    }
}

void DisplaySplitWidget::zoomOut(QPoint* to)
{
    for (int i=0;i<NUM_VIEWS;i++)
    {
        if( p_displayWidgets[i]->isHidden() || p_displayWidgets[i]->displayObject() == NULL )
            continue;

        QSize imageSize = QSize( p_displayWidgets[i]->displayObject()->width(), p_displayWidgets[i]->displayObject()->height() );

        double currentZoomFactor = p_displayWidgets[i]->zoomFactor();
        double newZoomFactor = 1.0;

        if( currentZoomFactor < 2.0 )
            newZoomFactor = 1.0/( floor(1.0/currentZoomFactor + 1.0) );
        else
            newZoomFactor = currentZoomFactor - 1.0;

        QRect currentView = p_displayWidgets[i]->displayRect();

        int currentViewWidth = currentView.width();
        int currentViewHeight= currentView.height();

        int newViewHeight   = (float)imageSize.height()*newZoomFactor;
        int newViewWidth    = (float)imageSize.width()*newZoomFactor;

        int mouseOffsetX=0;
        int mouseOffsetY=0;

        currentView.setSize(QSize(newViewWidth,newViewHeight));
        QPoint TopLeft=currentView.topLeft();
        float zoomMouseOffset;
        if (newZoomFactor < 1.0)
        {
           zoomMouseOffset = 1.0/newZoomFactor;
        }
        else
        {
            zoomMouseOffset = currentZoomFactor;
        }
        if (to)
        {
            mouseOffsetX = (to->x()-TopLeft.x())/zoomMouseOffset;
            mouseOffsetY = (to->y()-TopLeft.y())/zoomMouseOffset;
        }
        else
        {
           mouseOffsetX = -(newViewWidth-currentViewWidth)/2;
           mouseOffsetY = -(newViewHeight-currentViewHeight)/2;
        }
        currentView.translate(QPoint(mouseOffsetX,mouseOffsetY));
        p_displayWidgets[i]->setDisplayRect(currentView);

        // take special care in comparison mode
        if (viewMode_==COMPARISON)
        {
            currentView.translate(-p_displayWidgets[i%NUM_VIEWS]->width(),0);
            p_displayWidgets[(i+1)%NUM_VIEWS]->setDisplayRect(currentView);
            return;
        }
    }

}
void DisplaySplitWidget::zoomToFit()
{
    switch (viewMode_)
    {
    case SIDE_BY_SIDE:
        for (int i=0;i<NUM_VIEWS;i++)
        {
            if (p_displayWidgets[i]->displayObject())
            {
                QSize imageSize = QSize( p_displayWidgets[i]->displayObject()->width(), p_displayWidgets[i]->displayObject()->height() );
                QSize widgetSize = p_displayWidgets[i]->size();

                float aspectView = (float)widgetSize.width()/(float)widgetSize.height();
                float aspectImage= (float)imageSize.width()/(float)imageSize.height();

                double zoomFactor = 1.0;

                if (aspectView > aspectImage)
                {
                    // scale to height
                    if(widgetSize.height()>imageSize.height())
                    {
                        zoomFactor   = floor((float)widgetSize.height()/(float)imageSize.height());
                    }
                    else
                    {
                        zoomFactor   = 1.0/ceil((float)imageSize.height()/(float)widgetSize.height());
                    }
                }
                else
                {
                    // scale to width
                    if(widgetSize.width()>imageSize.width())
                    {
                        zoomFactor   = floor((float)widgetSize.width()/(float)imageSize.width());
                    }
                    else
                    {
                        zoomFactor   = 1.0/ceil((float)imageSize.width()/(float)widgetSize.width());
                    }
                }

                int newHeight   = (float)imageSize.height()*zoomFactor;
                int newWidth    = (float)imageSize.width()*zoomFactor;

                QPoint topLeft((p_displayWidgets[i]->width()-newWidth)/2, (p_displayWidgets[i]->height()-newHeight)/2);

                QRect currentView(topLeft, QSize(newWidth, newHeight));
                p_displayWidgets[i]->setDisplayRect(currentView);
            }
        }
        break;
    case COMPARISON:
        if (p_displayWidgets[LEFT_VIEW]->displayObject() && p_displayWidgets[RIGHT_VIEW]->displayObject())
        {            
            QSize leftImageSize = QSize( p_displayWidgets[LEFT_VIEW]->displayObject()->width(), p_displayWidgets[LEFT_VIEW]->displayObject()->height() );
            QSize leftWidgetSize = p_displayWidgets[LEFT_VIEW]->size();

            float aspectView = (float)leftWidgetSize.width()/(float)leftWidgetSize.height();
            float aspectImage= (float)leftImageSize.width()/(float)leftImageSize.height();

            double zoomFactor = 1.0;

            if (aspectView > aspectImage)
            {
                // scale to height
                if(leftWidgetSize.height()>leftImageSize.height())
                {
                    zoomFactor   = floor((float)height()/(float)leftImageSize.height());
                }
                else
                {
                    zoomFactor   = 1.0/ceil((float)leftImageSize.height()/(float)height());
                }
            }
            else
            {
                // scale to width
                if(leftWidgetSize.width()>leftImageSize.width())
                {
                    zoomFactor   = floor((float)width()/(float)leftImageSize.width());
                }
                else
                {
                    zoomFactor   = 1.0/ceil((float)leftImageSize.width()/(float)width());
                }
            }

            int newHeight   = (float)leftImageSize.height()*zoomFactor;
            int newWidth    = (float)leftImageSize.width()*zoomFactor;

            QPoint topLeft((width()-newWidth)/2, (height()-newHeight)/2);

            QRect currentView(topLeft, QSize(newWidth, newHeight));
            p_displayWidgets[LEFT_VIEW]->setDisplayRect(currentView);

            // shift for right view
            currentView.translate(-leftWidgetSize.width(),0);
            p_displayWidgets[RIGHT_VIEW]->setDisplayRect(currentView);
        }
        break;
    }
}

void DisplaySplitWidget::zoomToStandard()
{
    resetViews();
    updateView();
}

void DisplaySplitWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        QWidget::dragEnterEvent(event);
}

void DisplaySplitWidget::dropEvent(QDropEvent *event)
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

                if( fi.isDir() || ext == "yuv" || ext == "yuvplaylist" || ext == "csv" )
                    fileList.append(fileName);
            }

            event->acceptProposedAction();

            mainWindow->loadFiles(fileList);
        }
    }
    QWidget::dropEvent(event);
}

void DisplaySplitWidget::mousePressEvent(QMouseEvent* e)
{
    switch (e->button()) {
    case Qt::LeftButton:
        // Start selection.
        p_selectionStartPoint = e->pos();

        // empty rect for now
        p_displayWidgets[LEFT_VIEW]->setSelectionRect(QRect());
        p_displayWidgets[RIGHT_VIEW]->setSelectionRect(QRect());

        selectionMode_ = SELECT;
        break;
    case Qt::MiddleButton:
        p_selectionStartPoint = e->pos();
        selectionMode_ = DRAG;
        break;
    default:
        QWidget::mousePressEvent(e);
    }
}

void DisplaySplitWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (p_zoomBoxEnabled)
    {
        p_displayWidgets[LEFT_VIEW]->setZoomBoxPoint(e->pos());
        QPoint rightViewPoint = e->pos();//-QPoint(p_displayWidgets[LEFT_VIEW]->width(),0);
        p_displayWidgets[RIGHT_VIEW]->setZoomBoxPoint(rightViewPoint);
    }

    switch (selectionMode_) {
    case SELECT:
    {
        // Updates rectangle_ coordinates and redraws rectangle
        p_selectionEndPoint = e->pos();

        QRect selectionRect = QRect(p_selectionStartPoint, p_selectionEndPoint);

        p_displayWidgets[LEFT_VIEW]->setSelectionRect(selectionRect);

        if(p_displayWidgets[RIGHT_VIEW]->isVisible())
        {
            QRect selectionRectSecundary = selectionRect;
            p_displayWidgets[RIGHT_VIEW]->setSelectionRect(selectionRectSecundary);
        }
        break;
    }
    case DRAG:
    {
        QRect currentView1=p_displayWidgets[LEFT_VIEW]->displayRect();
        QRect currentView2=p_displayWidgets[RIGHT_VIEW]->displayRect();
        currentView1.translate(e->pos()-p_selectionStartPoint);
        currentView2.translate(e->pos()-p_selectionStartPoint);
        p_selectionStartPoint=e->pos();
        p_displayWidgets[LEFT_VIEW]->setDisplayRect(currentView1);
        switch (viewMode_)
        {
        case SIDE_BY_SIDE:
            p_displayWidgets[RIGHT_VIEW]->setDisplayRect(currentView2);
            break;
        case COMPARISON:
            int widgetWidth1 = p_displayWidgets[LEFT_VIEW]->width();
            currentView1.translate(-widgetWidth1,0);
            p_displayWidgets[RIGHT_VIEW]->setDisplayRect(currentView1);
            break;
        }
        break;
    }
    default:
        QWidget::mouseMoveEvent(e);
    }
}

void DisplaySplitWidget::setSplitEnabled(bool enableSplit)
{
    p_displayWidgets[RIGHT_VIEW]->setVisible(enableSplit);
    if (enableSplit)
    {
        p_displayWidgets[LEFT_VIEW]->resize(width()/2,height());
        p_displayWidgets[RIGHT_VIEW]->resize(width()/2,height());
        p_LastSplitPos = width()/2;
        moveSplitter(p_LastSplitPos,1);
    }
    else
    {
        p_displayWidgets[LEFT_VIEW]->resize(width(),height());
    }
    refresh();
    updateView();
}

void DisplaySplitWidget::mouseReleaseEvent(QMouseEvent* e)
{
    switch (selectionMode_) {
    case SELECT:
    {
        QRect selectionRect = QRect(p_selectionStartPoint, p_selectionEndPoint);
        if( abs(selectionRect.width()) > 10 && abs(selectionRect.height()) > 10 )   // min selection size: 10x10
        {
            // TODO: compute display rectangles
        }
        else if (p_zoomFactor != 1)
        {
            zoomToStandard();
        }
        // when mouse is released, we don't draw the selection any longer
        selectionMode_ = NONE;
        p_displayWidgets[LEFT_VIEW]->setSelectionRect(QRect());
        p_displayWidgets[RIGHT_VIEW]->setSelectionRect(QRect());
        break;
    }
    case DRAG:
        selectionMode_ = NONE;
        break;
    default:
        QWidget::mouseReleaseEvent(e);
    }
}

void DisplaySplitWidget::wheelEvent (QWheelEvent *e)
{
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

void DisplaySplitWidget::resizeEvent(QResizeEvent*)
{
    p_LastSplitPos=p_displayWidgets[LEFT_VIEW]->width();
    refresh();
    updateView();
}

void DisplaySplitWidget::splitterMovedTo(int pos, int)
{
    if (p_LastSplitPos<0)
    {
        p_LastSplitPos=width()/2;
    }
    switch (viewMode_)
    {
    case SIDE_BY_SIDE:
    {
        QRect viewRefR = p_displayWidgets[RIGHT_VIEW]->displayRect();
        viewRefR.translate(p_LastSplitPos-pos,0);
        p_displayWidgets[RIGHT_VIEW]->setDisplayRect(viewRefR);
    }
        break;
    case COMPARISON:
    {
        if (p_displayWidgets[LEFT_VIEW]->displayObject()&&p_displayWidgets[RIGHT_VIEW]->displayObject())
        {
            // use left image as reference
            QRect ViewRef1 = p_displayWidgets[LEFT_VIEW]->displayRect();
            int widgetWidth1 = p_displayWidgets[LEFT_VIEW]->width();
            ViewRef1.translate(-widgetWidth1,0);
            p_displayWidgets[RIGHT_VIEW]->setDisplayRect(ViewRef1);
        }
    }

    }
    p_LastSplitPos=pos;
}

void DisplaySplitWidget::updateView()
{

    switch (viewMode_)
    {
    case SIDE_BY_SIDE:
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
            {
                int currentWidgetWidth = p_displayWidgets[i]->width();
                QRect currentView = p_displayWidgets[i]->displayRect();
                int offsetX = floor((currentWidgetWidth - currentView.width())/2.0);
                int offsetY = floor((height() - currentView.height())/2.0);
                QPoint topLeft(offsetX, offsetY);
                QPoint bottomRight(currentView.width()-1 + offsetX, currentView.height()-1 + offsetY);
                currentView.setTopLeft(topLeft);
                currentView.setBottomRight(bottomRight);
                p_displayWidgets[i]->setDisplayRect(currentView);
            }
        }
        break;
    case COMPARISON:

        if (p_displayWidgets[LEFT_VIEW]->displayObject()&&p_displayWidgets[RIGHT_VIEW]->displayObject())
        {
            // use left image as reference
            QRect ViewRef1 = p_displayWidgets[LEFT_VIEW]->displayRect();
            QRect ViewRef2 = p_displayWidgets[RIGHT_VIEW]->displayRect();

            int TotalWidth = width();
            int TotalHeight= height();
            int displayWidget1Width  = p_displayWidgets[LEFT_VIEW]->DisplayWidgetWidth();
            QPoint TopLeftWidget1((TotalWidth-ViewRef1.width())/2,(TotalHeight-ViewRef1.height())/2);
            QPoint BottomRightWidget1(((TotalWidth-ViewRef1.width())/2)+ViewRef1.width()-1,((TotalHeight-ViewRef1.height())/2)+ViewRef1.height()-1);
            QPoint TopLeftWidget2(TopLeftWidget1.x()-displayWidget1Width,TopLeftWidget1.y());
            QPoint BottomRightWidget2(TopLeftWidget2.x()+ViewRef1.width()-1,TopLeftWidget2.y()+ViewRef1.height()-1);
            ViewRef1.setTopLeft(TopLeftWidget1);
            ViewRef1.setBottomRight(BottomRightWidget1);
            ViewRef2.setTopLeft(TopLeftWidget2);
            ViewRef2.setBottomRight(BottomRightWidget2);
            p_displayWidgets[LEFT_VIEW]->setDisplayRect(ViewRef1);
            p_displayWidgets[RIGHT_VIEW]->setDisplayRect(ViewRef2);
        }
        break;
    }
}

