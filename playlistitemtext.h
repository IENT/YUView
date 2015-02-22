#ifndef PLAYLISTITEMTEXT_H
#define PLAYLISTITEMTEXT_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlaylistItemText : public PlaylistItem
{
public:
    PlaylistItemText(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlaylistItemText();

    PlaylistItemType itemType();

private:

};

#endif // PLAYLISTITEMTEXT_H
