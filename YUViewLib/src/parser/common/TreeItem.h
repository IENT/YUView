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

#include <optional>
#include <sstream>
#include <string>
#include <vector>

// The tree item is used to feed the tree view. Each NAL unit can return a representation using
// TreeItems
class TreeItem
{
public:
  // Some useful constructors of new Tree items. You must at least specify a parent. The new item is
  // atomatically added as a child of the parent.
  TreeItem() = default;
  TreeItem(std::vector<std::string> &data) { this->itemData = data; }
  TreeItem(std::string name, int val) { this->itemData = {name, std::to_string(val)}; }
  TreeItem(std::string &name, int val, std::string coding, std::string code)
  {
    this->itemData = {name, std::to_string(val), coding, code};
  }
  TreeItem(std::string name, unsigned val, std::string coding, std::string code)
  {
    this->itemData = {name, std::to_string(val), coding, code};
  }
  TreeItem(std::string name, uint64_t val, std::string coding, std::string code)
  {
    this->itemData = {name, std::to_string(val), coding, code};
  }
  TreeItem(std::string name, int64_t val, std::string coding, std::string code)
  {
    this->itemData = {name, std::to_string(val), coding, code};
  }
  TreeItem(std::string name, bool val, std::string coding, std::string code)
  {
    this->itemData = {name, (val ? "1" : "0"), coding, code};
  }
  TreeItem(std::string name, double val, std::string coding, std::string code)
  {
    this->itemData = {name, std::to_string(val), coding, code};
  }
  TreeItem(std::string name, int val, std::string coding, std::string code, std::string meaning)
  {
    this->itemData = {name, std::to_string(val), coding, code, meaning};
  }
  TreeItem(std::string name,
           std::string val     = {},
           std::string coding  = {},
           std::string code    = {},
           std::string meaning = {},
           bool        isError = false)
  {
    this->itemData = {name, val, coding, code, meaning};
    this->setError(isError);
  }

  ~TreeItem() = default;

  void setProperties(std::string name    = {},
                     std::string value   = {},
                     std::string coding  = {},
                     std::string code    = {},
                     std::string meaning = {})
  {
    this->itemData = {name, value, coding, code, meaning};
  }

  void setError(bool isError = true) { this->error = isError; }
  bool isError() const { return this->error; }

  std::string getName(bool showStreamIndex) const
  {
    std::stringstream ss;
    if (showStreamIndex && this->streamIndex >= 0)
      ss << "Stream " << this->streamIndex << " - ";
    if (this->itemData.size() > 0)
      ss << this->itemData[0];
    return ss.str();
  }

  int getStreamIndex() const
  {
    if (this->streamIndex >= 0)
      return this->streamIndex;
    if (parentItem)
      return parentItem->getStreamIndex();
    return -1;
  }
  void setStreamIndex(int idx) { this->streamIndex = idx; }

  // Return a pointer to the added item
  TreeItem *addChildItem(TreeItem newChild)
  {
    newChild.parentItem = this;
    this->childItems.push_back(newChild);
    return &this->childItems.back();
  }

  size_t getNrChildItems() const { return this->childItems.size(); }

  std::string getData(unsigned idx) const
  {
    if (idx < this->itemData.size())
      return this->itemData.at(idx);
    return {};
  }

  const TreeItem *getChild(unsigned idx) const
  {
    if (idx < this->childItems.size())
      return &this->childItems[idx];
    return {};
  }

  TreeItem *getParentItem() const { return this->parentItem; }

  std::optional<size_t> getIndexOfChildItem(TreeItem *child)
  {
    for (size_t i = 0; i < this->childItems.size(); i++)
      if ((&this->childItems[i]) == child)
        return i;
    return {};
  }

private:
  std::vector<TreeItem>    childItems;
  std::vector<std::string> itemData;
  TreeItem *               parentItem{};

  bool error{};
  // This is set for the first layer items in case of AVPackets
  int streamIndex{-1};
};
