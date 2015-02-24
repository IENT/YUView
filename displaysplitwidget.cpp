#include "displaysplitwidget.h"

#include <QMimeData>
#include "mainwindow.h"

DisplaySplitWidget::DisplaySplitWidget(QWidget *parent) : QSplitter(parent)
{
    p_primaryDisplayWidget = new DisplayWidget(this);
    p_secondaryDisplayWidget = new DisplayWidget(this);

    this->addWidget(p_primaryDisplayWidget);
    this->addWidget(p_secondaryDisplayWidget);

    setAcceptDrops(true);
}

DisplaySplitWidget::~DisplaySplitWidget()
{
    delete p_primaryDisplayWidget;
    delete p_secondaryDisplayWidget;
}

void DisplaySplitWidget::setActiveDisplayObjects( DisplayObject* newPrimaryDisplayObject, DisplayObject* newSecondaryDisplayObject )
{
    p_primaryDisplayWidget->setDisplayObject(newPrimaryDisplayObject);
    p_secondaryDisplayWidget->setDisplayObject(newSecondaryDisplayObject);
}

void DisplaySplitWidget::setActiveStatisticsObjects(StatisticsObject* newPrimaryStatisticsObject, StatisticsObject* newSecondaryStatisticsObject)
{
    p_primaryDisplayWidget->setOverlayStatisticsObject(newPrimaryStatisticsObject);
    p_secondaryDisplayWidget->setOverlayStatisticsObject(newSecondaryStatisticsObject);
}

// triggered from timer in application
void DisplaySplitWidget::drawFrame(unsigned int frameIdx)
{
    // propagate the draw request to worker widgets
    p_primaryDisplayWidget->drawFrame(frameIdx);
    p_secondaryDisplayWidget->drawFrame(frameIdx);
}

void DisplaySplitWidget::clear()
{
    p_primaryDisplayWidget->clear();
    p_secondaryDisplayWidget->clear();
}

void DisplaySplitWidget::setRegularGridParameters(bool show, int size, unsigned char color[])
{
    p_primaryDisplayWidget->setRegularGridParameters(show, size, color);
    p_secondaryDisplayWidget->setRegularGridParameters(show, size, color);
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

                if( fi.isDir() || ext == "yuv" || ext == "xml" || ext == "csv" )
                    fileList.append(fileName);
            }

            event->acceptProposedAction();

            mainWindow->loadFiles(fileList);
        }
    }
    QWidget::dropEvent(event);
}
