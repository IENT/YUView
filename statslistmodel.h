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

#ifndef QSTATSLISTMODEL_H
#define QSTATSLISTMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QStringList>
#include <QMimeData>
#include "statisticsobject.h"

class QMimeData;

class StatsListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit StatsListModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    Qt::DropActions supportedDropActions() const;
    bool removeRows(int row, int count, const QModelIndex &index);
    bool insertRows(int row, int count, const QModelIndex &index);
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    QStringList mimeTypes() const;

    void setStatisticsTypeList(StatisticsTypeList stats);
    StatisticsTypeList getStatisticsTypeList();

signals:
    void signalStatsTypesChanged();

private:

    StatisticsTypeList p_statisticsTypeList;
};

#endif // QSTATSLISTMODEL_H
