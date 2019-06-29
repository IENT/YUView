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

#include "parserCommon.h"

#include <QString>
#include <assert.h>

#define PARSERCOMMON_DEBUG_OUTPUT 0
#if PARSERCOMMON_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_PARSER qDebug
#else
#define DEBUG_PARSER(fmt,...) ((void)0)
#endif

using namespace parserCommon;

unsigned int sub_byte_reader::readBits(int nrBits, QString &bitsRead)
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

uint64_t sub_byte_reader::readBits64(int nrBits, QString &bitsRead)
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

QByteArray sub_byte_reader::readBytes(int nrBytes)
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

unsigned int sub_byte_reader::readUE_V(QString &bitsRead, int &bit_count)
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

int sub_byte_reader::readSE_V(QString &bitsRead, int &bit_count)
{
  int val = readUE_V(bitsRead, bit_count);
  if (val%2 == 0) 
    return -(val+1)/2;
  else
    return (val+1)/2;
}

uint64_t sub_byte_reader::readLeb128(QString &bitsRead, int &bit_count)
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

uint64_t sub_byte_reader::readUVLC(QString &bitsRead, int &bit_count)
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

int sub_byte_reader::readNS(int maxVal, QString &bitsRead, int &bit_count)
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

int sub_byte_reader::readSU(int nrBits, QString &bitsRead)
{
  int value = readBits(nrBits, bitsRead);
  int signMask = 1 << (nrBits - 1);
  if (value & signMask)
    value = value - 2 * signMask;
  return value;
}

/* Is there more data? There is no more data if the next bit is the terminating bit and all
* following bits are 0. */
bool sub_byte_reader::more_rbsp_data()
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
bool sub_byte_reader::payload_extension_present()
{
  // TODO: What is the difference to this?
  return more_rbsp_data();
}

bool sub_byte_reader::testReadingBits(int nrBits)
{
  const int curBitsLeft = 8 - posInBuffer_bits;
  const int entireBytesLeft = byteArray.size() - posInBuffer_bytes - 1;
  const int nrBitsLeftToRead = curBitsLeft + entireBytesLeft * 8;
    
  return nrBits <= nrBitsLeftToRead;
}

bool sub_byte_reader::gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
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

void sub_byte_writer::writeBits(int val, int nrBits)
{
  while(nrBits > 0)
  {
    if (posInByte == 7 && nrBits >= 8)
    {
      // We can write an entire byte at once
      const int shift = (nrBits - 8);
      char byte = (val >> shift) & 0xff;
      byteArray += byte;
      nrBits -= 8;
    }
    else
    {
      // Write bit by bit
      bool bit = val & (1 << (nrBits-1));
      if (bit)
        currentByte += (1 << posInByte);
      posInByte--;
      nrBits--;
      if (posInByte == -1)
      {
        // Write the byte out
        byteArray += currentByte;
        currentByte = 0;
        posInByte = 7;
      }
    }
  }
}

void sub_byte_writer::writeBool(bool flag)
{
  if (flag)
    currentByte += (1 << posInByte);
  posInByte--;
  if (posInByte == -1)
  {
    // Write the byte out
    byteArray += currentByte;
    currentByte = 0;
    posInByte = 7;
  }
}

void sub_byte_writer::writeData(QByteArray data)
{
  assert(posInByte == 7);
  byteArray += data;
}

QByteArray sub_byte_writer::getByteArray()
{
  if (posInByte != 7)
  {
    // Write out the remaining bits
    byteArray += currentByte;
    currentByte = 0;
    posInByte = 7;
  }
  return byteArray;
}

/// --------------- reader_helper ---------------------

void reader_helper::init(const QByteArray &inArr, TreeItem *item, QString new_sub_item_name)
{
  set_input(inArr);
  if (item)
  {
    if (new_sub_item_name.isEmpty())
      currentTreeLevel = item;
    else
      currentTreeLevel = new TreeItem(new_sub_item_name, item);
  }
  itemHierarchy.append(currentTreeLevel);
}

void reader_helper::addLogSubLevel(const QString name)
{
  assert(!name.isEmpty());
  if (itemHierarchy.last() == nullptr)
    return;
  currentTreeLevel = new TreeItem(name, itemHierarchy.last());
  itemHierarchy.append(currentTreeLevel);
}

void reader_helper::removeLogSubLevel()
{
  if (itemHierarchy.length() <= 1)
    // Don't remove the root
    return;
  itemHierarchy.removeLast();
  currentTreeLevel = itemHierarchy.last();
}

