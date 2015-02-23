#include "playlistitem.h"

#include "playlistitemvid.h"

PlaylistItem::PlaylistItem(const QString &itemName, QTreeWidget * parent) : QTreeWidgetItem ( parent, 1001 )
{
    p_statsParser = 0;

    // update item name to short name
    setText(0, itemName);

    // update icon
    setIcon(0, QIcon(":images/img_folder.png"));

    // make sure drag&drop is possible for group items
    setFlags(flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled);
}

PlaylistItem::~PlaylistItem() {
    if (p_statsParser != 0) {
        delete p_statsParser;
        p_statsParser = 0;
    }
}

void PlaylistItem::setStatisticsObject(StatisticsObject* stats) {
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

void PlaylistItem::updateStatsTypes(QVector<StatisticsRenderItem> types) {
    p_renderStatsTypes = types;
}
