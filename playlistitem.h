#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include "displayobject.h"
#include "statisticsobject.h"

enum PlaylistItemType {
    VideoItem,
    TextItem,
    StatisticsItem
};

class PlaylistItem : public QTreeWidgetItem
{
public:
    PlaylistItem(const QString &itemName, QTreeWidget * parent = 0);

    ~PlaylistItem();

    virtual DisplayObject *displayObject() { return p_displayObject; }

    virtual PlaylistItemType itemType() = 0;

    virtual StatisticsObject* getStatisticsObject() { return p_statsParser; }
    virtual bool statisticsSupported() { return true; } // by default all listitems can have stats
    void setStatisticsObject(StatisticsObject* stats);
    QVector<StatisticsRenderItem>& getStatsTypes() { return p_renderStatsTypes; }

public slots:
    void updateStatsTypes(QVector<StatisticsRenderItem> types);

private:
    StatisticsObject *p_statsParser;
    QVector<StatisticsRenderItem> p_renderStatsTypes; // contains all type-IDs of stats and whether they should be rendered (in order)

protected:
    DisplayObject* p_displayObject;
};

#endif // PLAYLISTITEM_H
