#include "playlistitemstats.h"

PlayListItemStats::PlayListItemStats(const QString &srcFileName, QTreeWidget* parent) : PlayListItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = NULL;

    // update item name to short name
    //setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlayListItemStats::~PlayListItemStats()
{
    delete p_displayObject;
}

PlayListItemType PlayListItemStats::itemType()
{
    return StatisticsItem;
}
