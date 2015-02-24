#include "playlisttreewidget.h"
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeData>
#include "playlistitem.h"

#include "mainwindow.h"

PlaylistTreeWidget::PlaylistTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setSortingEnabled(true);
}

void PlaylistTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QTreeWidget::dragMoveEvent(event);


}

// TODO: still not working correctly
void PlaylistTreeWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
    else    // default behavior
    {
        QModelIndex dropIndex = indexAt(event->pos());
        PlaylistItem* dropItem = dynamic_cast<PlaylistItem*>(itemAt(event->pos()));

        QRect targetRect = visualRect(dropIndex);
        QPoint dropPos = event->pos();

        if( targetRect.adjusted(+1,+1,-1,-1).contains(dropPos) == false )
        {
            event->acceptProposedAction();
            return;
        }

        if( dropIndex.row() == -1 )
            return;

        QList<QTreeWidgetItem*> activeItems = selectedItems();
        PlaylistItem* draggedItem = dynamic_cast<PlaylistItem*>( activeItems.at(0) );

        QTreeWidget::DropIndicatorPosition dropIndPosition = dropIndicatorPosition();

        bool acceptAction = false;
        switch (dropIndPosition)
        {
        case QAbstractItemView::OnItem:
            acceptAction = ( dropItem && dropItem->itemType() == VideoItemType && dropItem->childCount() == 0 && activeItems.count() == 1 && draggedItem->itemType() == StatisticsItemType );
            break;
        default:
            acceptAction = true;
            break;
        }

        // final decision
        if (acceptAction)
        {
            //event->accept();
            QTreeWidget::dragEnterEvent(event);
        }
        else
        {
            event->ignore();
        }


    }
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
//    {
//        QModelIndex droppedIndex = indexAt( event->pos() );
//        if( !droppedIndex.isValid() )
//            return;

//        QTreeWidget::dropEvent(event);

//        DropIndicatorPosition dp = dropIndicatorPosition();
//        if (dp == QAbstractItemView::BelowItem) {
//            droppedIndex = droppedIndex.sibling(droppedIndex.row() + 1, // adjust the row number
//                                                droppedIndex.column());
//        }
//        selectionModel()->select(droppedIndex, QItemSelectionModel::Select);
//    }
}

Qt::DropActions PlaylistTreeWidget::supportedDropActions () const
{
    return Qt::CopyAction | Qt::MoveAction;
}
