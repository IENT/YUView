#include "displaysplitwidget.h"

#include <QMimeData>
#include "mainwindow.h"
#define _USE_MATH_DEFINES
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define ZOOMBOX_MIN_FACTOR 5
#define ZOOMBOX_MAX_FACTOR 20

DisplaySplitWidget::DisplaySplitWidget(QWidget *parent) : QSplitter(parent)
{
    for(int i=0; i<NUM_VIEWS; i++)
    {
        p_displayWidgets[i] = new DisplayWidget(this);
        this->addWidget(p_displayWidgets[i]);
    }

    setAcceptDrops(true);

    selectionMode_ = NONE;
    viewMode_ = SIDE_BY_SIDE;

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

void DisplaySplitWidget::setActiveDisplayObjects( DisplayObject* newPrimaryDisplayObject, DisplayObject* newSecondaryDisplayObject )
{
    p_displayWidgets[0]->setDisplayObject(newPrimaryDisplayObject);
    p_displayWidgets[1]->setDisplayObject(newSecondaryDisplayObject);
}

void DisplaySplitWidget::setActiveStatisticsObjects(StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject)
{
    p_displayWidgets[0]->setOverlayStatisticsObject(newPrimaryStatisticsObject);
    p_displayWidgets[1]->setOverlayStatisticsObject(newSecondaryStatisticsObject);
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

void DisplaySplitWidget::zoomIn(QPoint* to)
{
    int widthOffset=0;
    int heightOffset=0;

    QRect currentView1 = p_displayWidgets[0]->displayRect();

    int currentViewWidth1 = currentView1.width();
    int currentViewHeight1= currentView1.height();

    int newViewWidth1 = currentViewWidth1 * 2;
    int newViewHeight1 = currentViewHeight1 * 2;

    if (to->x()==0 && to->y()==0)
    {
        widthOffset = (width()-newViewWidth1)/2;
        heightOffset = (height()-newViewHeight1)/2;
    }
    else {
        widthOffset = -(newViewWidth1-currentViewWidth1)/2;
        heightOffset = -(newViewHeight1-currentViewHeight1)/2;
    }
    QPoint currentCenter=currentView1.center();
    int mouseOffsetX = currentCenter.x()-to->x();
    int mouseOffsetY = currentCenter.y()-to->y();
    currentView1.setSize(QSize(newViewWidth1,newViewHeight1));
    currentView1.translate(QPoint(widthOffset+mouseOffsetX,heightOffset+mouseOffsetY));
    p_displayWidgets[0]->setDisplayRect(currentView1);
}

void DisplaySplitWidget::zoomOut(QPoint* to)
{
    int widthOffset=0;
    int heightOffset=0;

    QRect currentView1 = p_displayWidgets[0]->displayRect();

    int currentViewWidth1 = currentView1.width();
    int currentViewHeight1= currentView1.height();

    int newViewWidth1 = currentViewWidth1 / 2;
    int newViewHeight1 = currentViewHeight1 / 2;

    if (to->x()==0 && to->y()==0)
    {
        widthOffset = (width()-newViewWidth1)/2;
        heightOffset = (height()-newViewHeight1)/2;
    }
    else {
        widthOffset = -(newViewWidth1-currentViewWidth1)/2;
        heightOffset = -(newViewHeight1-currentViewHeight1)/2;
    }
    QPoint currentCenter=currentView1.center();
    int mouseOffsetX = (currentCenter.x()-to->x())/2;
    int mouseOffsetY = (currentCenter.y()-to->y())/2;
    currentView1.setSize(QSize(newViewWidth1,newViewHeight1));
    currentView1.translate(QPoint(widthOffset-mouseOffsetX,heightOffset-mouseOffsetY));
    p_displayWidgets[0]->setDisplayRect(currentView1);}

void DisplaySplitWidget::zoomToFit()
{
    p_zoomFactor = 1;
}

void DisplaySplitWidget::zoomToStandard()
{
    p_zoomFactor = 1;
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
        p_displayWidgets[0]->setSelectionRect(QRect());
        p_displayWidgets[1]->setSelectionRect(QRect());

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
    if (p_zoomBoxEnabled) {
        setCurrentMousePosition(e->pos());
    }

    switch (selectionMode_) {
    case SELECT:
    {
        // Updates rectangle_ coordinates and redraws rectangle
        p_selectionEndPoint = e->pos();

        QRect selectionRect = QRect(p_selectionStartPoint, p_selectionEndPoint);

        p_displayWidgets[0]->setSelectionRect(selectionRect);

        if(p_displayWidgets[0]->isVisible())
        {
            QRect selectionRectSecundary = selectionRect;
            p_displayWidgets[1]->setSelectionRect(selectionRectSecundary);
        }
        break;
    }
    case DRAG:
    {
        // TODO: Make a loop or something more general
        // TODO: Maybe make two modes out of this?
        QRect currentView1=p_displayWidgets[0]->displayRect();
        QRect currentView2=p_displayWidgets[1]->displayRect();
        currentView1.translate(e->pos()-p_selectionStartPoint);
        currentView2.translate(e->pos()-p_selectionStartPoint);
        p_selectionStartPoint=e->pos();
        p_displayWidgets[0]->setDisplayRect(currentView1);
        p_displayWidgets[1]->setDisplayRect(currentView1);
        break;
    }
    default:
        QWidget::mouseMoveEvent(e);
    }
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
        p_displayWidgets[0]->setSelectionRect(QRect());
        p_displayWidgets[1]->setSelectionRect(QRect());
        break;
    }
    case DRAG:
        selectionMode_ = NONE;
        break;
    default:
        QWidget::mouseReleaseEvent(e);
    }
}

void DisplaySplitWidget::wheelEvent (QWheelEvent *e) {
    //TODO: Zooming right now only performed with factor 2
    // because I'm lazy :-)
    // TODO: Synchronize Zoom between the Widgets and / or
    // make two modes out of this
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

void DisplaySplitWidget::splitterMovedTo(int pos, int index)
{
    updateView();
}

void DisplaySplitWidget::updateView()
{
    switch (viewMode_)
    {
    case STANDARD:
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
                p_displayWidgets[i]->centerView(i);
        }
        break;
    case SIDE_BY_SIDE:
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
                p_displayWidgets[i]->centerView();
        }
        break;
    case COMPARISON:

        if (p_displayWidgets[0]->displayObject()&&p_displayWidgets[1]->displayObject())
        {
        // use left image as reference
        QPixmap imageRef1 = p_displayWidgets[0]->displayObject()->displayImage();
        QPixmap imageRef2 = p_displayWidgets[0]->displayObject()->displayImage();

        int TotalWidth = width();
        int TotalHeight= height();
        int displayWidget1Width  = p_displayWidgets[0]->DisplayWidgetWidth();

        QRect QRectWidget1(QPoint((TotalWidth-imageRef1.width())/2,((TotalHeight-imageRef1.height())/2)),QPoint((TotalWidth-imageRef1.width())/2+imageRef1.width(),((TotalHeight-imageRef1.height())/2+imageRef1.height())));
        QRect QRectWidget2(QPoint((TotalWidth-imageRef1.width())/2-displayWidget1Width,((TotalHeight-imageRef1.height())/2)),QPoint((TotalWidth-imageRef1.width())/2+imageRef2.width()-displayWidget1Width,((TotalHeight-imageRef2.height())/2+imageRef2.height())));

        p_displayWidgets[0]->setDisplayRect(QRectWidget1);
        p_displayWidgets[1]->setDisplayRect(QRectWidget2);
        }
        break;
     }
}

