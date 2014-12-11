#include "playlisttreewidget.h"
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>

#include "mainwindow.h"

PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) :
    QTreeWidget(parent)
{
}

/*
void PlaylistTreeWidget::dragEnterEvent(QDragEnterEvent *event)
 {
     if (event->mimeData()->hasUrls())
         event->acceptProposedAction();
     else
         QTreeWidget::dragEnterEvent(event);
 }

void PlaylistTreeWidget::dropEvent(QDropEvent *event)
 {
    if (event->mimeData()->hasUrls())
    {
        QStringList fileList;
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty())
        {
            QUrl url;
            foreach (url, urls)
            {
                QString fileName = url.toLocalFile();

                fileList.append(fileName);
            }
        }
        event->acceptProposedAction();

        // use our main window to open this file
        MainWindow* mainWindow = (MainWindow*)this->window();
        mainWindow->loadFiles(fileList);
    }
    else
        QTreeWidget::dropEvent(event);
 }

QStringList PlaylistTreeWidget::mimeTypes () const
{
   return model()->QAbstractItemModel::mimeTypes();
}

bool PlaylistTreeWidget::dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action)
{
   QModelIndex idx;
   if (parent) idx = indexFromItem(parent);

    return model()->QAbstractItemModel::dropMimeData(data, action , index, 0, idx);
}

Qt::DropActions PlaylistTreeWidget::supportedDropActions () const
{
   return Qt::CopyAction | Qt::MoveAction;
}
*/
