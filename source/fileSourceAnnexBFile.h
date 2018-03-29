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

#ifndef FILESOURCEANNEXBFILE_H
#define FILESOURCEANNEXBFILE_H

#include <QAbstractItemModel>
#include "annexBParser.h"
#include "fileSource.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

#define BUFFER_SIZE 40960

/* This class can perform basic NAL unit reading from a bitstream.
 * Basically it understands that this is a binary file where each unit starts with a start code (0x0000001)
*/
class fileSourceAnnexBFile : public fileSource
{
  Q_OBJECT

public:
  fileSourceAnnexBFile();
  ~fileSourceAnnexBFile();

  // Open the given file. If another file is given, 
  virtual bool openFile(const QString &filePath) Q_DECL_OVERRIDE { return openFile(filePath, false); }
  virtual bool openFile(const QString &filePath, bool saveAllUnits, fileSourceAnnexBFile *otherFile=nullptr);

  // How many POC's have been found in the file
  int getNumberPOCs() const { return parser->getNumberPOCs(); }

  // Is the file at the end?
  virtual bool atEnd() const Q_DECL_OVERRIDE { return fileBufferSize == 0; }

  // Seek to the first byte of the payload data of the next NAL unit
  // Return false if not successfull (eg. file ended)
  bool seekToNextNALUnit();

  // Get the remaining bytes in the NAL unit or maxBytes (if set).
  // This function might also return less than maxBytes if a NAL header is encountered before reading maxBytes bytes.
  // Or: do getCurByte(), gotoNextByte until we find a new start code.
  QByteArray getRemainingNALBytes(int maxBytes=-1);

  // Calculate the closest random access point (RAP) before the given frame number.
  // Return the frame number of that random access point.
  int getClosestSeekableFrameNumber(int frameIdx) const;

  // Seek the file to the given frame number. The given frame number has to be a random 
  // access point. We can start decoding the file from here. Use getClosestSeekableFrameNumber to find a random access point.
  // Returns the active parameter sets as a byte array. This has to be given to the decoder first.
  virtual QList<QByteArray> seekToFrameNumber(int iFrameNr);
  
  // Move the file to the next byte. Update the buffer if necessary.
  // Return false if the operation failed.
  bool gotoNextByte();

  // Get the current byte in the buffer
  char getCurByte() const { return fileBuffer.at(posInBuffer); }

  // Get if the current position is the one byte of a start code
  bool curPosAtStartCode() const { return numZeroBytes >= 2 && getCurByte() == (char)1; }

  // The current absolut position in the file (byte precise)
  quint64 tell() const { return bufferStartPosInFile + posInBuffer; }

  // Read the remaining bytes from the buffer and return them. Then load the next buffer.
  QByteArray getRemainingBuffer_Update();

  // Get the bytes of the next NAL unit;
  QByteArray getNextNALUnit();

protected:
  
  // Buffers to access the binary file
  QByteArray   fileBuffer;
  quint64      fileBufferSize;
  unsigned int posInBuffer;	         ///< The current position in the input buffer in bytes
  quint64      bufferStartPosInFile; ///< The byte position in the file of the start of the currently loaded buffer
  int          numZeroBytes;         ///< The number of zero bytes that occured. (This will be updated by gotoNextByte() and seekToNextNALUnit()

  // A pointer to the parser. This can be any specific annexB parser (AVC, HEVC, JEM ...)
  // This file format always has a parser because we need it to determine the POCs and the random access points.
  QScopedPointer<annexBParser> parser;

  // The start code pattern
  QByteArray startCode;

  // Scan the file. We don't know the specific type of bitstream so we will just look for start-codes
  // and pass all the actual data to the annexBParser.
  bool scanFileForNalUnits(bool saveAllUnits);

  // load the next buffer
  bool updateBuffer();

  // Seek the file to the given byte position. Update the buffer.
  bool seekToFilePos(quint64 pos);
};

#endif //FILESOURCEANNEXBFILE_H

