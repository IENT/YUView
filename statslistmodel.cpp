/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "statslistmodel.h"

StatsListModel::StatsListModel(QObject *parent) : QAbstractListModel(parent)
{
}

int StatsListModel::rowCount(const QModelIndex &) const {
    return p_statisticsTypeList.count();
}

QVariant StatsListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role == Qt::CheckStateRole)
        return p_statisticsTypeList[index.row()].render?Qt::Checked:Qt::Unchecked;
    else if (role == Qt::DisplayRole)
        return p_statisticsTypeList[index.row()].typeName;
    else if (role == Qt::UserRole)
        return p_statisticsTypeList[index.row()].typeID;
    else if (role == Qt::UserRole+1)
        return p_statisticsTypeList[index.row()].alphaFactor;
    else if (role == Qt::UserRole+2)
        return p_statisticsTypeList[index.row()].renderGrid;

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
    if (index.row() >= 0 && index.row() < p_statisticsTypeList.count()) {
        switch (role) {
        case Qt::CheckStateRole:
            p_statisticsTypeList[index.row()].render = value.toUInt()==Qt::Checked;
            emit dataChanged(index, index);
            emit signalStatsTypesChanged();
            return true;
        case Qt::UserRole+1:
            p_statisticsTypeList[index.row()].alphaFactor = value.toInt();
            emit dataChanged(index, index);
            emit signalStatsTypesChanged();
            return true;
        case Qt::UserRole+2:
            p_statisticsTypeList[index.row()].renderGrid = value.toBool();
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
        StatisticsType newStatsType;
        p_statisticsTypeList.insert(row, newStatsType);
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
        p_statisticsTypeList.remove(row);
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
            bool draw = data(checkindex, Qt::CheckStateRole).toBool();
            bool drawgrid = data(index, Qt::UserRole+2).toBool();
            QString name = data(index, Qt::DisplayRole).toString();
            stream << ind << alpha << draw << drawgrid << name;
        }
    }

    mimeData->setData("application/YUView-Statistics", encodedData);
    return mimeData;
}

bool StatsListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &)
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
        bool draw;
        bool drawgrid;
        QString name;
        stream >> index >> alpha >> draw >> drawgrid >> name;

        if (row == INT_INVALID)
            row = p_statisticsTypeList.count();

        beginInsertRows(QModelIndex(), row, row);

        StatisticsType newStatsType;
        newStatsType.typeID = index;
        newStatsType.typeName = name;
        newStatsType.render = draw;
        newStatsType.renderGrid = drawgrid;
        newStatsType.alphaFactor = alpha;

        p_statisticsTypeList.insert(row, newStatsType);

        endInsertRows();
    }

    emit signalStatsTypesChanged();
    return true;
}

void StatsListModel::setStatisticsTypeList(StatisticsTypeList typeList)
{
    beginResetModel();

    // establish connection
    p_statisticsTypeList = typeList;

    endResetModel();
}

StatisticsTypeList StatsListModel::getStatisticsTypeList() { return p_statisticsTypeList; }
