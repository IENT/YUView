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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <common/Typedef.h>

using ::testing::Bool;
using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::ValuesIn;

namespace yuviewTest
{

std::string replaceNonSupportedCharacters(const std::string &str);

template <typename T> std::string _formatArgument(T arg)
{
  if constexpr (std::is_same_v<T, std::string>)
    return replaceNonSupportedCharacters(arg);
  else if constexpr (std::is_same_v<T, const char *>)
    return std::string(arg);
  else if constexpr (std::is_same_v<T, Size>)
    return std::to_string(arg.width) + "x" + std::to_string(arg.height);
  else if constexpr (std::is_same_v<T, std::string_view>)
    return std::string(arg);
  else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
  {
    if (arg < 0)
      return "n" + std::to_string(std::abs(arg));
    else
      return std::to_string(arg);
  }
  else
    return std::to_string(arg);
}

template <typename T> std::string _formatArgument(std::optional<T> arg)
{
  if (arg)
    return _formatArgument(*arg);
  return "NA";
}

template <typename T, std::size_t N> std::string _formatArgument(std::array<T, N> values)
{
  std::ostringstream stream;
  bool               first = true;
  for (const auto value : values)
  {
    if (!first)
      stream << "_";
    stream << _formatArgument(value);
    first = false;
  }
  return stream.str();
}

template <typename T> std::string _formatArgument(std::vector<T> values)
{
  std::ostringstream stream;
  bool               first = true;
  for (const auto value : values)
  {
    if (!first)
      stream << "_";
    stream << _formatArgument(value);
    first = false;
  }
  return stream.str();
}

template <typename FirstArg, typename... Args>
std::string formatTestName(FirstArg &&firstArg, Args &&...args)
{
  std::ostringstream stream;
  stream << _formatArgument(firstArg);
  ((stream << "_" << _formatArgument(args)), ...);
  return stream.str();
}

} // namespace yuviewTest
