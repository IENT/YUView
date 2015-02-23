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

    void setCurrentStatistics(StatisticsObject* stats, QVector<StatisticsRenderItem> &renderTypes);
    QVector<StatisticsRenderItem> getStatistics();

signals:
    void signalStatsTypesChanged();

private:
    QVector<int> indices;
    QVector<Qt::CheckState> checkStates;
    QVector<bool> drawGrids;
    QVector<QString> names;
    QVector<int> alphas;
};

#endif // QSTATSLISTMODEL_H
