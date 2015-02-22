#ifndef PLAYLISTITEMSTATS_H
#define PLAYLISTITEMSTATS_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlaylistItemStats : public PlaylistItem
{
public:
    PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlaylistItemStats();

    PlaylistItemType itemType();

private:

};

#endif // PLAYLISTITEMSTATS_H
