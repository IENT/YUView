#include "playlistitemstats.h"

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = NULL;

    // update item name to short name
    //setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlaylistItemStats::~PlaylistItemStats()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemStats::itemType()
{
    return StatisticsItem;
}
