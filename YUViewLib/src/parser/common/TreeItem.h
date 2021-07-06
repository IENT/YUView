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

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// The tree item is used to feed the tree view.
class TreeItem : public std::enable_shared_from_this<TreeItem>
{
public:
  TreeItem()  = default;
  ~TreeItem() = default;

  void setProperties(std::string name    = {},
                     std::string value   = {},
                     std::string coding  = {},
                     std::string code    = {},
                     std::string meaning = {})
  {
    this->name    = name;
    this->value   = value;
    this->coding  = coding;
    this->code    = code;
    this->meaning = meaning;
  }

  void setError(bool isError = true) { this->error = isError; }
  bool isError() const { return this->error; }

  std::string getName(bool showStreamIndex) const
  {
    std::stringstream ss;
    if (showStreamIndex && this->streamIndex >= 0)
      ss << "Stream " << this->streamIndex << " - ";
    ss << this->name;
    return ss.str();
  }

  int getStreamIndex() const
  {
    if (this->streamIndex >= 0)
      return this->streamIndex;
    if (auto p = this->parent.lock())
      return p->getStreamIndex();
    return -1;
  }
  void setStreamIndex(int idx) { this->streamIndex = idx; }

  template <typename T>
  std::shared_ptr<TreeItem> createChildItem(std::string name    = {},
                                            T           value   = {},
                                            std::string coding  = {},
                                            std::string code    = {},
                                            std::string meaning = {},
                                            bool        isError = false)
  {
    auto newItem    = std::make_shared<TreeItem>();
    newItem->parent = this->weak_from_this();
    newItem->setProperties(name, std::to_string(value), coding, code, meaning);
    newItem->error = isError;
    this->childItems.push_back(newItem);
    return newItem;
  }

  std::shared_ptr<TreeItem> createChildItem(std::string name    = {},
                                            std::string value   = {},
                                            std::string coding  = {},
                                            std::string code    = {},
                                            std::string meaning = {},
                                            bool        isError = false)
  {
    auto newItem    = std::make_shared<TreeItem>();
    newItem->parent = this->weak_from_this();
    newItem->setProperties(name, value, coding, code, meaning);
    newItem->error = isError;
    this->childItems.push_back(newItem);
    return newItem;
  }

  size_t getNrChildItems() const { return this->childItems.size(); }

  std::string getData(unsigned idx) const
  {
    switch (idx)
    {
    case 0:
      return this->name;
    case 1:
      return this->value;
    case 2:
      return this->coding;
    case 3:
      return this->code;
    case 4:
      return this->meaning;
    default:
      return {};
    }
  }

  const std::shared_ptr<TreeItem> getChild(unsigned idx) const
  {
    if (idx < this->childItems.size())
      return this->childItems[idx];
    return {};
  }

  std::weak_ptr<TreeItem> getParentItem() const { return this->parent; }

  std::optional<size_t> getIndexOfChildItem(std::shared_ptr<TreeItem> child) const
  {
    for (size_t i = 0; i < this->childItems.size(); i++)
      if (this->childItems[i] == child)
        return i;

    std::optional<size_t> ret;
    return ret;
  }

private:
  std::vector<std::shared_ptr<TreeItem>> childItems;
  std::weak_ptr<TreeItem>                parent{};

  std::string name;
  std::string value;
  std::string coding;
  std::string code;
  std::string meaning;

  bool error{};
  // This is set for the first layer items in case of AVPackets
  int streamIndex{-1};
};
