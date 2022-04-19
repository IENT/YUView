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

#include <common/Typedef.h>

#include <optional>

namespace functions
{

// Get the optimal thread count (QThread::optimalThreadCount()-1) or at least 1
// so that one thread is "reserved" for the main GUI. I don't know if this is optimal.
unsigned int getOptimalThreadCount();

// Returns the size of system memory in megabytes.
// This function is thread safe and inexpensive to call.
unsigned int systemMemorySizeInMB();

// These are the names of the supported themes
QStringList getThemeNameList();
// Get the name of the theme in the resource file that we will load
QString getThemeFileName(QString themeName);
// For the given theme, return the primary colors to replace.
// In the qss file, we can use tags, which will be replaced by these colors. The tags are:
// #backgroundColor, #activeColor, #inactiveColor, #highlightColor
// The values to replace them by are returned in this order.
QStringList getThemeColors(QString themeName);

// Format the data size as a huma readable string. If isBits is set, assumes bits, oterwise bytes.
// From Qt 5.10 there is a built in function (QLocale::formattedDataSize). But we want to be 5.9
// compatible.
QString formatDataSize(double size, bool isBits = false);

QStringList toQStringList(const std::vector<std::string> &stringVec);
std::string toLower(std::string str);

inline std::string boolToString(bool b)
{
  return b ? "True" : "False";
}

template <typename T> unsigned clipToUnsigned(T val)
{
  static_assert(std::is_signed<T>::value, "T must must be a signed type");
  if (val < 0)
    return 0;
  return unsigned(val);
}

template <typename T, typename R> inline T clip(const T val, const R min, const R max)
{
  return (val < min) ? min : (val > max) ? max : val;
}

template <typename T, typename R> unsigned clip(T val, Range<R> range)
{
  return (val < range.min) ? range.min : (val > range.max) ? range.max : val;
}

std::optional<unsigned long> toUnsigned(const std::string &text);

} // namespace functions
