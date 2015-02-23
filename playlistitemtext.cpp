#include "playlistitemtext.h"

PlaylistItemText::PlaylistItemText(const QString &srcFileName, QTreeWidget* parent) : PlaylistItem ( srcFileName, parent )
{
    // create new object for this video file
    p_displayObject = new TextObject(srcFileName);

    // update item name to short name
    //setText(0, p_displayObject->name());

    // update icon
    setIcon(0, QIcon(":images/img_television.png"));

    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlaylistItemText::~PlaylistItemText()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemText::itemType()
{
    return TextItemType;
}
