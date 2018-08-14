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

#ifndef PARSERCOMMON_H
#define PARSERCOMMON_H

#include <QList>
#include <QString>
#include <QAbstractItemModel>

/* Abstract base class that prvides features which are common to all parsers
 */
class parserBase
{
public:
  parserBase() {}
  virtual ~parserBase() = 0;

  // Get a pointer to the nal unit model. The model is only filled if you call enableModel() first.
  QAbstractItemModel *getNALUnitModel() { return &nalUnitModel; }
  void enableModel();

protected:
  // ----- Some nested classes that are only used in the scope of the parser classes

  // The tree item is used to feed the tree view. Each NAL unit can return a representation using TreeItems
  struct TreeItem
  {
    // Some useful constructors of new Tree items. You must at least specify a parent. The new item is atomatically added as a child 
    // of the parent.
    TreeItem(TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); }
    TreeItem(QList<QString> &data, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData = data; }
    TreeItem(const QString &name, TreeItem *parent)  { parentItem = parent; if (parent) parent->childItems.append(this); itemData.append(name); }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, unsigned int val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, uint64_t     val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, int64_t      val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, bool         val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << (val ? "1" : "0")    << coding << code; }
    TreeItem(const QString &name, double       val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, QString      val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, QStringList meanings, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; if (val >= meanings.length()) val = meanings.length() - 1; itemData.append(meanings.at(val)); }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, QMap<int, QString> meanings, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; if (meanings.contains(val)) itemData.append(meanings.value(val)); else if (meanings.contains(-1)) itemData.append(meanings.value(-1)); }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, QString meaning, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; itemData.append(meaning); }
    TreeItem(const QString &name, QString      val, const QString &coding, const QString &code, QString meaning, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; itemData.append(meaning); }

    ~TreeItem() { qDeleteAll(childItems); }

    QList<TreeItem*> childItems;
    QList<QString> itemData;
    TreeItem *parentItem;
  };

  class NALUnitModel : public QAbstractItemModel
  {
  public:
    NALUnitModel() {}

    // The functions that must be overridden from the QAbstractItemModel
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual QModelIndex parent(const QModelIndex &index) const Q_DECL_OVERRIDE;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE { Q_UNUSED(parent); return 5; }

    // The root of the tree
    QScopedPointer<TreeItem> rootItem;
  };
  NALUnitModel nalUnitModel;

  /* This class provides the ability to read a byte array bit wise. Reading of ue(v) symbols is also supported.
   * This class can "read out" the emulation prevention bytes. This is enabled by default but can be disabled
   * if needed.
  */
  class sub_byte_reader
  {
  public:
    sub_byte_reader(const QByteArray &inArr) : byteArray(inArr), posInBuffer_bytes(0), posInBuffer_bits(0), numEmuPrevZeroBytes(0) { skipEmulationPrevention = true; }
    // Read the given number of bits and return as integer. If bitsRead is true, the bits that were read are returned as a QString.
    unsigned int readBits(int nrBits, QString *bitsRead=nullptr);
    uint64_t     readBits64(int nrBits, QString *bitsRead=nullptr);
    QByteArray   readBytes(int nrBytes);
    // Read an UE(v) code from the array. If given, increase bit_count with every bit read.
    int readUE_V(QString *bitsRead=nullptr, int *bit_count=nullptr);
    // Read an SE(v) code from the array
    int readSE_V(QString *bitsRead=nullptr);
    // Is there more RBSP data or are we at the end?
    bool more_rbsp_data();
    // Will reading of the given number of bits succeed?
    bool testReadingBits(int nrBits);
    // How many full bytes were read/are left from the reader?
    int nrBytesRead() { return (posInBuffer_bits == 0) ? posInBuffer_bytes : posInBuffer_bytes + 1; }
    int nrBytesLeft() { return byteArray.size() - posInBuffer_bytes - 1; }

    void disableEmulationPrevention() { skipEmulationPrevention = false; }

  protected:
    QByteArray byteArray;

    bool skipEmulationPrevention;

    // Move to the next byte and look for an emulation prevention 3 byte. Remove it (skip it) if found.
    // This function is just used by the internal reading functions.
    bool gotoNextByte();

    int posInBuffer_bytes;   // The byte position in the buffer
    int posInBuffer_bits;    // The sub byte (bit) position in the buffer (0...7)
    int numEmuPrevZeroBytes; // The number of emulation prevention three bytes that were found
  };

  /* This class provides the ability to write to a QByteArray on a bit basis. 
  */
  class sub_byte_writer
  {
  public:
    sub_byte_writer() { currentByte = 0; posInByte = 7; };
    // Write the bits to the output. Bits are written MSB first.
    void writeBits(int val, int nrBits);
    void writeBool(bool flag);
    void writeData(QByteArray data);
    QByteArray getByteArray();
  protected:
    QByteArray byteArray;
    char currentByte;     // The current (unfinished) byte
    int posInByte;        // The bit position in the current byte
  };
};

#endif // PARSERCOMMON_H
