#ifndef PLAYLISTITEMSTATS_H
#define PLAYLISTITEMSTATS_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlayListItemStats : public PlayListItem
{
public:
    PlayListItemStats(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlayListItemStats();

    PlayListItemType itemType();

private:

};

#endif // PLAYLISTITEMSTATS_H
