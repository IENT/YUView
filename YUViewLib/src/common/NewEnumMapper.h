/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include <algorithm>
#include <array>
#include <cuchar>
#include <optional>
#include <string_view>

#include "Functions.h"

using namespace std::string_view_literals;

template <class T, std::size_t N> struct NewEnumMapper
{
  using VALUE_AND_NAME = std::pair<T, std::string_view>;

  template <typename... Args> constexpr NewEnumMapper(Args... args)
  {
    static_assert(sizeof...(Args) == N);
    this->addElementsRecursively(0, args...);
  }

  constexpr std::size_t size() const { return N; }

  constexpr std::string_view getName(const T value) const
  {
    const auto it = std::find(this->items.begin(), this->items.end(), value);
    if (it == this->items.end())
      throw std::logic_error(
          "The given type T was not registered in the mapper. All possible enums must be mapped.");
    const auto index = std::distance(this->items.begin(), it);
    return this->names.at(index);
  }

  constexpr std::optional<T> getValue(const std::string_view name) const
  {
    const auto it = std::find(this->names.begin(), this->names.end(), name);
    if (it == this->names.end())
      return {};

    const auto index = std::distance(this->names.begin(), it);
    return this->items.at(index);
  }

  constexpr std::optional<T> getValueCaseInsensitive(const std::string_view name) const
  {
    const auto compareToNameLowercase = [&name](const std::string_view str)
    {
      if (name.length() != str.length())
        return false;
      for (std::size_t i = 0; i < name.length(); ++i)
      {
        if (std::tolower(name.at(i)) != std::tolower(str.at(i)))
          return false;
      }
      return true;
    };

    const auto it = std::find_if(this->names.begin(), this->names.end(), compareToNameLowercase);
    if (it == this->names.end())
      return {};

    const auto index = std::distance(this->names.begin(), it);
    return this->items.at(index);
  }

  std::optional<T> getValueFromNameOrIndex(const std::string_view nameOrIndex) const
  {
    if (auto index = functions::toUnsigned(nameOrIndex))
      if (*index < N)
        return this->items.at(*index);

    return this->getValue(nameOrIndex);
  }

  constexpr size_t indexOf(const T value) const
  {
    const auto it = std::find(this->items.begin(), this->items.end(), value);
    if (it == this->items.end())
      throw std::logic_error(
          "The given type T was not registered in the mapper. All possible enums must be mapped.");

    const auto index = std::distance(this->items.begin(), it);
    return index;
  }

  constexpr std::optional<T> at(const size_t index) const
  {
    if (index >= N)
      return {};
    return this->items.at(index);
  }

  constexpr const std::array<T, N>                &getItems() const { return this->items; }
  constexpr const std::array<std::string_view, N> &getNames() const { return this->names; }

private:
  constexpr void addElementsRecursively(const std::size_t) {};

  template <typename TArg, typename... Args>
  constexpr void addElementsRecursively(const std::size_t index, TArg first, Args... args)
  {
    static_assert(std::is_same<VALUE_AND_NAME, TArg>());

    const auto [value, name] = first;
    this->items[index]       = value;
    this->names[index]       = name;

    addElementsRecursively(index + 1, args...);
  }

  std::array<T, N>                items{};
  std::array<std::string_view, N> names{};
};
