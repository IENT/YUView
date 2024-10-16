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

#include "DataSourceLocalFile.h"

namespace filesource
{

DataSourceLocalFile::DataSourceLocalFile(const std::filesystem::path &filePath)
{
  this->file.open(filePath.string(), std::ios_base::in | std::ios_base::binary);
  if (this->isOk())
    this->path = filePath;
}

std::vector<InfoItem> DataSourceLocalFile::getInfoList() const
{
  if (!this->isOk())
    return {};

  std::vector<InfoItem> infoList;
  infoList.push_back(
      InfoItem({"File Path", this->path.string(), "The absolute path of the local file"}));
  if (const auto size = this->fileSize())
    infoList.push_back(InfoItem({"File Size", std::to_string(*size)}));

  return infoList;
}

bool DataSourceLocalFile::atEnd() const
{
  return this->file.eof();
}

bool DataSourceLocalFile::isOk() const
{
  if (this->file.fail())
    return this->file.eof();
  return true;
}

std::int64_t DataSourceLocalFile::position() const
{
  return this->filePosition;
}

bool DataSourceLocalFile::seek(const std::int64_t pos)
{
  if (!this->isOk())
    return false;

  this->file.clear();
  this->file.seekg(static_cast<std::streampos>(pos));
  this->filePosition = this->file.tellg();

  return this->isOk();
}

std::int64_t DataSourceLocalFile::read(ByteVector &buffer, const std::int64_t nrBytes)
{
  if (!this->isOk())
    return 0;

  const auto usize = static_cast<size_t>(nrBytes);
  if (static_cast<std::int64_t>(buffer.size()) < nrBytes)
    buffer.resize(usize);

  this->file.read(reinterpret_cast<char *>(buffer.data()), usize);

  const auto bytesRead = this->file.gcount();
  buffer.resize(bytesRead);

  this->filePosition += bytesRead;
  return static_cast<std::int64_t>(bytesRead);
}

std::optional<std::int64_t> DataSourceLocalFile::fileSize() const
{
  if (this->path.empty())
    return {};

  const auto size = std::filesystem::file_size(this->path);
  return static_cast<std::int64_t>(size);
}

[[nodiscard]] std::filesystem::path DataSourceLocalFile::filePath() const
{
  return this->path;
}

} // namespace filesource