// TODO: Fixed length / variable length codes logging
bool reader_helper::readBits(int numBits, unsigned int &into, QString intoName, QString meaning)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, uint64_t &into, QString intoName, QString meaning)
{
  QString code;
  if (!readBits64_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, unsigned int &into, QString intoName, QStringList meanings)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, unsigned int &into, QString intoName, QMap<int,QString> meanings)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, unsigned int &into, QString intoName, meaning_callback_function pMeaning)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, pMeaning(into), currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx)
{
  QString code;
  unsigned int val;
  if (!readBits_catch(val, numBits, code))
    return false;
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, QString("u(v) -> u(%1)").arg(numBits), code, currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx, meaning_callback_function pMeaning)
{
  QString code;
  unsigned int val;
  if (!readBits_catch(val, numBits, code))
    return false;
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, QString("u(v) -> u(%1)").arg(numBits), code, pMeaning(val), currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, QByteArray &into, QString intoName, int idx)
{
  assert(numBits <= 8);
  QString code;
  unsigned int val;
  if (!readBits_catch(val, numBits, code))
    return false;
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, QString("u(v) -> u(%1)").arg(numBits), code, currentTreeLevel);
  return true;
}

bool reader_helper::readBits(int numBits, unsigned int &into, QMap<int, QString> intoNames)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)    
    new TreeItem(getMeaningValue(intoNames, into), into, QString("u(v) -> u(%1)").arg(numBits), code, currentTreeLevel);
  return true;
}

bool reader_helper::readZeroBits(int numBits, QString intoName)
{
  QString code;
  bool allZero = true;
  bool bitsToRead = numBits;
  while (numBits > 0)
  {
    unsigned int into;
    if (!readBits_catch(into, 1, code))
      return false;
    if (into != 0)
      allZero = false;
    numBits--;
  }
  if (currentTreeLevel)
  {
    new TreeItem(intoName, allZero ? QString("0") : QString("Not 0"), QString("u(v) -> u(%1)").arg(bitsToRead), code, currentTreeLevel);
    if (!allZero)
      addErrorMessageChildItem("The zero bits " + intoName + " must be zero");
  }
  return allZero;
}

bool reader_helper::ignoreBits(int numBits)
{
  unsigned int into;
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  return true;
}

