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
#include <filesource/FileSource.h>

/* Extends the FileSource to read nal units from a raw AnnexB file.
 */
class FileSourceAnnexBFile : public FileSource
{
  Q_OBJECT

public:
  FileSourceAnnexBFile();
  FileSourceAnnexBFile(const QString &filePath) : FileSourceAnnexBFile() { openFile(filePath); }
  ~FileSourceAnnexBFile(){};

  bool openFile(const QString &filePath) override;

  [[nodiscard]] bool atEnd() const override;

  // --- Retrieving of data from the file ---
  // You can either read a file NAL by NAL or frame by frame. Do not mix the two interfaces.
  // TODO: We could always use the second option, right? Also for the libde265 and HM decoder this
  // should work.

  // Get the next NAL unit (everything including the start code)
  // Also return the start and end position of the NAL unit in the file so you can seek to it.
  // startEndPosInFile: The file positions of the first byte in the NAL header and the end position
  // of the last byte
  DataAndStartEndPos getNextNALUnit(bool getLastDataAgain = false);

  // Get all bytes that are needed to decode the next frame (from the given start to the given end
  // position) The data will be returned in the ISO/IEC 14496-15 format (4 bytes size followed by
  // the payload).
  QByteArray getFrameData(const FileStartEndPos &startEndFilePos);

  bool seek(int64_t pos) override;

  [[nodiscard]] uint64_t getNrBytesBeforeFirstNAL() const { return this->nrBytesBeforeFirstNAL; }

protected:
  QByteArray fileBuffer;
  int64_t    fileBufferSize{0}; ///< How many of the bytes are used? We don't resize the fileBuffer
  int64_t    bufferStartPosInFile{
      0}; ///< The byte position in the file of the start of the currently loaded buffer

  // The current position in the input buffer in bytes. This always points to the first byte of a
  // start code. So if the start code is 0001 it will point to the first byte (the first 0). If the
  // start code is 001, it will point to the first 0 here. Note: The pos may be negative if we
  // update the buffer and the start of the start code was in the previous buffer
  int64_t posInBuffer{0};

  bool updateBuffer();
  void seekToFirstNAL();

  // We will keep the last buffer in case the reader wants to get it again
  DataAndStartEndPos lastDataAndPos;

  int64_t nrBytesBeforeFirstNAL{0};
};
