#ifndef PLAYLISTTREEWIDGET_H
#define PLAYLISTTREEWIDGET_H

#include <QTreeWidget>
#include "QMouseEvent"

class PlaylistTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit PlaylistTreeWidget(QWidget *parent = 0);

    /*
    QStringList mimeTypes() const;
    bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data, Qt::DropAction action);

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    Qt::DropActions supportedDropActions() const;
    */

    QTreeWidgetItem* getItemFromIndex( const QModelIndex &index ) { return itemFromIndex(index); }
signals:
    
public slots:

private:
    /*
    virtual void mousePressEvent(QMouseEvent *event)
    {
        QModelIndex item = indexAt(event->pos());
        QTreeView::mousePressEvent(event);
        if (item.row() == -1 && item.column() == -1)
        {
            clearSelection();
            const QModelIndex index;
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
        }
    }
    */
};

#endif // PLAYLISTTREEWIDGET_H
