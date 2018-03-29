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

#ifndef ANNEXBPARSER_H
#define ANNEXBPARSER_H

#include <QList>
#include <QAbstractItemModel>
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

/* The (abstract) base class for the various types of AnnexB files (AVC, HEVC, JEM) that we can parse.
*/
class annexBParser
{
public:
  annexBParser() {};
  ~annexBParser() {};

  // How many POC's have been found in the file
  int getNumberPOCs() const { return POC_List.size(); }

  // Clear all knowledge about the bitstream.
  void clearData();

  // This function must be overloaded and parse the NAL unit header and whatever the NAL unit may contain.
  // Finally it should add the unit to the nalUnitList (if it is a parameter set or an RA point).
  virtual void parseAndAddNALUnit(int nalID, QByteArray data, quint64 curFilePos = -1) = 0;

  // Get a pointer to the nal unit model
  QAbstractItemModel *getNALUnitModel() { return &nalUnitModel; }

  // What it the framerate?
  virtual double getFramerate() const = 0;
  // What is the sequence resolution?
  virtual QSize getSequenceSizeSamples() const = 0;
  // What is the chroma format?
  virtual YUVSubsamplingType getSequenceSubsampling() const = 0;
  // What is the bit depth of the reconstruction?
  virtual int getSequenceBitDepth(Component c) const = 0;

  // When we want to seek to a specific frame number, this function return the parameter sets that you need
  // to start decoding. If file positions were set for the NAL units, the file position where decoding can begin will 
  // also be returned.
  virtual QList<QByteArray> determineSeekPoint(int iFrameNr, quint64 &filePos);

protected:
  // ----- Some nested classes that are only used in the scope of this file handler class

  // The tree item is used to feed the tree view. Each NAL unit can return a representation using TreeItems
  struct TreeItem
  {
    // Some useful constructors of new Tree items. You must at least specify a parent. The new item is atomatically added as a child 
    // of the parent.
    TreeItem(TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); }
    TreeItem(QList<QString> &data, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData = data; }
    TreeItem(const QString &name, TreeItem *parent)  { parentItem = parent; if (parent) parent->childItems.append(this); itemData.append(name); }
    TreeItem(const QString &name, int  val  , const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, bool val  , const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << (val ? "1" : "0")    << coding << code; }
    TreeItem(const QString &name, double val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, QString val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; }
    TreeItem(const QString &name, int val, const QString &coding, const QString &code, QStringList meanings, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; if (val >= meanings.length()) val = meanings.length() - 1; itemData.append(meanings.at(val)); }
    TreeItem(const QString &name, int val, const QString &coding, const QString &code, QString meaning, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; itemData.append(meaning); }
    TreeItem(const QString &name, QString val, const QString &coding, const QString &code, QString meaning, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; itemData.append(meaning); }

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
  */
  class sub_byte_reader
  {
  public:
    sub_byte_reader(const QByteArray &inArr) : p_byteArray(inArr), posInBuffer_bytes(0), posInBuffer_bits(0), p_numEmuPrevZeroBytes(0) {}
    // Read the given number of bits and return as integer. If bitsRead is true, the bits that were read are returned as a QString.
    unsigned int readBits(int nrBits, QString *bitsRead=nullptr);
    // Read an UE(v) code from the array. If given, increase bit_count with every bit read.
    int readUE_V(QString *bitsRead=nullptr, int *bit_count=nullptr);
    // Read an SE(v) code from the array
    int readSE_V(QString *bitsRead=nullptr);
    // Is there more RBSP data or are we at the end?
    bool more_rbsp_data();
    // How many full bytes were read from the reader?
    int nrBytesRead() { return (posInBuffer_bits == 0) ? posInBuffer_bytes : posInBuffer_bytes + 1; }

  protected:
    QByteArray p_byteArray;

    // Move to the next byte and look for an emulation prevention 3 byte. Remove it (skip it) if found.
    // This function is just used by the internal reading functions.
    bool p_gotoNextByte();

    int posInBuffer_bytes;   // The byte position in the buffer
    int posInBuffer_bits;    // The sub byte (bit) position in the buffer (0...7)
    int p_numEmuPrevZeroBytes; // The number of emulation prevention three bytes that were found
  };

  /* The basic NAL unit. Contains the NAL header and the file position of the unit.
  */
  struct nal_unit
  {
    nal_unit(quint64 filePos, int nal_idx) : filePos(filePos), nal_idx(nal_idx), nal_unit_type_id(-1) {}
    virtual ~nal_unit() {} // This class is meant to be derived from.

                           // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    virtual void parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) = 0;

    /// Pointer to the first byte of the start code of the NAL unit
    quint64 filePos;

    // The index of the nal within the bitstream
    int nal_idx;

    // Get the NAL header including the start code
    virtual QByteArray getNALHeader() const = 0;
    virtual bool isParameterSet() const = 0;
    virtual int  getPOC() const { return -1; }
    // Get the raw NAL unit (including start code, nal unit header and payload)
    // This only works if the payload was saved of course
    QByteArray getRawNALData() const { return getNALHeader() + nalPayload; }

    // Each nal unit (in all known standards) has a type id
    int nal_unit_type_id;

    // Optionally, the NAL unit can store it's payload. A parameter set, for example, can thusly be saved completely.
    QByteArray nalPayload;
  };

  // A list of all POCs in the sequence (in coding order). POC's don't have to be consecutive, so the only
  // way to know how many pictures are in a sequences is to keep a list of all POCs.
  QList<int> POC_List;

  // Returns false if the POC was already present int the list
  bool addPOCToList(int poc);

  // A list of nal units sorted by position in the file.
  // Only parameter sets and random access positions go in here.
  // So basically all information we need to seek in the stream and start the decoder at a certain position.
  QList<QSharedPointer<nal_unit>> nalUnitList;

};

#endif // ANNEXBPARSER_H
