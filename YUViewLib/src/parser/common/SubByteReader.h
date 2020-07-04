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

#include <QByteArray>
#include <QString>

/* This class provides the ability to read a byte array bit wise. Reading of ue(v) symbols is also supported.
* This class can "read out" the emulation prevention bytes. This is enabled by default but can be disabled
* if needed.
*/
class SubByteReader
{
public:
  SubByteReader() {};
  SubByteReader(const QByteArray &inArr, unsigned int inArrOffset = 0) : byteArray(inArr), posInBuffer_bytes(inArrOffset), initialPosInBuffer(inArrOffset) {}
  
  void set_input(const QByteArray &inArr, unsigned int inArrOffset = 0) { byteArray = inArr; posInBuffer_bytes = inArrOffset; initialPosInBuffer = inArrOffset; }
  
  // Read the given number of bits and return as integer. If bitsRead is true, the bits that were read are returned as a QString.
  unsigned int readBits(int nrBits, QString &bitsRead);
  uint64_t     readBits64(int nrBits, QString &bitsRead);
  QByteArray   readBytes(int nrBytes);
  // Read an UE(v) code from the array. If given, increase bit_count with every bit read.
  unsigned int readUE_V(QString &bitsRead, int &bit_count);
  // Read an SE(v) code from the array
  int readSE_V(QString &bitsRead, int &bit_count);
  // Read an leb128 code from the array (as defined in AV1)
  uint64_t readLeb128(QString &bitsRead, int &bit_count);
  // REad an uvlc code from the array (as defined in AV1)
  uint64_t readUVLC(QString &bitsRead, int &bit_count);
  // Read a NS code from the array (as defined in AV1)
  int readNS(int maxVal, QString &bitsRead, int &bit_count);
  // Read a SU code from the array (as defined in AV1)
  int readSU(int nrBits, QString &bitsRead);

  // Is there more RBSP data or are we at the end?
  bool more_rbsp_data() const;
  bool payload_extension_present() const;
  // Will reading of the given number of bits succeed?
  bool testReadingBits(int nrBits) const;
  // How many full bytes were read/are left from the reader?
  unsigned int nrBytesRead() const { return posInBuffer_bytes - initialPosInBuffer + (posInBuffer_bits != 0 ? 1 : 0); }
  unsigned int nrBytesLeft() const { return (unsigned int)(std::max(0, byteArray.size() - int(posInBuffer_bytes) - 1)); }
  bool isByteAligned() const { return posInBuffer_bits == 0 || posInBuffer_bits == 8; }

  void disableEmulationPrevention() { skipEmulationPrevention = false; }

protected:
  QByteArray byteArray;

  bool skipEmulationPrevention {true};

  // Move to the next byte and look for an emulation prevention 3 byte. Remove it (skip it) if found.
  // This function is just used by the internal reading functions.
  bool gotoNextByte();

  unsigned int posInBuffer_bytes   {0}; // The byte position in the buffer
  unsigned int posInBuffer_bits    {0}; // The sub byte (bit) position in the buffer (0...7)
  unsigned int numEmuPrevZeroBytes {0}; // The number of emulation prevention three bytes that were found
  unsigned int initialPosInBuffer  {0}; // The position that was given when creating the sub reader
};
