#ifndef PLAYLISTITEMVID_H
#define PLAYLISTITEMVID_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlayListItemVid : public PlayListItem
{
public:
    PlayListItemVid(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlayListItemVid();

    PlayListItemType itemType();

private:

};

#endif // PLAYLISTITEMVID_H
