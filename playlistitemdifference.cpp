#include "playlistitemdifference.h"

PlaylistItemDifference::PlaylistItemDifference(const QString &itemName, QTreeWidget* parent) : PlaylistItem ( itemName, parent )
{
    // create new object for this video file
    p_displayObject = new DifferenceObject();

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    // enable dropping
    setFlags(flags() | Qt::ItemIsDropEnabled);
    }

PlaylistItemDifference::~PlaylistItemDifference()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemDifference::itemType()
{
    return DifferenceItemType;
}
