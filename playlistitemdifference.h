#ifndef PLAYLISTITEMDIFFERENCE_H
#define PLAYLISTITEMDIFFERENCE_H

#include <QObject>
#include "playlistitem.h"
#include "differenceobject.h"

class PlaylistItemDifference : public PlaylistItem
{
public:
    PlaylistItemDifference(const QString &itemName, QTreeWidget* parent = NULL);
    ~PlaylistItemDifference();

    PlaylistItemType itemType();

    DifferenceObject *displayObject() { return dynamic_cast<DifferenceObject*>(p_displayObject); }
};

#endif // PLAYLISTITEMDIFFERENCE_H
