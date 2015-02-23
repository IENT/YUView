#include "playlistitemstats.h"

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new StatisticsObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/stats.png"));

    // enable dragging
    setFlags(flags() | Qt::ItemIsDragEnabled);
}

PlaylistItemStats::~PlaylistItemStats()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemStats::itemType()
{
    return StatisticsItemType;
}
