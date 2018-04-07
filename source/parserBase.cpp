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

#include "parserBase.h"
#include <assert.h>

QVariant parserBase::NALUnitModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && rootItem != nullptr)
    return rootItem->itemData.value(section, QString());

  return QVariant();
}

QVariant parserBase::NALUnitModel::data(const QModelIndex &index, int role) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::data " << index;

  if (!index.isValid())
    return QVariant();

  if (role != Qt::DisplayRole && role != Qt::ToolTipRole)
    return QVariant();

  TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

  return QVariant(item->itemData.value(index.column()));
}

QModelIndex parserBase::NALUnitModel::index(int row, int column, const QModelIndex &parent) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::index " << row << column << parent;

  if (!hasIndex(row, column, parent))
    return QModelIndex();

  TreeItem *parentItem;

  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  if (parentItem == nullptr)
    return QModelIndex();

  TreeItem *childItem = parentItem->childItems.value(row, nullptr);
  if (childItem)
    return createIndex(row, column, childItem);
  else
    return QModelIndex();
}

QModelIndex parserBase::NALUnitModel::parent(const QModelIndex &index) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::parent " << index;

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

int parserBase::NALUnitModel::rowCount(const QModelIndex &parent) const
{
  //qDebug() << "ileSourceHEVCAnnexBFile::rowCount " << parent;

  TreeItem *parentItem;
  if (parent.column() > 0)
    return 0;

  if (!parent.isValid())
    parentItem = rootItem.data();
  else
    parentItem = static_cast<TreeItem*>(parent.internalPointer());

  return (parentItem == nullptr) ? 0 : parentItem->childItems.count();
}

void parserBase::enableModel()
{
  if (nalUnitModel.rootItem.isNull())
    nalUnitModel.rootItem.reset(new TreeItem(QStringList() << "Name" << "Value" << "Coding" << "Code" << "Meaning", nullptr));
}

unsigned int parserBase::sub_byte_reader::readBits(int nrBits, QString *bitsRead)
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
      if (!p_gotoNextByte())
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

    char c = p_byteArray[posInBuffer_bytes];
    c = c >> offset;
    int mask = ((1<<readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    posInBuffer_bits += readBits;
  }

  if (bitsRead)
    for (int i = nrBitsRead-1; i >= 0; i--)
    {
      if (out & (1 << i))
        bitsRead->append("1");
      else
        bitsRead->append("0");
    }

  return out;
}

uint64_t parserBase::sub_byte_reader::readBits64(int nrBits, QString *bitsRead)
{
  uint64_t val = 0;
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

QByteArray parserBase::sub_byte_reader::readBytes(int nrBytes)
{
  if (skipEmulationPrevention)
    throw std::logic_error("Reading bytes with emulation prevention active is not supported.");
  if (posInBuffer_bits != 0 && posInBuffer_bits != 8)
    throw std::logic_error("When reading bytes from the bitstream, it should be byte alligned.");

  if (posInBuffer_bits == 8)
    if (!p_gotoNextByte())
      // We are at the end of the buffer but we need to read more. Error.
      throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");

  QByteArray retArray;
  for (int i = 0; i < nrBytes; i++)
  {
    if (posInBuffer_bytes >= p_byteArray.size())
      throw std::logic_error("Error while reading annexB file. Trying to read over buffer boundary.");

    retArray.append(p_byteArray[posInBuffer_bytes]);
    posInBuffer_bytes++;
  }

  return retArray;
}

int parserBase::sub_byte_reader::readUE_V(QString *bitsRead, int *bit_count)
{
  int readBit = readBits(1, bitsRead);
  if (bit_count)
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
  int val = readBits(golLength, bitsRead);
  // Add the exponentional part
  val += (1 << golLength)-1;

  if (bit_count)
    bit_count += 2 * golLength;

  return val;
}

int parserBase::sub_byte_reader::readSE_V(QString *bitsRead)
{
  int val = readUE_V(bitsRead);
  if (val%2 == 0) 
    return -(val+1)/2;
  else
    return (val+1)/2;
}

/* Is there more data? There is no more data if the next bit is the terminating bit and all
* following bits are 0. */
bool parserBase::sub_byte_reader::more_rbsp_data()
{
  int posBytes = posInBuffer_bytes;
  int posBits  = posInBuffer_bits;
  bool terminatingBitFound = false;
  if (posBits == 8)
  {
    posBytes++;
    posBits = 0;
  }
  else
  {
    // Check the remainder of the current byte
    unsigned char c = p_byteArray[posBytes];
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
  while(posBytes < p_byteArray.size())
  {
    unsigned char c = p_byteArray[posBytes];
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

bool parserBase::sub_byte_reader::p_gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
  if (p_byteArray[posInBuffer_bytes] == (char)0)
    p_numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  posInBuffer_bits = 0;
  // Advance pointer
  posInBuffer_bytes++;

  if (posInBuffer_bytes >= p_byteArray.size()) 
    // The next byte is outside of the current buffer. Error.
    return false;    

  if (skipEmulationPrevention)
  {
    if (p_numEmuPrevZeroBytes == 2 && p_byteArray[posInBuffer_bytes] == (char)3) 
    {
      // The current byte is an emulation prevention 3 byte. Skip it.
      posInBuffer_bytes++; // Skip byte

      if (posInBuffer_bytes >= p_byteArray.size()) {
        // The next byte is outside of the current buffer. Error
        return false;
      }

      // Reset counter
      p_numEmuPrevZeroBytes = 0;
    }
    else if (p_byteArray[posInBuffer_bytes] != (char)0)
      // No zero byte. No emulation prevention 3 byte
      p_numEmuPrevZeroBytes = 0;
  }

  return true;
}