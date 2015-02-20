#ifndef PLAYLISTITEMTEXT_H
#define PLAYLISTITEMTEXT_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "frameobject.h"

class PlayListItemText : public PlayListItem
{
public:
    PlayListItemText(const QString &srcFileName, QTreeWidget* parent = NULL);

    ~PlayListItemText();

    PlayListItemType itemType();

private:

};

#endif // PLAYLISTITEMTEXT_H
