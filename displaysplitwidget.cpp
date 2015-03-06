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

void DisplaySplitWidget::resetViews()
{
    for(int i=0; i<NUM_VIEWS; i++)
    {
        if(p_displayWidgets[i]->displayObject())
            p_displayWidgets[i]->resetView();
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
    for (int i=0; i<NUM_VIEWS;i++)
    {
    QRect currentView1 = p_displayWidgets[i]->displayRect();

    int currentViewWidth1 = currentView1.width();
    int currentViewHeight1= currentView1.height();

    int newViewWidth1 = currentViewWidth1 * 2;
    int newViewHeight1 = currentViewHeight1 * 2;
    QPoint currentCenter=currentView1.center();
    int mouseOffsetX=0;
    int mouseOffsetY=0;
    widthOffset = -(newViewWidth1-currentViewWidth1)/2;
    heightOffset = -(newViewHeight1-currentViewHeight1)/2;
    if (to)
    {
        mouseOffsetX = currentCenter.x()-to->x();
        mouseOffsetY = currentCenter.y()-to->y();
    }

    currentView1.setSize(QSize(newViewWidth1,newViewHeight1));
    currentView1.translate(QPoint(widthOffset+mouseOffsetX,heightOffset+mouseOffsetY));
    p_displayWidgets[i]->setDisplayRect(currentView1);
    if (viewMode_==COMPARISON)
    {
        currentView1.translate(-p_displayWidgets[i%NUM_VIEWS]->width(),0);
        p_displayWidgets[(i+1)%NUM_VIEWS]->setDisplayRect(currentView1);
        return;
    }
    }
}

void DisplaySplitWidget::zoomOut(QPoint* to)
{
    int widthOffset=0;
    int heightOffset=0;
    for (int i=0;i<NUM_VIEWS;i++)
    {
    QRect currentView1 = p_displayWidgets[i]->displayRect();

    int currentViewWidth1 = currentView1.width();
    int currentViewHeight1= currentView1.height();

    int newViewWidth1 = currentViewWidth1 / 2;
    int newViewHeight1 = currentViewHeight1 / 2;

    QPoint currentCenter=currentView1.center();
    int mouseOffsetX=0;
    int mouseOffsetY=0;
    widthOffset = -(newViewWidth1-currentViewWidth1)/2;
    heightOffset = -(newViewHeight1-currentViewHeight1)/2;
    if (to)
    {
        mouseOffsetX = (currentCenter.x()-to->x())/2;
        mouseOffsetY = (currentCenter.y()-to->y())/2;
    }
    currentView1.setSize(QSize(newViewWidth1,newViewHeight1));
    currentView1.translate(QPoint(widthOffset-mouseOffsetX,heightOffset-mouseOffsetY));
    p_displayWidgets[i]->setDisplayRect(currentView1);
    if (viewMode_==COMPARISON)
    {

        currentView1.translate(-p_displayWidgets[i%NUM_VIEWS]->width(),0);
        p_displayWidgets[(i+1)%NUM_VIEWS]->setDisplayRect(currentView1);
        return;
    }
    }

}
void DisplaySplitWidget::zoomToFit()
{
  switch (viewMode_)
  {
  case STANDARD:
  case SIDE_BY_SIDE:
      for (int i=0;i<NUM_VIEWS;i++)
      {
          if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
        {
          int newHeight;
          int newWidth;
          QPoint topLeft;
          QPixmap currentImage=p_displayWidgets[i]->displayObject()->displayImage();
          float aspectView = (float)p_displayWidgets[i]->width()/(float)p_displayWidgets[i]->height();
          float aspectImage= (float)currentImage.width()/(float)currentImage.height();
          if (aspectView>aspectImage)
          { // scale to height
              newHeight = p_displayWidgets[i]->height();
              newWidth = round(((float)newHeight/(float)currentImage.height())*(float)currentImage.width());
              topLeft.setX((p_displayWidgets[i]->width()-newWidth)/2);
              topLeft.setY(0);
          }
          else
          { // scale to width
            newWidth =  p_displayWidgets[i]->width();
            newHeight = round(((float)newWidth/(float)currentImage.width())*(float)currentImage.height());
            topLeft.setX(0);
            topLeft.setY((p_displayWidgets[i]->height()-newHeight)/2);
          }
          QPoint bottomRight(topLeft.x()+newWidth,topLeft.y()+newHeight);
          QRect currentView(topLeft,bottomRight);
          p_displayWidgets[i]->setDisplayRect(currentView);
          }
      }
      break;
  case COMPARISON:
      if (p_displayWidgets[0]->displayObject()&&p_displayWidgets[1]->displayObject())
    {
          int newHeight;
          int newWidth;
          QPoint topLeft;
          int leftWidth = p_displayWidgets[0]->width();
          QPixmap currentImage=p_displayWidgets[0]->displayObject()->displayImage();
          float aspectView = (float)p_displayWidgets[0]->width()/(float)p_displayWidgets[0]->height();
          float aspectImage= (float)currentImage.width()/(float)currentImage.height();
          if (aspectView>aspectImage)
          { // scale to height
              newHeight = height();
              newWidth = round(((float)newHeight/(float)currentImage.height())*(float)currentImage.width());
              topLeft.setX((p_displayWidgets[0]->width()-newWidth)/2);
              topLeft.setY(0);
          }
          else
          { // scale to width
            newWidth =  width();
            newHeight = round(((float)newWidth/(float)currentImage.width())*(float)currentImage.height());
            topLeft.setX(0);
            topLeft.setY((p_displayWidgets[0]->height()-newHeight)/2);
          }
      QPoint bottomRight(topLeft.x()+newWidth,topLeft.y()+newHeight);
      QRect currentView(topLeft,bottomRight);
      p_displayWidgets[0]->setDisplayRect(currentView);
      currentView.translate(-leftWidth,0);
      p_displayWidgets[1]->setDisplayRect(currentView);
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
        switch (viewMode_)
        {
            case STANDARD:
            case SIDE_BY_SIDE:
                p_displayWidgets[1]->setDisplayRect(currentView2);
                break;
            case COMPARISON:
                int widgetWidth1 = p_displayWidgets[0]->width();
                currentView1.translate(-widgetWidth1,0);
                p_displayWidgets[1]->setDisplayRect(currentView1);
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
  p_displayWidgets[1]->setVisible(enableSplit);
  if (enableSplit)
  {
      widget(0)->resize(width()/2,height());
      widget(1)->resize(width()/2,height());
      updateView();
      moveSplitter(width()/2,1);
  }
  else
  {
      widget(0)->resize(width(),height());
      updateView();
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

    switch (viewMode_)
    {
    case STANDARD:
    case SIDE_BY_SIDE:
        // TODO: this is still really, really buggy and needs rework
        // TODO: reimplement this stuff
        updateView();
        break;
    case COMPARISON:
    {
        if (p_displayWidgets[0]->displayObject()&&p_displayWidgets[1]->displayObject())
        {
        // use left image as reference
        QRect ViewRef1 = p_displayWidgets[0]->displayRect();
        int widgetWidth1 = p_displayWidgets[0]->width();
        ViewRef1.translate(-widgetWidth1,0);
        p_displayWidgets[1]->setDisplayRect(ViewRef1);
        }
    }

    }
}

void DisplaySplitWidget::updateView()
{

    switch (viewMode_)
    {
    case STANDARD:
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
            {
                p_displayWidgets[i]->centerView(i);
            }
        }
        break;
    case SIDE_BY_SIDE:
        for( int i=0; i<NUM_VIEWS; i++ )
        {
            if (p_displayWidgets[i]->isVisible() && p_displayWidgets[i]->displayObject())
            {
                p_displayWidgets[i]->centerView();

            }
        }
        break;
    case COMPARISON:

        if (p_displayWidgets[0]->displayObject()&&p_displayWidgets[1]->displayObject())
        {
        // use left image as reference
        QRect ViewRef1 = p_displayWidgets[0]->displayRect();
        QRect ViewRef2 = p_displayWidgets[1]->displayRect();

        int TotalWidth = width();
        int TotalHeight= height();
        int displayWidget1Width  = p_displayWidgets[0]->DisplayWidgetWidth();
        // all clear? ;-)
        QPoint TopLeftWidget1((TotalWidth-ViewRef1.width())/2,(TotalHeight-ViewRef1.height())/2);
        QPoint BottomRightWidget1(((TotalWidth-ViewRef1.width())/2)+ViewRef1.width()-1,((TotalHeight-ViewRef1.height())/2)+ViewRef1.height()-1);
        QPoint TopLeftWidget2(TopLeftWidget1.x()-displayWidget1Width,TopLeftWidget1.y());
        QPoint BottomRightWidget2(TopLeftWidget2.x()+ViewRef1.width()-1,TopLeftWidget2.y()+ViewRef1.height()-1);
        ViewRef1.setTopLeft(TopLeftWidget1);
        ViewRef1.setBottomRight(BottomRightWidget1);
        ViewRef2.setTopLeft(TopLeftWidget2);
        ViewRef2.setBottomRight(BottomRightWidget2);
        p_displayWidgets[0]->setDisplayRect(ViewRef1);
        p_displayWidgets[1]->setDisplayRect(ViewRef2);
        }
        break;
     }
}

