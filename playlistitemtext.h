#ifndef PLAYLISTITEMTEXT_H
#define PLAYLISTITEMTEXT_H

#include <QTreeWidgetItem>
#include <QObject>
#include "playlistitem.h"
#include "textobject.h"
#include <QFont>

class PlaylistItemText : public PlaylistItem
{
public:
    PlaylistItemText(const QString &text, QFont font, double duration, QTreeWidget* parent = NULL);

    ~PlaylistItemText();

    PlaylistItemType itemType();

    TextObject *displayObject() {return dynamic_cast<TextObject*>(p_displayObject);}    

private:

};

#endif // PLAYLISTITEMTEXT_H
