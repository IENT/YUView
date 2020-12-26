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

#include "ReaderHelperNew.h"

#include <sstream>

namespace
{

template <typename T> std::string formatCoding(const std::string formatName, T value)
{
  std::ostringstream stringStream;
  stringStream << formatName << "(v) -> u(" << value << ")";
  return stringStream.str();
}

} // namespace

namespace parser
{

ByteVector ReaderHelperNew::convertBeginningToByteArray(QByteArray data)
{
  ByteVector ret;
  const auto maxLength = 2000u;
  const auto length    = std::min(unsigned(data.size()), maxLength);
  for (auto i = 0u; i < length; i++)
  {
    ret.push_back(data.at(i));
  }
  return ret;
}

ReaderHelperNew::ReaderHelperNew(SubByteReaderNew &reader,
                                 TreeItem *        item,
                                 std::string       new_sub_item_name)
{
  this->reader = reader;
  if (item)
  {
    if (new_sub_item_name.empty())
      this->currentTreeLevel = item;
    else
      this->currentTreeLevel = new TreeItem(item, new_sub_item_name);
  }
  this->itemHierarchy.push(this->currentTreeLevel);
}

ReaderHelperNew::ReaderHelperNew(const ByteVector &inArr,
                                 TreeItem *        item,
                                 std::string       new_sub_item_name,
                                 size_t            inOffset)
{
  this->reader = SubByteReaderNew(inArr, inOffset);
  if (item)
  {
    if (new_sub_item_name.empty())
      this->currentTreeLevel = item;
    else
      this->currentTreeLevel = new TreeItem(item, new_sub_item_name);
  }
  this->itemHierarchy.push(this->currentTreeLevel);
}

void ReaderHelperNew::addLogSubLevel(const std::string name)
{
  assert(!name.empty());
  if (itemHierarchy.top() == nullptr)
    return;
  this->currentTreeLevel = new TreeItem(this->itemHierarchy.top(), name);
  this->itemHierarchy.push(this->currentTreeLevel);
}

void ReaderHelperNew::removeLogSubLevel()
{
  if (itemHierarchy.size() <= 1)
    // Don't remove the root
    return;
  this->itemHierarchy.pop();
  this->currentTreeLevel = this->itemHierarchy.top();
}

uint64_t ReaderHelperNew::readBits(std::string symbolName, int numBits, Options options)
{
  try
  {
    auto [value, code] = this->reader.readBits(numBits);
    if (currentTreeLevel)
      new TreeItem(
          currentTreeLevel, symbolName, std::to_string(value), formatCoding("u", numBits), code);
    return value;
  }
  catch (const std::exception &ex)
  {
    // TODO
    // if (currentTreeLevel)
    //   new TreeItem("Error", "", "", "", errorMessage, item, true);
    (void)ex;
    return false;
  }
}

bool ReaderHelperNew::readFlag(std::string symbolName, Options options)
{
  try
  {
    auto [value, code] = this->reader.readBits(1);
    if (currentTreeLevel)
      new TreeItem(currentTreeLevel, symbolName, std::to_string(value), "u(1)", code);
    return (value != 0);
  }
  catch (const std::exception &ex)
  {
    // TODO
    // if (currentTreeLevel)
    //   new TreeItem("Error", "", "", "", errorMessage, item, true);
    (void)ex;
    return false;
  }
}

} // namespace parser