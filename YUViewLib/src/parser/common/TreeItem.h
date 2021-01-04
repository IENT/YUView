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

#include <QList>
#include <QString>

// The tree item is used to feed the tree view. Each NAL unit can return a representation using
// TreeItems
class TreeItem
{
public:
  // Some useful constructors of new Tree items. You must at least specify a parent. The new item is
  // atomatically added as a child of the parent.
  TreeItem(QList<QString> &data, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData = data;
  }
  TreeItem(const QString &name, int val, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val);
  }
  TreeItem(const QString &name, QString val, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << val;
  }
  TreeItem(
      const QString &name, int val, const QString &coding, const QString &code, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
  }
  TreeItem(const QString &name,
           unsigned int   val,
           const QString &coding,
           const QString &code,
           TreeItem *     parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
  }
  TreeItem(const QString &name,
           uint64_t       val,
           const QString &coding,
           const QString &code,
           TreeItem *     parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
  }
  TreeItem(const QString &name,
           int64_t        val,
           const QString &coding,
           const QString &code,
           TreeItem *     parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
  }
  TreeItem(
      const QString &name, bool val, const QString &coding, const QString &code, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << (val ? "1" : "0") << coding << code;
  }
  TreeItem(
      const QString &name, double val, const QString &coding, const QString &code, TreeItem *parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
  }
  TreeItem(const QString &name,
           QString        val,
           const QString &coding,
           const QString &code,
           TreeItem *     parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << val << coding << code;
  }
  TreeItem(const QString &name,
           int            val,
           const QString &coding,
           const QString &code,
           QString        meaning,
           TreeItem *     parent)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << QString::number(val) << coding << code;
    itemData.append(meaning);
  }
  TreeItem(const QString &name,
           QString        val,
           const QString &coding,
           const QString &code,
           QString        meaning,
           TreeItem *     parent,
           bool           isError = false)
  {
    parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    itemData << name << val << coding << code;
    itemData.append(meaning);
    setError(isError);
  }

  TreeItem(TreeItem *         parent,
           const std::string &name    = {},
           const std::string &value   = {},
           const std::string &coding  = {},
           const std::string &code    = {},
           const std::string &meaning = {})
  {
    this->parentItem = parent;
    if (parent)
      parent->childItems.append(this);
    this->setProperties(name, value, coding, code, meaning);
  }

  void setProperties(const std::string &name    = {},
                     const std::string &value   = {},
                     const std::string &coding  = {},
                     const std::string &code    = {},
                     const std::string &meaning = {})
  {
    this->itemData.clear();
    this->itemData << QString::fromStdString(name) << QString::fromStdString(value)
                   << QString::fromStdString(coding) << QString::fromStdString(code)
                   << QString::fromStdString(meaning);
  }

  ~TreeItem() { qDeleteAll(childItems); }
  void setError(bool isError = true) { error = isError; }
  bool isError() { return error; }

  QString getName(bool showStreamIndex) const
  {
    QString r =
        (showStreamIndex && streamIndex != -1) ? QString("Stream %1 - ").arg(streamIndex) : "";
    if (itemData.count() > 0)
      r += itemData[0];
    return r;
  }

  QList<TreeItem *> childItems;
  QList<QString>    itemData;
  TreeItem *        parentItem{nullptr};

  int getStreamIndex()
  {
    if (streamIndex >= 0)
      return streamIndex;
    if (parentItem)
      return parentItem->getStreamIndex();
    return -1;
  }
  void setStreamIndex(int idx) { streamIndex = idx; }

private:
  bool error{false};
  // This is set for the first layer items in case of AVPackets
  int streamIndex{-1};
};
