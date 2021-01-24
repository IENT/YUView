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

#include "common/typedef.h"
#include <map>
#include <string>
#include <vector>

namespace parser
{

template <typename T> std::string formatArray(std::string variableName, T idx)
{
  return variableName + "[" + std::to_string(idx) + "]";
}

template <typename T1, typename T2>
std::string formatArray(std::string variableName, T1 idx1, T2 idx2)
{
  return variableName + "[" + std::to_string(idx1) + "][" + std::to_string(idx2) + "]";
}

template <typename T1, typename T2, typename T3>
std::string formatArray(std::string variableName, T1 idx1, T2 idx2, T3 idx3)
{
  return variableName + "[" + std::to_string(idx1) + "][" + std::to_string(idx2) + "][" +
         std::to_string(idx3) + "]";
}

std::string convertSliceCountsToString(const std::map<std::string, unsigned int> &sliceCounts);
std::vector<std::string> splitX26XOptionsString(const std::string str, const std::string seperator);
size_t                   getStartCodeOffset(const ByteVector &data);

} // namespace parser
