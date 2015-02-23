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
