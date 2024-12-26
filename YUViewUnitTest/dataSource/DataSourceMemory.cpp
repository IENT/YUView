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

#include "DataSourceMemory.h"

namespace datasource::test
{

DataSourceMemory::DataSourceMemory(ByteVector &&data) : data(std::move(data))
{
  this->readingPosition = this->data.begin();
}

std::vector<InfoItem> DataSourceMemory::getInfoList() const
{
  return {};
}

bool DataSourceMemory::atEnd() const
{
  return this->readingPosition == this->data.end();
}

bool DataSourceMemory::isOk() const
{
  return true;
}

std::int64_t DataSourceMemory::getPosition() const
{
  return std::distance(this->data.begin(),
                       static_cast<ByteVector::const_iterator>(this->readingPosition));
}

std::optional<std::int64_t> DataSourceMemory::getSize() const
{
  return this->data.size();
}

void DataSourceMemory::clearFileCache()
{
}

bool DataSourceMemory::wasSourceModified() const
{
  return false;
}

void DataSourceMemory::reloadAndResetDataSource()
{
}

bool DataSourceMemory::seek(const std::int64_t pos)
{
  if (pos < 0)
    throw std::invalid_argument("Position must not be negative");

  if (static_cast<std::size_t>(pos) >= this->data.size())
    this->readingPosition = this->data.end();
  else
    this->readingPosition = this->data.begin() + pos;

  return true;
}

std::int64_t DataSourceMemory::read(ByteVector &buffer, const std::int64_t nrBytes)
{
  if (nrBytes <= 0)
    throw std::invalid_argument("Nr bytes must be > 0");

  const auto remainingBytes =
      static_cast<std::int64_t>(std::distance(this->readingPosition, this->data.end()));
  const auto nrBytesToRead = std::min(nrBytes, remainingBytes);

  if (buffer.size() != static_cast<std::size_t>(nrBytesToRead))
    buffer.resize(nrBytesToRead);

  const auto endInData = this->readingPosition + nrBytesToRead;
  std::copy(this->readingPosition, endInData, buffer.begin());
  this->readingPosition = endInData;

  return nrBytesToRead;
}

} // namespace datasource::test