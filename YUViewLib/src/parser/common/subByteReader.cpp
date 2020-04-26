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

#include "subByteReader.h"

unsigned int subByteReader::readBits(int nrBits, QString &bitsRead)
{
  int out = 0;
  int nrBitsRead = nrBits;

  // The return unsigned int is of depth 32 bits
  if (nrBits > 32)
    throw std::logic_error("Trying to read more than 32 bits at once from the bitstream.");

  while (nrBits > 0)
  {
    if (posInBuffer_bits == 8 && nrBits != 0) 
    {
      // We read all bits we could from the current byte but we need more. Go to the next byte.
      if (!gotoNextByte())
        // We are at the end of the buffer but we need to read more. Error.
        throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");
    }

    // How many bits can be gotton from the current byte?
    int curBitsLeft = 8 - posInBuffer_bits;

    int readBits; // Nr of bits to read
    int offset;   // Offset for reading from the right
    if (nrBits >= curBitsLeft)
    {
      // Read "curBitsLeft" bits
      readBits = curBitsLeft;
      offset = 0;
    }
    else 
    {
      // Read "nrBits" bits
      assert(nrBits < 8 && nrBits < curBitsLeft);
      readBits = nrBits;
      offset = curBitsLeft - nrBits;
    }

    // Shift output value so that the new bits fit
    out = out << readBits;

    char c = byteArray[posInBuffer_bytes];
    c = c >> offset;
    int mask = ((1<<readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    posInBuffer_bits += readBits;
  }

  for (int i = nrBitsRead-1; i >= 0; i--)
  {
    if (out & (1 << i))
      bitsRead.append("1");
    else
      bitsRead.append("0");
  }

  return out;
}

uint64_t subByteReader::readBits64(int nrBits, QString &bitsRead)
{
  if (nrBits > 64)
    throw std::logic_error("Trying to read more than 64 bits at once from the bitstream.");
  if (nrBits <= 32)
    return readBits(nrBits, bitsRead);

  // We just use the readBits function twice
  int lowerBits = nrBits - 32;
  int upper = readBits(32, bitsRead);
  int lower = readBits(lowerBits, bitsRead);
  uint64_t ret = (upper << lowerBits) + lower;
  return ret;
}

QByteArray subByteReader::readBytes(int nrBytes)
{
  if (skipEmulationPrevention)
    throw std::logic_error("Reading bytes with emulation prevention active is not supported.");
  if (posInBuffer_bits != 0 && posInBuffer_bits != 8)
    throw std::logic_error("When reading bytes from the bitstream, it should be byte alligned.");

  if (posInBuffer_bits == 8)
    if (!gotoNextByte())
      // We are at the end of the buffer but we need to read more. Error.
      throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");

  QByteArray retArray;
  for (int i = 0; i < nrBytes; i++)
  {
    if (posInBuffer_bytes >= (unsigned int)byteArray.size())
      throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");

    retArray.append(byteArray[posInBuffer_bytes]);
    posInBuffer_bytes++;
  }

  return retArray;
}

unsigned int subByteReader::readUE_V(QString &bitsRead, int &bit_count)
{
  int readBit = readBits(1, bitsRead);
  bit_count++;
  if (readBit == 1)
    return 0;

  // Get the length of the golomb
  int golLength = 0;
  while (readBit == 0) 
  {
    readBit = readBits(1, bitsRead);
    golLength++;
  }

  // Read "golLength" bits
  unsigned int val = readBits(golLength, bitsRead);
  // Add the exponentional part
  val += (1 << golLength)-1;

  bit_count += 2 * golLength;

  return val;
}

int subByteReader::readSE_V(QString &bitsRead, int &bit_count)
{
  int val = readUE_V(bitsRead, bit_count);
  if (val%2 == 0) 
    return -(val+1)/2;
  else
    return (val+1)/2;
}

uint64_t subByteReader::readLeb128(QString &bitsRead, int &bit_count)
{
  // We will read full bytes (up to 8)
  // The highest bit indicates if we need to read another bit. The rest of the bits is added to the counter (shifted accordingly)
  // See the AV1 reading specification
  uint64_t value = 0;
  for (int i = 0; i < 8; i++)
  {
    int leb128_byte = readBits(8, bitsRead);
    bit_count += 8;
    value |= ((leb128_byte & 0x7f) << (i*7));
    if (!(leb128_byte & 0x80))
      break;
  }
  return value;
}

uint64_t subByteReader::readUVLC(QString &bitsRead, int &bit_count)
{
  int leadingZeros = 0;
  while (1)
  {
    int done = readBits(1, bitsRead);
    bit_count += 1;
    if (done)
      break;
    leadingZeros++;
  }
  if (leadingZeros >= 32)
    return ((uint64_t)1 << 32) - 1;
  uint64_t value = readBits(leadingZeros, bitsRead);
  return value + ((uint64_t)1 << leadingZeros) - 1;
}

int subByteReader::readNS(int maxVal, QString &bitsRead, int &bit_count)
{
  // FloorLog2
  int floorVal;
  {
    int x = maxVal;
    int s = 0;
    while (x != 0)
    {
      x = x >> 1;
      s++;
    }
    floorVal = s - 1;
  }
  
  int w = floorVal + 1;
  int m = (1 << w) - maxVal;
  int v = readBits(w-1, bitsRead);
  bit_count += w-1;
  if (v < m)
    return v;
  int extra_bit = readBits(1, bitsRead);
  bit_count++;
  return (v << 1) - m + extra_bit;
}

int subByteReader::readSU(int nrBits, QString &bitsRead)
{
  int value = readBits(nrBits, bitsRead);
  int signMask = 1 << (nrBits - 1);
  if (value & signMask)
    value = value - 2 * signMask;
  return value;
}

/* Is there more data? There is no more data if the next bit is the terminating bit and all
* following bits are 0. */
bool subByteReader::more_rbsp_data()
{
  unsigned int posBytes = posInBuffer_bytes;
  unsigned int posBits  = posInBuffer_bits;
  bool terminatingBitFound = false;
  if (posBits == 8)
  {
    posBytes++;
    posBits = 0;
  }
  else
  {
    // Check the remainder of the current byte
    unsigned char c = byteArray[posBytes];
    if (c & (1 << (7-posBits)))
      terminatingBitFound = true;
    else
      return true;
    posBits++;
    while (posBits != 8)
    {
      if (c & (1 << (7-posBits)))
        // Only zeroes should follow
        return true;
      posBits++;
    }
    posBytes++;
  }
  while(posBytes < (unsigned int)byteArray.size())
  {
    unsigned char c = byteArray[posBytes];
    if (terminatingBitFound && c != 0)
      return true;
    else if (!terminatingBitFound && (c & 128))
      terminatingBitFound = true;
    else
      return true;
    posBytes++;
  }
  if (!terminatingBitFound)
    return true;
  return false;
}

/* Is there more data? If the current position in the sei_payload() syntax structure is not the position of the last (least significant, right-
   most) bit that is equal to 1 that is less than 8 * payloadSize bits from the beginning of the syntax structure (i.e.,
   the position of the payload_bit_equal_to_one syntax element), the return value of payload_extension_present( )
   is equal to TRUE.
 */
bool subByteReader::payload_extension_present()
{
  // TODO: What is the difference to this?
  return more_rbsp_data();
}

bool subByteReader::testReadingBits(int nrBits)
{
  const int curBitsLeft = 8 - posInBuffer_bits;
  const int entireBytesLeft = byteArray.size() - posInBuffer_bytes - 1;
  const int nrBitsLeftToRead = curBitsLeft + entireBytesLeft * 8;
    
  return nrBits <= nrBitsLeftToRead;
}

bool subByteReader::gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
  if (posInBuffer_bytes >= unsigned(byteArray.length()))
    throw std::out_of_range("Reading out of bounds");
  if (byteArray[posInBuffer_bytes] == (char)0)
    numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  posInBuffer_bits = 0;
  // Advance pointer
  posInBuffer_bytes++;

  if (posInBuffer_bytes >= (unsigned int)byteArray.size()) 
    // The next byte is outside of the current buffer. Error.
    return false;    

  if (skipEmulationPrevention)
  {
    if (numEmuPrevZeroBytes == 2 && byteArray[posInBuffer_bytes] == (char)3) 
    {
      // The current byte is an emulation prevention 3 byte. Skip it.
      posInBuffer_bytes++; // Skip byte

      if (posInBuffer_bytes >= (unsigned int)byteArray.size()) {
        // The next byte is outside of the current buffer. Error
        return false;
      }

      // Reset counter
      numEmuPrevZeroBytes = 0;
    }
    else if (byteArray[posInBuffer_bytes] != (char)0)
      // No zero byte. No emulation prevention 3 byte
      numEmuPrevZeroBytes = 0;
  }

  return true;
}