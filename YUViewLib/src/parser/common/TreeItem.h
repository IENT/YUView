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
#include <vector>
#include <memory>

// The tree item is used to feed the tree view. Each NAL unit can return a representation using
// TreeItems
class TreeItem
{
public:
  TreeItem() = default;
  ~TreeItem() = default;

  void setProperties(const std::string &name    = {},
                     const std::string &value   = {},
                     const std::string &coding  = {},
                     const std::string &code    = {},
                     const std::string &meaning = {})
  {
    this->itemData.name    = name;
    this->itemData.value   = value;
    this->itemData.coding  = coding;
    this->itemData.code    = code;
    this->itemData.meaning = meaning;
  }

  TreeItem *addChild(const std::string &name    = {},
                     const std::string &value   = {},
                     const std::string &coding  = {},
                     const std::string &code    = {},
                     const std::string &meaning = {})
  {
    auto newItem = std::make_unique<TreeItem>();
    newItem->parentItem = this;
    newItem->setProperties(name, value, coding, code, meaning);
    this->childItems.push_back(std::move(newItem));
    return this->childItems.back().get();
  }

  void setError(bool isError = true) { error = isError; }
  bool isError() const { return error; }

  std::string getName(bool showStreamIndex) const
  {

    std::string r = (showStreamIndex && streamIndex != -1)
                        ? ("Stream " + std::to_string(streamIndex) + " - ")
                        : "";
    if (!itemData.name.empty())
      r += itemData.name;
    return r;
  }

  struct ItemData
  {
    std::string name;
    std::string value;
    std::string coding;
    std::string code;
    std::string meaning;
  };
  ItemData itemData;

  int getStreamIndex() const
  {
    if (streamIndex >= 0)
      return streamIndex;
    if (parentItem)
      return parentItem->getStreamIndex();
    return -1;
  }
  void setStreamIndex(int idx) { streamIndex = idx; }

  TreeItem *getChildOrNull(size_t index) const
  {
    if (index >= this->childItems.size())
      return nullptr;
    return this->childItems.at(index).get();
  }

  TreeItem *getParent() const
  {
    return this->parentItem;
  }

  std::optional<size_t> getIndexOfChildItem(TreeItem *item) const
  {
    if (item == nullptr)
      return {};
    for (size_t i = 0; i < this->childItems.size(); i++)
    {
      if (this->childItems.at(i).get() == item)
        return i;
    }
    return {};
  }

  size_t getNrChildItems() const
  {
    return this->childItems.size();
  }

private:
  std::vector<std::unique_ptr<TreeItem>> childItems;

  TreeItem *parentItem{nullptr};

  bool error{false};
  // This is set for the first layer items in case of AVPackets
  int streamIndex{-1};
};
