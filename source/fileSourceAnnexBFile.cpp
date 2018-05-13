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

#include "fileSourceAnnexBFile.h"
#include "mainwindow.h"

#define ANNEXBFILE_DEBUG_OUTPUT 0
#if ANNEXBFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB qDebug
#else
#define DEBUG_ANNEXB(fmt,...) ((void)0)
#endif

fileSourceAnnexBFile::fileSourceAnnexBFile()
{
  fileBuffer.resize(BUFFER_SIZE);
  posInBuffer = 0;
  bufferStartPosInFile = 0;
  fileBufferSize = 0;
  //numZeroBytes = 0;
  
  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);
}

fileSourceAnnexBFile::~fileSourceAnnexBFile()
{
}

// Open the file and fill the read buffer. 
bool fileSourceAnnexBFile::openFile(const QString &fileName)
{
  DEBUG_ANNEXB("fileSourceAnnexBFile::openFile fileName %s", fileName);

  // Open the input file (again)
  fileSource::openFile(fileName);

  // Fill the buffer
  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  if (fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;

  // Discard all bytes until we find a start code
  getNextNALUnit();
  return true;
}

QByteArray fileSourceAnnexBFile::getNextNALUnit(QUint64Pair *startEndPosInFile)
{
  QByteArray retArray;
  
  if (startEndPosInFile)
    startEndPosInFile->first = bufferStartPosInFile + posInBuffer;

  int nextStartCodePos = -1;
  bool startCodeFound = false;
  while (!startCodeFound)
  {
    nextStartCodePos = fileBuffer.indexOf(startCode, posInBuffer + 3);
        
    if (nextStartCodePos == -1)
    {
      // No start code found ... append all data in the current buffer.
      retArray += fileBuffer.mid(posInBuffer);
      DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::getNextNALUnit no start code found - ret size %d", retArray.size());

      if (fileBufferSize < BUFFER_SIZE)
      {
        // We are out of file and could not find a next position
        posInBuffer = BUFFER_SIZE;
        return retArray;
      }

      // Before we load the next bytes: The start code might be located at the boundary to the next buffer
      const bool lastByteZero0 = fileBuffer.at(fileBufferSize - 3) == (char)0;
      const bool lastByteZero1 = fileBuffer.at(fileBufferSize - 2) == (char)0;
      const bool lastByteZero2 = fileBuffer.at(fileBufferSize - 1) == (char)0;

      // We have to continue searching - get the next buffer
      updateBuffer();
      
      if (fileBufferSize > 2)
      {
        // Now look for the special boundary case:
        if (fileBuffer.at(0) == (char)1 && lastByteZero2 && lastByteZero1)
        {
          // Found a start code - the 1 byte is here and the two (or three) 0 bytes were in the last buffer
          startCodeFound = true;
          nextStartCodePos = lastByteZero0 ? -3 : -2;
        }
        else if (fileBuffer.at(0) == (char)0 && fileBuffer.at(1) == (char)1 && lastByteZero2)
        {
          // Found a start code - the 01 bytes are here and the one (or two) 0 bytes were in the last buffer
          startCodeFound = true;
          nextStartCodePos = lastByteZero1 ? -2 : -1;
        }
        else if (fileBuffer.at(0) == (char)0 && fileBuffer.at(1) == (char)0 && fileBuffer.at(2) == (char)1)
        {
          // Found a start code - the 001 bytes are here. Check the last byte of the last buffer
          startCodeFound = true;
          nextStartCodePos = lastByteZero2 ? -1 : 0;
        }
      }
    }
    else
    {
      // Start code found. Check if the start code is 0x000001 or 0x00000001
      startCodeFound = true;
      if (fileBuffer.at(nextStartCodePos - 1) == (char)0)
        nextStartCodePos--;
    }
  }

  // Position found
  if (startEndPosInFile)
    startEndPosInFile->second = bufferStartPosInFile + nextStartCodePos;
  retArray += fileBuffer.mid(posInBuffer, nextStartCodePos - posInBuffer);
  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::getNextNALUnit start code found - ret size %d", retArray.size());
  posInBuffer = nextStartCodePos;
  return retArray;
}

QByteArray fileSourceAnnexBFile::getFrameData(QUint64Pair startEndFilePos)
{
  QByteArray retArray;

  // We have to retrieve all data from start to end. We also have replace all startcodes by the ISO 4 byte size signaling
  uint64_t start = startEndFilePos.first;
  uint64_t end = startEndFilePos.second;

  // Seek the source file to the start position
  seek(start);

  // Retrieve NAL units (and repackage them) until we reached out end position
  while (end > bufferStartPosInFile + posInBuffer)
  {
    QByteArray nalData = getNextNALUnit();

    int headerOffset = 0;
    if (nalData.at(0) == (char)0 && nalData.at(1) == (char)0)
    {
      if (nalData.at(2) == (char)0 && nalData.at(3) == (char)1)
        headerOffset = 4;
      else if (nalData.at(2) == (char)1)
        headerOffset = 3;
    }

    const uint64_t nalSize = nalData.length() - headerOffset;
    DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::getFrameData Load NAL - size %d", nalSize);

    retArray += (char)((nalSize >> 24) & 0xff);
    retArray += (char)((nalSize >> 16) & 0xff);
    retArray += (char)((nalSize >> 8 ) & 0xff);
    retArray += (char)((nalSize      ) & 0xff);
    retArray += nalData.mid(headerOffset);
  }

  return retArray;
}

bool fileSourceAnnexBFile::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  bufferStartPosInFile += fileBufferSize;

  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  posInBuffer = 0;

  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::updateBuffer fileBufferSize %d", fileBufferSize);
  return (fileBufferSize > 0);
}

bool fileSourceAnnexBFile::seek(int64_t pos)
{
  if (!isFileOpened)
    return false;

  DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::seek ot %d", pos);
  // Seek the file and update the buffer
  srcFile.seek(pos);
  fileBufferSize = srcFile.read(fileBuffer.data(), BUFFER_SIZE);
  if (fileBufferSize == 0)
    // The file is empty of there was an error reading from the file.
    return false;
  bufferStartPosInFile = pos;
  posInBuffer = 0;

  if (pos == 0)
    // When seeking to the beginning, discard all bytes until we find a start code
    getNextNALUnit();
  else
  {
    // The position should have pointed to the first byte of a start code
    if (fileBuffer.mid(0, 3) != startCode)
    {
      DEBUG_ANNEXB("fileSourceHEVCAnnexBFile::seek could not find start code at seek position");
      return false;
    }
    // Skip the start code
    posInBuffer = 3;
  }

  return true;
}
