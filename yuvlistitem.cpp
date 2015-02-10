#include "yuvlistitem.h"

#include "yuvlistitemvid.h"

YUVListItem::YUVListItem(const QString &itemName, QTreeWidget * parent) : QTreeWidgetItem ( parent, 1001 )
{
    p_statsParser = 0;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon(":images/img_folder.png"));

    // make sure drag&drop is possible for group items
    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

YUVListItem::YUVListItem(const QString &itemName, YUVListItem* parentItem) : QTreeWidgetItem ( parentItem, 1001 )
{
    p_statsParser = 0;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon(":images/img_folder.png"));

    // make sure drag&drop is possible for group items
    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

YUVListItem::YUVListItem(const QStringList &fileNames, QTreeWidget* parent) : QTreeWidgetItem ( parent, 1001 )
{
    p_statsParser = 0;

    for(int i = 0; i < fileNames.count(); i++)
    {
        QString fileName = fileNames.at(i);

        new YUVListItemVid(fileName, this);
    }

    // update item name to short name
    QString folderName = fileNames.at(0) + " (Group)";
    setText(0, folderName);

    // update icon
    setIcon(0, QIcon(":images/img_folder.png"));
}

YUVListItem::~YUVListItem() {
    if (p_statsParser != 0) {
        delete p_statsParser;
        p_statsParser = 0;
    }
}

YUVObject* YUVListItem::renderObject()
{
    return NULL;
}

YUVListItemType YUVListItem::itemType()
{
    return GroupItem;
}

void YUVListItem::setStatisticsParser(StatisticsParser* stats) {
    p_statsParser = stats;
    p_renderStatsTypes.clear();

    // update icon
    setIcon(1, QIcon());
    if (stats == 0) return;

    setIcon(1, QIcon(":images/stats.png"));

    std::vector<int> types = stats->getTypeIDs();
    StatisticsRenderItem item;

    item.renderGrid = false;
    item.render = false;
    item.alpha = 50;
    for (int i=0; i<(int)types.size(); i++) {
        if (types[i] == -1)
            continue;
        item.type_id = types[i];
        p_renderStatsTypes.push_back(item);
    }
}

void YUVListItem::updateStatsTypes(QVector<StatisticsRenderItem> types) {
    p_renderStatsTypes = types;
}
