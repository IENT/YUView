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

#include <common/Functions.h>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/* This class implement mapping of "enum class" values to and from names (string).
 */
template <typename T> class EnumMapper
{
public:
  enum class StringType
  {
    Name,
    Text,
    NameOrIndex
  };
  struct Entry
  {
    Entry(T value, std::string name) : value(value), name(name) {}
    Entry(T value, std::string name, std::string text) : value(value), name(name), text(text) {}
    T           value;
    std::string name;
    std::string text;
  };

  using EntryVector = std::vector<Entry>;

  EnumMapper() = default;
  EnumMapper(const EntryVector &entryVector) : entryVector(entryVector){};

  std::optional<T> getValue(std::string name, StringType stringType = StringType::Name) const
  {
    if (stringType == StringType::NameOrIndex)
      if (auto index = functions::toUnsigned(name))
        return this->at(*index);

    for (const auto &entry : this->entryVector)
    {
      if ((stringType == StringType::Name && entry.name == name) ||
          (stringType == StringType::NameOrIndex && entry.text == name) ||
          (stringType == StringType::Text && entry.text == name))
        return entry.value;
    }
    return {};
  }

  std::optional<T> getValueCaseInsensitive(std::string name,
                                           StringType  stringType = StringType::Name) const
  {
    if (stringType == StringType::NameOrIndex)
      if (auto index = functions::toUnsigned(name))
        return this->at(*index);

    name = functions::toLower(name);
    for (const auto &entry : this->entryVector)
    {
      if ((stringType == StringType::Name && functions::toLower(entry.name) == name) ||
          (stringType == StringType::NameOrIndex && functions::toLower(entry.text) == name) ||
          (stringType == StringType::Text && functions::toLower(entry.text) == name))
        return entry.value;
    }
    return {};
  }

  std::string getName(T value) const
  {
    for (const auto &entry : this->entryVector)
      if (entry.value == value)
        return entry.name;
    throw std::logic_error(
        "The given type T was not registered in the mapper. All possible enums must be mapped.");
  }

  std::string getText(T value) const
  {
    for (const auto &entry : this->entryVector)
      if (entry.value == value)
        return entry.text;
    throw std::logic_error(
        "The given type T was not registered in the mapper. All possible enums must be mapped.");
  }

  size_t indexOf(T value) const
  {
    for (size_t i = 0; i < this->entryVector.size(); i++)
      if (this->entryVector.at(i).value == value)
        return i;
    throw std::logic_error(
        "The given type T was not registered in the mapper. All possible enums must be mapped.");
  }

  std::optional<T> at(size_t index) const
  {
    if (index >= this->entryVector.size())
      return {};
    return this->entryVector.at(index).value;
  }

  std::vector<T> getEnums() const
  {
    std::vector<T> m;
    for (const auto &entry : this->entryVector)
      m.push_back(entry.value);
    return m;
  }

  std::vector<std::string> getNames() const
  {
    std::vector<std::string> l;
    for (const auto &entry : this->entryVector)
      l.push_back(entry.name);
    return l;
  }

  std::vector<std::string> getTextEntries() const
  {
    std::vector<std::string> l;
    for (const auto &entry : this->entryVector)
      l.push_back(entry.text);
    return l;
  }

  size_t size() const { return this->entryVector.size(); }

  const EntryVector &entries() const { return this->entryVector; }

private:
  EntryVector entryVector;
};
