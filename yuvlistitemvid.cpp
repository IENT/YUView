#include "yuvlistitemvid.h"

YUVListItemVid::YUVListItemVid(const QString &srcFileName, QTreeWidget* parent) : YUVListItem ( srcFileName, parent )
{
    // create new object for this video file
    p_renderObject = new YUVObject(srcFileName);

    // update item name to short name
    setText(0, p_renderObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

YUVListItemVid::YUVListItemVid(const QString &srcFileName, YUVListItem* parentItem) : YUVListItem ( srcFileName, parentItem )
{
    // create new object for this video file
    p_renderObject = new YUVObject(srcFileName);

    // update item name to short name
    setText(0, p_renderObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

YUVListItemVid::~YUVListItemVid()
{
    delete p_renderObject;
}

YUVObject* YUVListItemVid::renderObject()
{
    return p_renderObject;
}


YUVListItemType YUVListItemVid::itemType()
{
    return VideoItem;
}
