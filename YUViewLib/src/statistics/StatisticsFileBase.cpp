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

#include "StatisticsFileBase.h"

using namespace std::string_view_literals;

namespace stats
{

namespace
{

std::string sortingToString(StatisticsFileBase::ParsingInfo::FileSorting fileSorting)
{
  using FileSorting = StatisticsFileBase::ParsingInfo::FileSorting;
  if (fileSorting == FileSorting::SortedByPOC)
    return "By POC";
  return "By Type";
}

} // namespace

StatisticsFileBase::StatisticsFileBase(const std::string &filename)
{
  this->file.openFile(filename);
  if (!this->file.isOk())
    this->parsingInfo.errorMessage = "Error opening file " + filename;
}

InfoData StatisticsFileBase::getInfo() const
{
  InfoData info("Statistics File info");

  for (const auto &infoItem : this->file.getFileInfoList())
    info.items.append(infoItem);
  info.items.append(InfoItem("Sorting"sv, sortingToString(this->parsingInfo.fileSorting)));
  info.items.append(
      InfoItem("Parsing:", std::to_string(this->parsingInfo.parsingProgress) + "..."));
  if (this->parsingInfo.pocWithDataOutsideOfFrame)
    info.items.append(InfoItem("Warning",
                               "A block in frame " +
                                   std::to_string(*this->parsingInfo.pocWithDataOutsideOfFrame) +
                                   " is outside of the given size of the statistics."));
  if (this->parsingInfo.errorMessage)
    info.items.append(InfoItem("Parsing Error:", *this->parsingInfo.errorMessage));

  return info;
}

} // namespace stats
