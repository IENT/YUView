#include "statslistview.h"
#include "mainwindow.h"

#include <QUrl>
#include <QMimeData>

StatsListView::StatsListView(QWidget *parent) :
    QListView(parent)
{
}

void StatsListView::dragEnterEvent(QDragEnterEvent *event)
 {
    MainWindow* mainWindow = (MainWindow*)this->window();

     if (mainWindow->isPlaylistItemSelected() && event->mimeData()->hasUrls())
         event->acceptProposedAction();
     else
         QListView::dragEnterEvent(event);
 }

void StatsListView::dropEvent(QDropEvent *event)
 {
    MainWindow* mainWindow = (MainWindow*)this->window();

    if (mainWindow->isPlaylistItemSelected() && event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QUrl url;
            foreach (url, urls)
            {
                QString fileName = url.toLocalFile();

                // use our main window to open this file
                mainWindow->loadStatsFile(fileName);
            }
        }
        event->acceptProposedAction();
    }
    else
        QListView::dropEvent(event);
 }
