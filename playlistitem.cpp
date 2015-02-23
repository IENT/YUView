#include "playlistitem.h"

#include "playlistitemvid.h"

PlaylistItem::PlaylistItem(const QString &itemName, QTreeWidget * parent) : QTreeWidgetItem ( parent, 1001 )
{
    p_displayObject = NULL;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon(":images/img_folder.png"));

    // make sure drag&drop is possible for group items
    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlaylistItem::~PlaylistItem() {

}
