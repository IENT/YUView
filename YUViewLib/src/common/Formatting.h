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

#include "Typedef.h"

#include <sstream>
#include <vector>

template <typename T>
std::ostream &operator<<(std::ostream &stream, const std::pair<T, T> &typePair)
{
  stream << "(" << typePair.first << ", " << typePair.second << ")";
  return stream;
}

template <typename T> std::string to_string(const std::pair<T, T> &typePair)
{
  std::ostringstream stream;
  stream << typePair;
  return stream.str();
}

template <typename T> std::ostream &operator<<(std::ostream &stream, const std::vector<T> vec)
{
  stream << "[";
  for (auto it = vec.begin(); it != vec.end(); it++)
  {
    if (it != vec.begin())
      stream << ", ";
    stream << (*it);
  }
  stream << "]";
  return stream;
}

template <typename T> std::string to_string(const std::vector<T> vec)
{
  std::ostringstream stream;
  stream << vec;
  return stream.str();
}

static std::ostream &operator<<(std::ostream &stream, const Size &size)
{
  stream << size.width << "x" << size.height;
  return stream;
}

inline std::string to_string(const Size &size)
{
  std::ostringstream stream;
  stream << size;
  return stream.str();
}

inline std::string to_string(const bool b)
{
  return b ? "True" : "False";
}

template <typename T>
static std::ostream &operator<<(std::ostream &stream, const std::optional<T> &opt)
{
  if (opt)
    stream << opt;
  else
    stream << "NA";
  return stream;
}

template <typename T> inline std::string to_string(const std::optional<T> &opt)
{
  std::ostringstream stream;
  stream << opt;
  return stream.str();
}

inline std::string stringReplaceAll(std::string str, char value, char replacement)
{
  std::replace(str.begin(), str.end(), value, replacement);
  return str;
}

inline std::string
stringReplaceAll(std::string str, std::initializer_list<char> values, char replacement)
{
  std::replace_if(
      str.begin(),
      str.end(),
      [&values](const char value)
      { return std::find(values.begin(), values.end(), value) != values.end(); },
      replacement);
  return str;
}
