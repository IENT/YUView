#ifndef PLAYLISTITEMSTATS_H
#define PLAYLISTITEMSTATS_H

#include <QTreeWidgetItem>

#include "playlistitem.h"
#include "statisticsobject.h"

class PlaylistItemStats : public PlaylistItem
{
public:
    PlaylistItemStats(const QString &srcFileName, QTreeWidget* parent = NULL);
    PlaylistItemStats(const QString &srcFileName, QTreeWidgetItem* parentItem);

    ~PlaylistItemStats();

    PlaylistItemType itemType();

    StatisticsObject *displayObject() { return dynamic_cast<StatisticsObject*>(p_displayObject); }

public slots:

private:


};

#endif // PLAYLISTITEMSTATS_H