bool reader_helper::readFlag(bool &into, QString intoName, QString meaning)
{
  QString code;
  unsigned int read_val;
  if (!readBits_catch(read_val, 1, code))
    return false;
  into = (read_val != 0);
  if (currentTreeLevel)
    new TreeItem(intoName, into, "u(1)", code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readFlag(QList<bool> &into, QString intoName, int idx, QString meaning)
{
  QString code;
  unsigned int read_val;
  if (!readBits_catch(read_val, 1, code))
    return false;
  bool val = (read_val != 0);
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, "u(1)", code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readFlag(bool &into, QString intoName, QStringList meanings)
{
  QString code;
  unsigned int read_val;
  if (!readBits_catch(read_val, 1, code))
    return false;
  into = (read_val != 0);
  if (currentTreeLevel)
    new TreeItem(intoName, into, "u(1)", code, getMeaningValue(meanings, read_val), currentTreeLevel);
  return true;
}

bool reader_helper::readUEV(unsigned int &into, QString intoName, QStringList meanings)
{
  QString code;
  int bit_count = 0;
  if (!readUEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ue(v) -> ue(%1)").arg(bit_count), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool reader_helper::readUEV(unsigned int &into, QString intoName, QString meaning)
{
  QString code;
  int bit_count = 0;
  if (!readUEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ue(v) -> ue(%1)").arg(bit_count), code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readUEV(QList<quint32> &into, QString intoName, int idx, QString meaning)
{
  QString code;
  int bit_count = 0;
  unsigned int val;
  if (!readUEV_catch(val, bit_count, code))
    return false;
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, QString("ue(v) -> ue(%1)").arg(bit_count), code, meaning, currentTreeLevel);
  return true;
}

bool reader_helper::readSEV(int &into, QString intoName, QStringList meanings)
{
  QString code;
  int bit_count = 0;
  if (!readSEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("se(v) -> se(%1)").arg(bit_count), code, getMeaningValue(meanings, (unsigned int)into), currentTreeLevel);
  return true;
}

bool reader_helper::readSEV(QList<int> into, QString intoName, int idx)
{
  QString code;
  int bit_count = 0;
  unsigned int val;
  if (!readUEV_catch(val, bit_count, code))
    return false;
  into.append(val);
  if (idx >= 0)
    intoName += QString("[%1]").arg(idx);
  if (currentTreeLevel)
    new TreeItem(intoName, val, QString("se(v) -> se(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool reader_helper::readLeb128(uint64_t &into, QString intoName)
{
  QString code;
  int bit_count = 0;
  if (!readLeb128_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("leb128(v) -> leb128(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool reader_helper::readUVLC(uint64_t &into, QString intoName)
{
  QString code;
  int bit_count = 0;
  if (!readUVLC_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("leb128(v) -> leb128(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool reader_helper::readNS(int &into, QString intoName, int maxVal)
{
  QString code;
  int bit_count = 0;
  if (!readNS_catch(into, maxVal, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ns(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool reader_helper::readSU(int &into, QString intoName, int nrBits)
{
  QString code;
  if (!readSU_catch(into, nrBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("su(%1)").arg(nrBits), code, currentTreeLevel);
  return true;
}

void reader_helper::logValue(int value, QString valueName, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, "calc", meaning, currentTreeLevel);
}

void reader_helper::logValue(int value, QString valueName, QString coding, QString code, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, coding, code, meaning, currentTreeLevel);
}

void reader_helper::logValue(QString value, QString valueName, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, "calc", meaning, currentTreeLevel);
}

void reader_helper::logInfo(QString info)
{
  if (currentTreeLevel)
    new TreeItem(info, currentTreeLevel);
}

bool reader_helper::addErrorMessageChildItem(QString errorMessage, TreeItem *item)
{
  if (item)
  {
    new TreeItem("Error", "", "", "", errorMessage, item, true);
    item->setError();
  }
  return false;
}

bool reader_helper::readBits_catch(unsigned int &into, int numBits, QString &code)
{
  try
  {
    into = sub_byte_reader::readBits(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readBits64_catch(uint64_t &into, int numBits, QString &code)
{
  try
  {
    into = sub_byte_reader::readBits64(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readUEV_catch(unsigned int &into, int &bit_count, QString &code)
{
  try
  {
    into = sub_byte_reader::readUE_V(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readSEV_catch(int &into, int &bit_count, QString &code)
{
  try
  {
    into = sub_byte_reader::readSE_V(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readLeb128_catch(uint64_t &into, int &bit_count, QString &code)
{
  try
  {
    into = sub_byte_reader::readLeb128(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readUVLC_catch(uint64_t &into, int &bit_count, QString &code)
{
  try
  {
    into = sub_byte_reader::readUVLC(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readNS_catch(int &into, int maxVal, int &bit_count, QString &code)
{
  try
  {
    into = sub_byte_reader::readNS(maxVal, code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool reader_helper::readSU_catch(int &into, int numBits, QString &code)
{
  try
  {
    into = sub_byte_reader::readSU(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

QString reader_helper::getMeaningValue(QStringList meanings, unsigned int val)
{
  if (val < (unsigned int)meanings.length())
    return meanings.at(val);
  else if (val >= (unsigned int)meanings.length() && !meanings.isEmpty())
    return meanings.last();
  return "";
}

QString reader_helper::getMeaningValue(QMap<int,QString> meanings, int val)
{
  if (meanings.contains(val))
    return meanings.value(val);
  else if (meanings.contains(-1))
    return meanings.value(-1);
  return "";
}

/// ----------------- PacketItemModel -----------------

// These are form the google material design color chooser (https://material.io/tools/color/)
QList<QColor> PacketItemModel::streamIndexColors = QList<QColor>()
  << QColor("#90caf9")  // blue (200)
  << QColor("#a5d6a7")  // green (200)
  << QColor("#ffe082")  // amber (200)
  << QColor("#ef9a9a")  // red (200)
  << QColor("#fff59d")  // yellow (200)
  << QColor("#ce93d8")  // purple (200)
  << QColor("#80cbc4")  // teal (200)
  << QColor("#bcaaa4")  // brown (200)
  << QColor("#c5e1a5")  // light green (200)
  << QColor("#1e88e5")  // blue (600)
  << QColor("#43a047")  // green (600)
  << QColor("#ffb300")  // amber (600)
  << QColor("#fdd835")  // yellow (600)
  << QColor("#8e24aa")  // purple (600)
  << QColor("#00897b")  // teal (600)
  << QColor("#6d4c41")  // brown (600)
  << QColor("#7cb342"); // light green (600)

PacketItemModel::PacketItemModel(QObject *parent) : QAbstractItemModel(parent)
{
}

PacketItemModel::~PacketItemModel()
{
}

QVariant PacketItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && rootItem != nullptr)
    return rootItem->itemData.value(section, QString());

  return QVariant();
}

QVariant PacketItemModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid())
    return QVariant();

  TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
  if (role == Qt::ForegroundRole)
  {
    if (item->isError())
      return QVariant(QBrush(QColor(255, 0, 0)));
    return QVariant(QBrush());
  }
  if (role == Qt::BackgroundRole)
  {
    if (!useColorCoding)
      return QVariant(QBrush());
    const int idx = item->getStreamIndex();
    if (idx >= 0)
      return QVariant(QBrush(streamIndexColors.at(idx % streamIndexColors.length())));
    return QVariant(QBrush());
  }
  else if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
  {
    if (index.column() == 0)
      return QVariant(item->getName(!showVideoOnly));
    else
      return QVariant(item->itemData.value(index.column()));
  }
  return QVariant();
}

QModelIndex PacketItemModel::index(int row, int column, const QModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
    return QModelIndex();

  TreeItem *parentItem;
  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  Q_ASSERT_X(parentItem != nullptr, "PacketItemModel::index", "pointer to parent is null. This must never happen");

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex PacketItemModel::parent(const QModelIndex &index) const
{
  if (!index.isValid())
    return QModelIndex();

  TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
  TreeItem *parentItem = childItem->parentItem;

  if (parentItem == rootItem.data())
    return QModelIndex();

  // Get the row of the item in the list of children of the parent item
  int row = 0;
  if (parentItem)
    row = parentItem->parentItem->childItems.indexOf(const_cast<TreeItem*>(parentItem));

  return createIndex(row, 0, parentItem);
}

int PacketItemModel::rowCount(const QModelIndex &parent) const
{
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
  {
    TreeItem *p = rootItem.data();
    return (p == nullptr) ? 0 : nrShowChildItems;
  }
  TreeItem *p = static_cast<TreeItem*>(parent.internalPointer());
  return (p == nullptr) ? 0 : p->childItems.count();
}

void PacketItemModel::setNewNumberModelItems(unsigned int n)
{
  Q_ASSERT_X(n >= nrShowChildItems, "PacketItemModel::setNewNumberModelItems", "Setting a smaller number of items.");
  unsigned int nrAddItems = n - nrShowChildItems;
  int lastIndex = nrShowChildItems;
  beginInsertRows(QModelIndex(), lastIndex, lastIndex + nrAddItems - 1);
  nrShowChildItems = n;
  endInsertRows();
}

void PacketItemModel::setUseColorCoding(bool colorCoding)
{
  if (useColorCoding == colorCoding)
    return;

  useColorCoding = colorCoding;
  emit dataChanged(QModelIndex(), QModelIndex(), QVector<int>() << Qt::BackgroundRole);
}

void PacketItemModel::setShowVideoStreamOnly(bool videoOnly)
{
  if (showVideoOnly == videoOnly)
    return;

  showVideoOnly = videoOnly;
  emit dataChanged(QModelIndex(), QModelIndex());
}

/// ------------------- FilterByStreamIndexProxyModel -----------------------------

bool FilterByStreamIndexProxyModel::filterAcceptsRow(int row, const QModelIndex &sourceParent) const
{
  if (streamIndex == -1)
  {
    DEBUG_PARSER("FilterByStreamIndexProxyModel::filterAcceptsRow %d - accepting all", row);
    return true;
  }

  TreeItem *parentItem;
  if (!sourceParent.isValid())
  {
    // Get the root item
    QAbstractItemModel *s = sourceModel();
    PacketItemModel *p = static_cast<PacketItemModel*>(s);
    if (p == nullptr)
    {
      DEBUG_PARSER("FilterByStreamIndexProxyModel::filterAcceptsRow Unable to get root item");  
      return false;
    }
    parentItem = p->getRootItem();
  }
  else
    parentItem = static_cast<TreeItem*>(sourceParent.internalPointer());
  Q_ASSERT_X(parentItem != nullptr, "PacketItemModel::index", "pointer to parent is null. This must never happen");

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem != nullptr)
  {
    DEBUG_PARSER("FilterByStreamIndexProxyModel::filterAcceptsRow item %d filter %d", childItem->getStreamIndex(), filterStreamIndex);
    return childItem->getStreamIndex() == streamIndex || childItem->getStreamIndex() == -1;
  }

  DEBUG_PARSER("FilterByStreamIndexProxyModel::filterAcceptsRow item null -> reject");
  return false;
}

void FilterByStreamIndexProxyModel::setFilterStreamIndex(int idx)
{
  streamIndex = idx;
  invalidateFilter();
}
