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

#pragma once

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

#include "treeItem.h"

// The item model which is used to display packets from the bitstream. This can be AVPackets or other units from the bitstream (NAL units e.g.)
class PacketItemModel : public QAbstractItemModel
{
  Q_OBJECT

public:
  PacketItemModel(QObject *parent);
  ~PacketItemModel();

  // The functions that must be overridden from the QAbstractItemModel
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
  virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
  virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
  virtual QModelIndex parent(const QModelIndex &index) const Q_DECL_OVERRIDE;
  virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
  virtual int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE { Q_UNUSED(parent); return 5; }

  // The root of the tree
  QScopedPointer<TreeItem> rootItem;
  TreeItem *getRootItem() { return rootItem.data(); }
  bool isNull() { return rootItem.isNull(); }

  void setUseColorCoding(bool colorCoding);
  void setShowVideoStreamOnly(bool showVideoOnly);

  void updateNumberModelItems();
private:
  // This is the current number of first level child items which we show right now.
  // The brackground parser will add more items and it will notify the bitstreamAnalysisWindow
  // about them. The bitstream analysis window will then update this count and the view to show the new items.
  unsigned int nrShowChildItems {0};

  unsigned int getNumberFirstLevelChildren() { return rootItem.isNull() ? 0 : rootItem->childItems.size(); }

  static QList<QColor> streamIndexColors;
  bool useColorCoding { true };
  bool showVideoOnly  { false };
};

class FilterByStreamIndexProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  FilterByStreamIndexProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {};

  int filterStreamIndex() const { return streamIndex; }
  void setFilterStreamIndex(int idx);

protected:
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
  int streamIndex { -1 };
};