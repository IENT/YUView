#ifndef YUVLISTITEM_H
#define YUVLISTITEM_H

#include <QTreeWidgetItem>
#include "yuvobject.h"
#include "statisticsparser.h"

enum YUVListItemType {
    GroupItem,
    VideoItem
};

class YUVListItem : public QTreeWidgetItem
{
public:
    YUVListItem(const QString &itemName, QTreeWidget * parent = 0);
    YUVListItem(const QString &itemName, YUVListItem* parentItem = 0);

    YUVListItem(const QStringList &fileNames, QTreeWidget* parent = 0);
    ~YUVListItem();

    virtual YUVObject *renderObject();

    virtual YUVListItemType itemType();

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
};

#endif // YUVLISTITEM_H
