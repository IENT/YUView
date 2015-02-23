#include "playlisttreewidget.h"
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>

#include "mainwindow.h"

PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSortingEnabled(true);
}

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
    {
        QModelIndex droppedIndex = indexAt( event->pos() );
        if( !droppedIndex.isValid() )
            return;

        QTreeWidget::dropEvent(event);

        DropIndicatorPosition dp = dropIndicatorPosition();
        if (dp == QAbstractItemView::BelowItem) {
            droppedIndex = droppedIndex.sibling(droppedIndex.row() + 1, // adjust the row number
                                                droppedIndex.column());
        }
        selectionModel()->select(droppedIndex, QItemSelectionModel::Select);
    }
}

Qt::DropActions PlaylistTreeWidget::supportedDropActions () const
{
    return Qt::CopyAction | Qt::MoveAction;
}
