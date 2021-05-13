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

#include "common/Color.h"
#include "common/functionsGui.h"
#include "common/typedef.h"

#include <QBrush>

#if PARSERCOMMON_DEBUG_FILTER_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FILTER qDebug
#else
#define DEBUG_FILTER(fmt, ...) ((void)0)
#endif

// These are form the google material design color chooser (https://material.io/tools/color/)
auto streamIndexColors = std::vector<Color>({Color("#90caf9"),   // blue (200)
                                             Color("#a5d6a7"),   // green (200)
                                             Color("#ffe082"),   // amber (200)
                                             Color("#ef9a9a"),   // red (200)
                                             Color("#fff59d"),   // yellow (200)
                                             Color("#ce93d8"),   // purple (200)
                                             Color("#80cbc4"),   // teal (200)
                                             Color("#bcaaa4"),   // brown (200)
                                             Color("#c5e1a5"),   // light green (200)
                                             Color("#1e88e5"),   // blue (600)
                                             Color("#43a047"),   // green (600)
                                             Color("#ffb300"),   // amber (600)
                                             Color("#fdd835"),   // yellow (600)
                                             Color("#8e24aa"),   // purple (600)
                                             Color("#00897b"),   // teal (600)
                                             Color("#6d4c41"),   // brown (600)
                                             Color("#7cb342")}); // light green (600)

PacketItemModel::PacketItemModel(QObject *parent) : QAbstractItemModel(parent) {}

PacketItemModel::~PacketItemModel() {}

QVariant PacketItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && this->rootItem)
  {
    auto dataStd = rootItem->getData(section);
    return QVariant(QString::fromStdString(dataStd));
  }

  return {};
}

QVariant PacketItemModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return {};

  auto item = static_cast<TreeItem *>(index.internalPointer());
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
      return QVariant(
          QBrush(functionsGui::toQColor(streamIndexColors.at(idx % streamIndexColors.size()))));
    return QVariant(QBrush());
  }
  else if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
  {
    if (index.column() == 0)
      return QVariant(QString::fromStdString(item->getName(!showVideoOnly)));
    else
      return QVariant(QString::fromStdString(item->getData(index.column())));
  }
  return QVariant();
}

QModelIndex PacketItemModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
    return {};

  auto parentItem = this->rootItem.get();
  if (parent.isValid())
    parentItem = static_cast<TreeItem *>(parent.internalPointer());

  Q_ASSERT_X(
      parentItem != nullptr, Q_FUNC_INFO, "pointer to parent is null. This must never happen");

  auto childItem = parentItem->getChild(row);
  if (childItem)
    return this->createIndex(row, column, childItem.get());
  return {};
}

QModelIndex PacketItemModel::parent(const QModelIndex &index) const
{
  if (!index.isValid())
    return {};

  auto childItem  = static_cast<TreeItem *>(index.internalPointer());
  auto parentItem = childItem->getParentItem().lock();

  if (parentItem == this->rootItem)
    return {};

  // Get the row of the item in the list of children of the parent item
  int row = 0;
  if (parentItem)
  {
    auto grandparent = parentItem->getParentItem().lock();
    if (grandparent)
    {
      if (auto rowIndex = grandparent->getIndexOfChildItem(parentItem))
        row = int(*rowIndex);
    }
  }

  return createIndex(row, 0, parentItem.get());
}

int PacketItemModel::rowCount(const QModelIndex &parent) const
{
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    return this->nrShowChildItems;
  auto p = static_cast<TreeItem *>(parent.internalPointer());
  return (p == nullptr) ? 0 : int(p->getNrChildItems());
}

size_t PacketItemModel::getNumberFirstLevelChildren() const
{ 
  if (this->rootItem)
    return rootItem->getNrChildItems();
  return {};
}

void PacketItemModel::updateNumberModelItems()
{
  auto n = getNumberFirstLevelChildren();
  Q_ASSERT_X(n >= this->nrShowChildItems, Q_FUNC_INFO, "Setting a smaller number of items.");
  auto nrAddItems = int(n - this->nrShowChildItems);
  if (nrAddItems == 0)
    return;

  auto lastIndex = int(this->nrShowChildItems);
  beginInsertRows(QModelIndex(), lastIndex, lastIndex + nrAddItems - 1);
  this->nrShowChildItems = unsigned(n);
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
    auto s = sourceModel();
    auto p = static_cast<PacketItemModel *>(s);
    if (p == nullptr)
    {
      DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow Unable to get root item");
      return false;
    }
    parentItem = p->rootItem.get();
  }
  else
    parentItem = static_cast<TreeItem *>(sourceParent.internalPointer());
  Q_ASSERT_X(
      parentItem != nullptr, Q_FUNC_INFO, "pointer to parent is null. This must never happen");

  auto childItem = parentItem->getChild(row);
  if (childItem != nullptr)
  {
    DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow item %d",
                 childItem->getStreamIndex());
    return childItem->getStreamIndex() == streamIndex || childItem->getStreamIndex() == -1;
  }

  DEBUG_FILTER("FilterByStreamIndexProxyModel::filterAcceptsRow item null -> reject");
  return false;
}
