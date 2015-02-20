#include "playlistitemvid.h"
#include "frameobject.h"

PlayListItemVid::PlayListItemVid(const QString &srcFileName, QTreeWidget* parent) : PlayListItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new FrameObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlayListItemVid::~PlayListItemVid()
{
    delete p_displayObject;
}

PlayListItemType PlayListItemVid::itemType()
{
    return VideoItem;
}
