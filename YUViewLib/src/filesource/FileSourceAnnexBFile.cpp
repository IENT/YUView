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

#include "FileSourceAnnexBFile.h"

#define ANNEXBFILE_DEBUG_OUTPUT 0
#if ANNEXBFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXBFILE(f) qDebug() << f
#else
#define DEBUG_ANNEXBFILE(f) ((void)0)
#endif

const auto BUFFERSIZE = 500000;
const auto STARTCODE  = QByteArrayLiteral("\x00\x00\x01");

FileSourceAnnexBFile::FileSourceAnnexBFile()
{
  this->fileBuffer.resize(BUFFERSIZE);
}

// Open the file and fill the read buffer.
bool FileSourceAnnexBFile::openFile(const QString &fileName)
{
  DEBUG_ANNEXBFILE("FileSourceAnnexBFile::openFile fileName " << fileName);

  // Open the input file (again)
  FileSource::openFile(fileName);

  // Fill the buffer
  this->fileBufferSize = srcFile.read(this->fileBuffer.data(), BUFFERSIZE);
  if (this->fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;

  // Discard all bytes until we find a start code
  this->seekToFirstNAL();
  return true;
}

bool FileSourceAnnexBFile::atEnd() const
{
  return this->fileBufferSize < BUFFERSIZE && this->posInBuffer >= int64_t(this->fileBufferSize);
}

void FileSourceAnnexBFile::seekToFirstNAL()
{
  auto nextStartCodePos = this->fileBuffer.indexOf(STARTCODE);
  if (nextStartCodePos < 0)
    // The first buffer does not contain a start code. This is very unusual. Use the normal
    // getNextNALUnit to seek
    this->getNextNALUnit();
  else
  {
    // For 0001 or 001 point to the first 0 byte
    if (nextStartCodePos > 0 && this->fileBuffer.at(nextStartCodePos - 1) == (char)0)
      this->posInBuffer = nextStartCodePos - 1;
    else
      this->posInBuffer = nextStartCodePos;
  }

  assert(this->posInBuffer >= 0);
  this->nrBytesBeforeFirstNAL = this->bufferStartPosInFile + uint64_t(this->posInBuffer);
}

QByteArray FileSourceAnnexBFile::getNextNALUnit(bool        getLastDataAgain,
                                                pairUint64 *startEndPosInFile)
{
  if (getLastDataAgain)
    return this->lastReturnArray;

  this->lastReturnArray.clear();

  if (startEndPosInFile)
    startEndPosInFile->first = this->bufferStartPosInFile + uint64_t(this->posInBuffer);

  int  nextStartCodePos = -1;
  int  searchOffset     = 3;
  bool startCodeFound   = false;
  while (!startCodeFound)
  {
    if (this->posInBuffer < 0)
    {
      // Part of the start code was in the last buffer (see special boundary cases below). Add those
      // parts.
      const auto nrZeroBytesMissing = std::abs(this->posInBuffer);
      this->lastReturnArray.append(nrZeroBytesMissing, char(0));
    }
    nextStartCodePos = this->fileBuffer.indexOf(STARTCODE, this->posInBuffer + searchOffset);

    if (nextStartCodePos < 0 || (uint64_t)nextStartCodePos > this->fileBufferSize)
    {
      // No start code found ... append all data in the current buffer.
      this->lastReturnArray +=
          this->fileBuffer.mid(this->posInBuffer, this->fileBufferSize - this->posInBuffer);
      DEBUG_ANNEXBFILE("FileSourceHEVCAnnexBFile::getNextNALUnit no start code found - ret size "
                       << this->lastReturnArray.size());

      if (this->fileBufferSize < BUFFERSIZE)
      {
        // We are out of file and could not find a next position
        this->posInBuffer = BUFFERSIZE;
        if (startEndPosInFile)
          startEndPosInFile->second = this->bufferStartPosInFile + this->fileBufferSize - 1;
        return this->lastReturnArray;
      }

      // Before we load the next bytes: The start code might be located at the boundary to the next
      // buffer
      const auto lastByteZero0 = this->fileBuffer.at(this->fileBufferSize - 3) == (char)0;
      const auto lastByteZero1 = this->fileBuffer.at(this->fileBufferSize - 2) == (char)0;
      const auto lastByteZero2 = this->fileBuffer.at(this->fileBufferSize - 1) == (char)0;

      // We have to continue searching - get the next buffer
      updateBuffer();

      if (this->fileBufferSize > 2)
      {
        // Now look for the special boundary case:
        if (this->fileBuffer.at(0) == (char)1 && lastByteZero2 && lastByteZero1)
        {
          // Found a start code - the 1 byte is here and the two (or three) 0 bytes were in the last
          // buffer
          startCodeFound   = true;
          nextStartCodePos = lastByteZero0 ? -3 : -2;
          this->lastReturnArray.chop(lastByteZero0 ? 3 : 2);
        }
        else if (this->fileBuffer.at(0) == (char)0 && this->fileBuffer.at(1) == (char)1 &&
                 lastByteZero2)
        {
          // Found a start code - the 01 bytes are here and the one (or two) 0 bytes were in the
          // last buffer
          startCodeFound   = true;
          nextStartCodePos = lastByteZero1 ? -2 : -1;
          this->lastReturnArray.chop(lastByteZero0 ? 1 : 1);
        }
        else if (this->fileBuffer.at(0) == (char)0 && this->fileBuffer.at(1) == (char)0 &&
                 this->fileBuffer.at(2) == (char)1)
        {
          // Found a start code - the 001 bytes are here. Check the last byte of the last buffer
          startCodeFound   = true;
          nextStartCodePos = lastByteZero2 ? -1 : 0;
          if (lastByteZero2)
            this->lastReturnArray.chop(1);
        }
      }

      searchOffset = 0;
    }
    else
    {
      // Start code found. Check if the start code is 001 or 0001
      startCodeFound = true;
      if (this->fileBuffer.at(nextStartCodePos - 1) == (char)0)
        nextStartCodePos--;
    }
  }

  // Position found
  if (startEndPosInFile)
    startEndPosInFile->second = this->bufferStartPosInFile + nextStartCodePos;
  if (nextStartCodePos > int(this->posInBuffer))
    this->lastReturnArray +=
        this->fileBuffer.mid(this->posInBuffer, nextStartCodePos - this->posInBuffer);
  this->posInBuffer = nextStartCodePos;
  DEBUG_ANNEXBFILE("FileSourceAnnexBFile::getNextNALUnit start code found - ret size "
                   << this->lastReturnArray.size());
  return this->lastReturnArray;
}

QByteArray FileSourceAnnexBFile::getFrameData(pairUint64 startEndFilePos)
{
  // Get all data for the frame (all NAL units in the raw format with start codes).
  // We don't need to convert the format to the mp4 ISO format. The ffmpeg decoder can also accept
  // raw NAL units. When the extradata is set as raw NAL units, the AVPackets must also be raw NAL
  // units.
  QByteArray retArray;

  auto start = startEndFilePos.first;
  auto end   = startEndFilePos.second;

  // Seek the source file to the start position
  this->seek(start);

  // Retrieve NAL units (and repackage them) until we reached out end position
  while (end > this->bufferStartPosInFile + this->posInBuffer)
  {
    auto nalData = getNextNALUnit();

    int headerOffset = 0;
    if (nalData.at(0) == (char)0 && nalData.at(1) == (char)0)
    {
      if (nalData.at(2) == (char)0 && nalData.at(3) == (char)1)
        headerOffset = 4;
      else if (nalData.at(2) == (char)1)
        headerOffset = 3;
    }
    assert(headerOffset > 0);
    if (headerOffset == 3)
      retArray.append((char)0);

    DEBUG_ANNEXBFILE("FileSourceAnnexBFile::getFrameData Load NAL - size " << nalData.length());
    retArray += nalData;
  }

  return retArray;
}

bool FileSourceAnnexBFile::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  this->bufferStartPosInFile += this->fileBufferSize;

  this->fileBufferSize = srcFile.read(this->fileBuffer.data(), BUFFERSIZE);
  this->posInBuffer    = 0;

  DEBUG_ANNEXBFILE("FileSourceAnnexBFile::updateBuffer this->fileBufferSize "
                   << this->fileBufferSize);
  return (this->fileBufferSize > 0);
}

bool FileSourceAnnexBFile::seek(int64_t pos)
{
  if (!isFileOpened)
    return false;

  DEBUG_ANNEXBFILE("FileSourceAnnexBFile::seek to " << pos);
  // Seek the file and update the buffer
  srcFile.seek(pos);
  this->fileBufferSize = srcFile.read(this->fileBuffer.data(), BUFFERSIZE);
  if (this->fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;
  this->bufferStartPosInFile = pos;
  this->posInBuffer          = 0;

  if (pos == 0)
    this->seekToFirstNAL();
  else
  {
    // Check if we are at a start code position (001 or 0001)
    if (this->fileBuffer.at(0) == (char)0 && this->fileBuffer.at(1) == (char)0 &&
        this->fileBuffer.at(2) == (char)0 && this->fileBuffer.at(3) == (char)1)
      return true;
    if (this->fileBuffer.at(0) == (char)0 && this->fileBuffer.at(1) == (char)0 &&
        this->fileBuffer.at(2) == (char)1)
      return true;

    DEBUG_ANNEXBFILE("FileSourceAnnexBFile::seek could not find start code at seek position");
    return false;
  }

  return true;
}
