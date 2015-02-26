#include "playlistitemtext.h"

PlaylistItemText::PlaylistItemText(const QString &text, QFont font, double duration, QTreeWidget* parent) : PlaylistItem ( text, parent )
{
    // create new object for this video file
    p_displayObject = new TextObject(text,font,duration);

    // update icon
    setIcon(0, QIcon(":images/img_text.png"));

    // disable dropping on text items
    setFlags(flags() & ~Qt::ItemIsDropEnabled);
}

PlaylistItemText::~PlaylistItemText()
{
    delete p_displayObject;
}

PlaylistItemType PlaylistItemText::itemType()
{
    return TextItemType;
}
