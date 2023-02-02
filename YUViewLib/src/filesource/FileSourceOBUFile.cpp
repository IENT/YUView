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

#include "FileSourceOBUFile.h"

#include <parser/AV1/obu_header.h>
#include <parser/common/SubByteReaderLogging.h>

#define OBUFILE_DEBUG_OUTPUT 0
#if OBUFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_OBUFILE(f) qDebug() << f
#else
#define DEBUG_OBUBFILE(f) ((void)0)
#endif

const auto BUFFERSIZE = 500'000;

using SubByteReaderLogging = parser::reader::SubByteReaderLogging;

bool FileSourceOBUFile::openFile(const QString &fileName)
{
  DEBUG_OBUBFILE("FileSourceOBUFile::openFile fileName " << fileName);

  FileSource::openFile(fileName);

  auto [firstOBUData, startEnd] = this->getNextOBU();
  if (firstOBUData.isEmpty() || firstOBUData.size() > 200)
  {
    DEBUG_OBUBFILE("Invalid data size (" << firstOBUData.size() ") for first OBU.");
    this->srcFile.close();
    return false;
  }

  return true;
}

DataAndStartEndPos FileSourceOBUFile::getNextOBU(bool getLastDataAgain)
{
  FileStartEndPos fileStartEndPos;
  fileStartEndPos.start = this->pos();
  auto data             = this->srcFile.read(10);

  auto obuHeaderAndSize = SubByteReaderLogging::convertToByteVector(data);
  if (obuHeaderAndSize.size() < 2)
  {
    DEBUG_OBUBFILE("Error reading OBU header byte from file");
    return {};
  }

  SubByteReaderLogging    reader(obuHeaderAndSize, nullptr);
  parser::av1::obu_header header;
  try
  {
    header.parse(reader);
  }
  catch (...)
  {
    DEBUG_OBUBFILE("Parsing of OBU header from bytes failed");
    return {};
  }

  if (!header.obu_has_size_field)
  {
    DEBUG_OBUBFILE("Raw OBU files must have size field");
    return {};
  }

  fileStartEndPos.end = fileStartEndPos.start + header.obu_size;

  // Todo: Read the rest of the needed bytes.

  return {data, fileStartEndPos};
}
