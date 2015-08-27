/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

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
    void setIsSaved(bool isSaved) {p_isSaved = isSaved;}
    bool getIsSaved() { return p_isSaved;}

    Qt::DropActions supportedDropActions() const;

    QModelIndex indexForItem(PlaylistItem * item) { return indexFromItem((QTreeWidgetItem*)item); }

signals:
    void playListKey(QKeyEvent* key);
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
    bool p_isSaved;
    virtual void keyPressEvent(QKeyEvent* event)
    {
           emit playListKey(event);
    }
};

#endif // PLAYLISTTREEWIDGET_H
