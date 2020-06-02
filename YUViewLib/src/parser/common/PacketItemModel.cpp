/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "PacketItemModel.h"

#include <QBrush>
#include <QColor>

#if PARSERCOMMON_DEBUG_FILTER_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FILTER qDebug
#else
#define DEBUG_FILTER(fmt,...) ((void)0)
#endif

// These are form the google material design color chooser (https://material.io/tools/color/)
QList<QColor> PacketItemModel::streamIndexColors = QList<QColor>()
  << QColor("#90caf9")  // blue (200)
  << QColor("#a5d6a7")  // green (200)
  << QColor("#ffe082")  // amber (200)
  << QColor("#ef9a9a")  // red (200)
  << QColor("#fff59d")  // yellow (200)
  << QColor("#ce93d8")  // purple (200)
  << QColor("#80cbc4")  // teal (200)
  << QColor("#bcaaa4")  // brown (200)
  << QColor("#c5e1a5")  // light green (200)
  << QColor("#1e88e5")  // blue (600)
  << QColor("#43a047")  // green (600)
  << QColor("#ffb300")  // amber (600)
  << QColor("#fdd835")  // yellow (600)
  << QColor("#8e24aa")  // purple (600)
  << QColor("#00897b")  // teal (600)
  << QColor("#6d4c41")  // brown (600)
  << QColor("#7cb342"); // light green (600)

PacketItemModel::PacketItemModel(QObject *parent) : QAbstractItemModel(parent)
{
}

PacketItemModel::~PacketItemModel()
{
}

QVariant PacketItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && rootItem != nullptr)
    return rootItem->itemData.value(section, QString());

  return QVariant();
}

QVariant PacketItemModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return QVariant();

  TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
  if (role == Qt::ForegroundRole)
  {
    if (item->isError())
      return QVariant(QBrush(QColor(255, 0, 0)));
    return QVariant(QBrush());
  }
  if (role == Qt::BackgroundRole)
  {
    if (!useColorCoding)
      return QVariant(QBrush());
    const int idx = item->getStreamIndex();
    if (idx >= 0)
      return QVariant(QBrush(streamIndexColors.at(idx % streamIndexColors.length())));
    return QVariant(QBrush());
  }
  else if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
  {
    if (index.column() == 0)
      return QVariant(item->getName(!showVideoOnly));
    else
      return QVariant(item->itemData.value(index.column()));
  }
  return QVariant();
}

QModelIndex PacketItemModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  TreeItem *parentItem;
  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  Q_ASSERT_X(parentItem != nullptr, Q_FUNC_INFO, "pointer to parent is null. This must never happen");

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex PacketItemModel::parent(const QModelIndex &index) const
{
  if (!index.isValid())
    return QModelIndex();

  TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
  TreeItem *parentItem = childItem->parentItem;

  if (parentItem == rootItem.data())
    return QModelIndex();

  // Get the row of the item in the list of children of the parent item
  int row = 0;
  if (parentItem)
    row = parentItem->parentItem->childItems.indexOf(const_cast<TreeItem*>(parentItem));

  return createIndex(row, 0, parentItem);
}

int PacketItemModel::rowCount(const QModelIndex &parent) const
{
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
  {
    TreeItem *p = rootItem.data();
    return (p == nullptr) ? 0 : nrShowChildItems;
  }
  TreeItem *p = static_cast<TreeItem*>(parent.internalPointer());
  return (p == nullptr) ? 0 : p->childItems.count();
}

void PacketItemModel::updateNumberModelItems()
{
  auto n = getNumberFirstLevelChildren();
  Q_ASSERT_X(n >= nrShowChildItems, Q_FUNC_INFO, "Setting a smaller number of items.");
  unsigned int nrAddItems = n - nrShowChildItems;
  if (nrAddItems == 0)
    return;

  int lastIndex = nrShowChildItems;
  beginInsertRows(QModelIndex(), lastIndex, lastIndex + nrAddItems - 1);
  nrShowChildItems = n;
  endInsertRows();
}

void PacketItemModel::setUseColorCoding(bool colorCoding)
{
  if (useColorCoding == colorCoding)
    return;

  useColorCoding = colorCoding;
  emit dataChanged(QModelIndex(), QModelIndex(), QVector<int>() << Qt::BackgroundRole);
}

void PacketItemModel::setShowVideoStreamOnly(bool videoOnly)
{
  if (showVideoOnly == videoOnly)
    return;

  showVideoOnly = videoOnly;
  emit dataChanged(QModelIndex(), QModelIndex());
}

/// ------------------- FilterByStreamIndexProxyModel -----------------------------

void FilterByStreamIndexProxyModel::setFilterStreamIndex(int idx)
{
  this->streamIndex = idx;
  invalidateFilter();
}

bool FilterByStreamIndexProxyModel::filterAcceptsRow(int row, const QModelIndex &sourceParent) const
{
  if (streamIndex == -1)
  {
    DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow %d - accepting all", row);
    return true;
  }

  TreeItem *parentItem;
  if (!sourceParent.isValid())
  {
    // Get the root item
    QAbstractItemModel *s = sourceModel();
    PacketItemModel *p = static_cast<PacketItemModel*>(s);
    if (p == nullptr)
    {
      DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow Unable to get root item");  
      return false;
    }
    parentItem = p->getRootItem();
  }
  else
    parentItem = static_cast<TreeItem*>(sourceParent.internalPointer());
  Q_ASSERT_X(parentItem != nullptr, Q_FUNC_INFO, "pointer to parent is null. This must never happen");

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem != nullptr)
  {
    DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow item %d", childItem->getStreamIndex());
    return childItem->getStreamIndex() == streamIndex || childItem->getStreamIndex() == -1;
  }

  DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow item null -> reject");
  return false;
}