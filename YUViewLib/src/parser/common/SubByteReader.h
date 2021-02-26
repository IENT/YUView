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

#include <algorithm>
#include <string>
#include <tuple>

#include "common/typedef.h"

namespace parser
{

/* This class provides the ability to read a byte array bit wise. Reading of ue(v) symbols is also
 * supported. This class can "read out" the emulation prevention bytes. This is enabled by default
 * but can be disabled if needed.
 */
class SubByteReader
{
public:
  SubByteReader() = default;
  SubByteReader(const ByteVector &inArr, size_t inArrOffset = 0);

  [[nodiscard]] bool more_rbsp_data() const;
  [[nodiscard]] bool byte_aligned() const;

  [[nodiscard]] bool payload_extension_present() const;
  [[nodiscard]] bool canReadBits(unsigned nrBits) const;

  [[nodiscard]] size_t nrBitsRead() const;
  [[nodiscard]] size_t nrBytesRead() const;
  [[nodiscard]] size_t nrBytesLeft() const;

  [[nodiscard]] ByteVector peekBytes(unsigned nrBytes) const;

  void disableEmulationPrevention() { skipEmulationPrevention = false; }

protected:
  std::tuple<uint64_t, std::string>   readBits(size_t nrBits);
  std::tuple<ByteVector, std::string> readBytes(size_t nrBytes);

  std::tuple<uint64_t, std::string> readUE_V();
  std::tuple<int64_t, std::string>  readSE_V();
  std::tuple<uint64_t, std::string> readLEB128();
  std::tuple<uint64_t, std::string> readUVLC();
  std::tuple<uint64_t, std::string> readNS(uint64_t maxVal);
  std::tuple<int64_t, std::string>  readSU(unsigned nrBits);

  ByteVector byteVector;

  bool skipEmulationPrevention{true};

  // Move to the next byte and look for an emulation prevention 3 byte. Remove it (skip it) if
  // found. This function is just used by the internal reading functions.
  bool gotoNextByte();

  size_t posInBufferBytes{0};    // The byte position in the buffer
  size_t posInBufferBits{0};     // The sub byte (bit) position in the buffer (0...7)
  size_t numEmuPrevZeroBytes{0}; // The number of emulation prevention three bytes that were found
  size_t initialPosInBuffer{0};  // The position that was given when creating the sub reader
};

} // namespace parser