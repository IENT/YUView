#ifndef PLAYLISTITEMVID_H
#define PLAYLISTITEMVID_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlaylistItemVid : public PlaylistItem
{
public:
    PlaylistItemVid(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlaylistItemVid();

    PlaylistItemType itemType();

    FrameObject *displayObject() { return dynamic_cast<FrameObject*>(p_displayObject); }

private:

};

#endif // PLAYLISTITEMVID_H
