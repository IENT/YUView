#ifndef PLAYLISTTREEWIDGET_H
#define PLAYLISTTREEWIDGET_H

#include <QTreeWidget>
#include "QMouseEvent"

class PlaylistItem;

class PlaylistTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit PlaylistTreeWidget(QWidget *parent = 0);

    void dragMoveEvent(QDragMoveEvent* event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    Qt::DropActions supportedDropActions() const;

signals:
    
public slots:

private:
    PlaylistItem* getDropTarget(QPoint pos);

    virtual void mousePressEvent(QMouseEvent *event)
    {
        QModelIndex item = indexAt(event->pos());
        QTreeView::mousePressEvent(event);
        if (item.row() == -1 && item.column() == -1)
        {
            clearSelection();
            const QModelIndex index;
            emit currentItemChanged(NULL, NULL);
            //selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
        }
    }
};

#endif // PLAYLISTTREEWIDGET_H
