#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include "displayobject.h"
#include "statisticsparser.h"

enum PlayListItemType {
    VideoItem,
    TextItem,
    StatisticsItem
};

class PlayListItem : public QTreeWidgetItem
{
public:
    PlayListItem(const QString &itemName, QTreeWidget * parent = 0);

    ~PlayListItem();

    DisplayObject *displayObject() { return p_displayObject; }

    virtual PlayListItemType itemType() = 0;

    virtual StatisticsParser* getStatisticsParser() { return p_statsParser; }
    virtual bool statisticsSupported() { return true; } // by default all listitems can have stats
    void setStatisticsParser(StatisticsParser* stats);
    QVector<StatisticsRenderItem>& getStatsTypes() { return p_renderStatsTypes; }

public slots:
    void updateStatsTypes(QVector<StatisticsRenderItem> types);

private:
    StatisticsParser *p_statsParser;
    QVector<StatisticsRenderItem> p_renderStatsTypes; // contains all type-IDs of stats and whether they should be rendered (in order)

protected:
    DisplayObject* p_displayObject;
};

#endif // PLAYLISTITEM_H
