#include "statslistmodel.h"

StatsListModel::StatsListModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int StatsListModel::rowCount(const QModelIndex &) const {
    return indices.size();
}

QVariant StatsListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role == Qt::CheckStateRole)
        return checkStates[index.row()];
    else
    if (role == Qt::DisplayRole) {
        return names[index.row()];
    }
    else
    if (role == Qt::UserRole)
        return indices[index.row()];
    else
    if (role == Qt::UserRole+1)
        return alphas[index.row()];
    else
    if (role == Qt::UserRole+2)
        return drawGrids[index.row()];

    return QVariant();
}

Qt::ItemFlags StatsListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    else
        return (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable);
}

Qt::DropActions StatsListModel::supportedDropActions() const
{
    return QAbstractItemModel::supportedDropActions() | Qt::MoveAction;
}

bool StatsListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() >= 0 && index.row() < indices.size()) {
        switch (role) {
        case Qt::CheckStateRole:
            checkStates[index.row()] = static_cast<Qt::CheckState>(value.toUInt());
            emit dataChanged(index, index);
            emit signalStatsTypesChanged();
            return true;
        case Qt::UserRole+1:
            alphas[index.row()] = value.toInt();
            emit dataChanged(index, index);
            emit signalStatsTypesChanged();
            return true;
        case Qt::UserRole+2:
            drawGrids[index.row()] = value.toBool();
            emit dataChanged(index, index);
            emit signalStatsTypesChanged();
            return true;
        }
    }
    return false;
}

bool StatsListModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if (count < 1 || row < 0 || row > rowCount(parent))
        return false;

    beginInsertRows(QModelIndex(), row, row + count - 1);

    for (int r = 0; r < count; ++r) {
        indices.insert(row, -1);
        checkStates.insert(row, Qt::Unchecked);
        drawGrids.insert(row, false);
        names.insert(row, "?");
        alphas.insert(row, 50);
    }

    endInsertRows();

    emit signalStatsTypesChanged();

    return true;
}

bool StatsListModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (count <= 0 || row < 0 || (row + count) > rowCount(parent))
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);

    for (int r = 0; r < count; ++r) {
        indices.remove(row);
        checkStates.remove(row);
        drawGrids.remove(row);
        names.remove(row);
        alphas.remove(row);
    }

    endRemoveRows();

    emit signalStatsTypesChanged();

    return true;
}


QStringList StatsListModel::mimeTypes() const
{
    QStringList types;
    types << "application/YUView-Statistics";
    return types;
}

QMimeData *StatsListModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (QModelIndex index, indexes) {
        if (index.isValid()) {
            int ind = data(index, Qt::UserRole).toInt();
            int alpha = data(index, Qt::UserRole+1).toInt();
            QModelIndex checkindex = StatsListModel::createIndex(index.row(), 1, 999);
            unsigned int checkstate = data(checkindex, Qt::CheckStateRole).toUInt();
            bool drawgrid = data(index, Qt::UserRole+2).toBool();
            QString name = data(index, Qt::DisplayRole).toString();
            stream << ind << alpha << checkstate << drawgrid << name;
        }
    }

    mimeData->setData("application/YUView-Statistics", encodedData);
    return mimeData;
}

bool StatsListModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                               int row, int column, const QModelIndex &)
{
    if (!data->hasFormat("application/YUView-Statistics"))
        return false;

    if (action == Qt::IgnoreAction)
        return true;

    if (column > 2)
        return false;

    QByteArray encodedData = data->data("application/YUView-Statistics");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    while (!stream.atEnd()) {
        int index;
        int alpha;
        unsigned int checkstate;
        bool drawgrid;
        QString name;
        stream >> index >> alpha >> checkstate >> drawgrid >> name;

        if (row == -1) row = indices.size();

        beginInsertRows(QModelIndex(), row, row);
        indices.insert(row, index);
        checkStates.insert(row, static_cast<Qt::CheckState>(checkstate));
        drawGrids.insert(row, drawgrid);
        names.insert(row, name);
        alphas.insert(row, alpha);
        endInsertRows();
    }

    emit signalStatsTypesChanged();
    return true;
}

void StatsListModel::setCurrentStatistics(StatisticsParser* stats, QVector<StatisticsRenderItem> &renderTypes) {
    beginResetModel();

    indices.clear();
    checkStates.clear();
    drawGrids.clear();
    names.clear();
    alphas.clear();

    if (stats != 0) {
        indices.resize(renderTypes.size());
        checkStates.resize(renderTypes.size());
        drawGrids.resize(renderTypes.size());
        names.resize(renderTypes.size());
        alphas.resize(renderTypes.size());

        for (int i=0; i<renderTypes.size(); i++) {
            indices[i] = renderTypes[i].type_id;
            checkStates[i] = renderTypes[i].render ? Qt::Checked : Qt::Unchecked;
            drawGrids[i] = renderTypes[i].renderGrid;
            names[i] = stats->getTypeName(renderTypes[i].type_id).c_str();
            alphas[i] = renderTypes[i].alpha;
        }
    }
    endResetModel();
}

QVector<StatisticsRenderItem> StatsListModel::getStatistics() {
    QVector<StatisticsRenderItem> tmp;
    tmp.resize(indices.size());
    StatisticsRenderItem item;

    for (int i=0; i<indices.size(); i++) {
        item.renderGrid = drawGrids[i];
        item.render = checkStates[i];
        item.type_id = indices[i];
        item.alpha = alphas[i];
        tmp[i] = item;
    }

    return tmp;
}
