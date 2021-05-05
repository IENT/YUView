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

#include <map>
#include <optional>
#include <stack>

#include "SubByteReader.h"
#include "SubByteReaderLoggingOptions.h"
#include "TreeItem.h"
#include "common/typedef.h"

namespace parser::reader
{

typedef std::string (*meaning_callback_function)(unsigned int);

class SubByteReaderLoggingSubLevel;

// This is a wrapper around the sub_byte_reader that adds the functionality to log the read symbold
// to TreeItems
class SubByteReaderLogging : public SubByteReader
{
public:
  SubByteReaderLogging() = default;
  SubByteReaderLogging(SubByteReader &           reader,
                       std::shared_ptr<TreeItem> item,
                       std::string               new_sub_item_name = "");
  SubByteReaderLogging(const ByteVector &        inArr,
                       std::shared_ptr<TreeItem> item,
                       std::string               new_sub_item_name = "",
                       size_t                    inOffset          = 0);

  // DEPRECATED. This is just for backwards compatibility and will be removed once
  // everything is using std types.
  static ByteVector convertToByteVector(QByteArray data);
  static QByteArray convertToQByteArray(ByteVector data);

  uint64_t readBits(const std::string &symbolName, size_t numBits, const Options &options = {});
  bool     readFlag(const std::string &symbolName, const Options &options = {});
  uint64_t readUEV(const std::string &symbolName, const Options &options = {});
  int64_t  readSEV(const std::string &symbolName, const Options &options = {});
  uint64_t readLEB128(const std::string &symbolName, const Options &options = {});
  uint64_t readNS(const std::string &symbolName, uint64_t maxVal, const Options &options = {});
  int64_t  readSU(const std::string &symbolName, unsigned nrBits, const Options &options = {});

  ByteVector readBytes(const std::string &symbolName, size_t nrBytes, const Options &options = {});

  void
  logCalculatedValue(const std::string &symbolName, int64_t value, const Options &options = {});
  void logArbitrary(const std::string &symbolName,
                    const std::string &value   = {},
                    const std::string &coding  = {},
                    const std::string &code    = {},
                    const std::string &meaning = {});

  void stashAndReplaceCurrentTreeItem(std::shared_ptr<TreeItem> newItem);
  void popTreeItem();

  [[nodiscard]] std::shared_ptr<TreeItem> getCurrentItemTree() { return this->currentTreeLevel; }

private:
  friend class SubByteReaderLoggingSubLevel;
  void addLogSubLevel(std::string name);
  void removeLogSubLevel();

  void logExceptionAndThrowError [[noreturn]] (const std::exception &ex, const std::string &when);

  std::stack<std::shared_ptr<TreeItem>> itemHierarchy;
  std::shared_ptr<TreeItem>             currentTreeLevel{};
  std::shared_ptr<TreeItem>             stashedTreeItem{};
};

// A simple wrapper for SubByteReaderLogging->addLogSubLevel /
// SubByteReaderLogging->removeLogSubLevel
class SubByteReaderLoggingSubLevel
{
public:
  SubByteReaderLoggingSubLevel() = default;
  SubByteReaderLoggingSubLevel(SubByteReaderLogging &reader, std::string name);
  ~SubByteReaderLoggingSubLevel();

private:
  SubByteReaderLogging *r{};
};

} // namespace parser::reader