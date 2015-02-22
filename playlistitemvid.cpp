#include "playlistitemvid.h"
#include "frameobject.h"

PlaylistItemVid::PlaylistItemVid(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new FrameObject(srcFileName);

    // update item name to short name
    setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlaylistItemVid::~PlaylistItemVid()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemVid::itemType()
{
    return VideoItem;
}
