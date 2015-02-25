#include "playlistitemstats.h"

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new StatisticsObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/stats.png"));

    // disable dropping on statistic items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemStats::PlaylistItemStats(const QString &srcFileName, QTreeWidgetItem* parentItem) : PlaylistItem( srcFileName, parentItem )
{
    // create new object for this video file
    p_displayObject = new StatisticsObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/stats.png"));

    // enable dragging
    setFlags(flags() | Qt::ItemIsDragEnabled);

    // disable dropping on statistic items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemStats::~PlaylistItemStats()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemStats::itemType()
{
    return StatisticsItemType;
}
