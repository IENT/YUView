#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include "displayobject.h"
#include "statisticsobject.h"

enum PlaylistItemType {
    VideoItemType,
    TextItemType,
    StatisticsItemType
};

class PlaylistItem : public QTreeWidgetItem
{
public:
    PlaylistItem(const QString &itemName, QTreeWidget* parent = 0);
    PlaylistItem(const QString &itemName, QTreeWidgetItem* parentItem);

    ~PlaylistItem();

    virtual DisplayObject *displayObject() { return p_displayObject; }

    virtual PlaylistItemType itemType() = 0;

public slots:

private:

protected:
    DisplayObject* p_displayObject;
};

#endif // PLAYLISTITEM_H
