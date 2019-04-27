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

#include <QBrush>
#include <QByteArray>
#include <QList>
#include <QSortFilterProxyModel>
#include <QString>

namespace parserCommon 
{
  /* This class provides the ability to read a byte array bit wise. Reading of ue(v) symbols is also supported.
    * This class can "read out" the emulation prevention bytes. This is enabled by default but can be disabled
    * if needed.
    */
  class sub_byte_reader
  {
  public:
    sub_byte_reader() {};
    sub_byte_reader(const QByteArray &inArr, unsigned int inArrOffset = 0) : byteArray(inArr), posInBuffer_bytes(inArrOffset), initialPosInBuffer(inArrOffset) {}
    
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
    bool more_rbsp_data();
    bool payload_extension_present();
    // Will reading of the given number of bits succeed?
    bool testReadingBits(int nrBits);
    // How many full bytes were read/are left from the reader?
    unsigned int nrBytesRead() { return posInBuffer_bytes - initialPosInBuffer + (posInBuffer_bits != 0 ? 1 : 0); }
    unsigned int nrBytesLeft() { return std::min((unsigned int)0, byteArray.size() - posInBuffer_bytes - 1); }

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

  // The tree item is used to feed the tree view. Each NAL unit can return a representation using TreeItems
  class TreeItem
  {
  public:
    // Some useful constructors of new Tree items. You must at least specify a parent. The new item is atomatically added as a child 
    // of the parent.
    TreeItem(TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); }
    TreeItem(QList<QString> &data, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData = data; }
    TreeItem(const QString &name, TreeItem *parent)  { parentItem = parent; if (parent) parent->childItems.append(this); itemData.append(name); }
    TreeItem(const QString &name, int          val, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val); }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, unsigned int val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, uint64_t     val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, int64_t      val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, bool         val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << (val ? "1" : "0")    << coding << code; }
    TreeItem(const QString &name, double       val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; }
    TreeItem(const QString &name, QString      val, const QString &coding, const QString &code, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; }
    TreeItem(const QString &name, int          val, const QString &coding, const QString &code, QString meaning, TreeItem *parent) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << QString::number(val) << coding << code; itemData.append(meaning); }
    TreeItem(const QString &name, QString      val, const QString &coding, const QString &code, QString meaning, TreeItem *parent, bool isError=false) { parentItem = parent; if (parent) parent->childItems.append(this); itemData << name << val << coding << code; itemData.append(meaning); setError(isError); }

    ~TreeItem() { qDeleteAll(childItems); }
    void setError(bool isError = true) { error = isError; }
    bool isError()                     { return error; }

    QString getName(bool showStreamIndex) const { QString r = (showStreamIndex && streamIndex != -1) ? QString("Stream %1 - ").arg(streamIndex) : ""; if (itemData.count() > 0) r += itemData[0]; return r; }

    QList<TreeItem*> childItems;
    QList<QString> itemData;
    TreeItem *parentItem { nullptr };

    int getStreamIndex() { if (streamIndex >= 0) return streamIndex; if (parentItem) return parentItem->getStreamIndex(); return -1; }
    void setStreamIndex(int idx) { streamIndex = idx; }

  private:
    bool error { false };
    // This is set for the first layer items in case of AVPackets
    int streamIndex { -1 };
  };

  typedef QString (*meaning_callback_function)(unsigned int);

  // This is a wrapper around the sub_byte_reader that adds the functionality to log the read symbold to TreeItems
  class reader_helper : protected parserCommon::sub_byte_reader
  {
  public:
    reader_helper() : sub_byte_reader() {};
    reader_helper(const QByteArray &inArr, TreeItem *item, QString new_sub_item_name = "") : sub_byte_reader(inArr) { init(inArr, item, new_sub_item_name); }

    void init(const QByteArray &inArr, TreeItem *item, QString new_sub_item_name = "");

    // Add another hierarchical log level to the tree or go back up. Don't call these directly but use the reader_sub_level wrapper.
    void addLogSubLevel(QString name);
    void removeLogSubLevel();

    bool readBits(int numBits, unsigned int &into, QString intoName, QString meaning = "");
    bool readBits(int numBits, uint64_t     &into, QString intoName, QString meaning = "");
    bool readBits(int numBits, unsigned int &into, QString intoName, QStringList meanings);
    bool readBits(int numBits, unsigned int &into, QString intoName, QMap<int,QString> meanings);
    bool readBits(int numBits, unsigned int &into, QString intoName, meaning_callback_function pMeaning);
    bool readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx);
    bool readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx, meaning_callback_function pMeaning);
    bool readBits(int numBits, QByteArray &into, QString intoName, int idx);
    bool readBits(int numBits, unsigned int &into, QMap<int, QString> intoNames);
    bool readZeroBits(int numBits, QString intoName);
    bool ignoreBits(int numBits);

    bool readFlag(bool &into, QString intoName, QString meaning = "");
    bool readFlag(QList<bool> &into, QString intoName, int idx, QString meaning = "");
    bool readFlag(bool &into, QString intoName, QStringList meanings);
    
    bool readUEV(unsigned int   &into, QString intoName, QStringList meanings = QStringList());
    bool readUEV(unsigned int   &into, QString intoName, QString meaning);
    bool readUEV(QList<quint32> &into, QString intoName, int idx, QString meaning = "");
    bool readSEV(          int  &into, QString intoName, QStringList meanings = QStringList());
    bool readSEV(    QList<int>  into, QString intoName, int idx);
    bool readLeb128(uint64_t &into, QString intoName);
    bool readUVLC(uint64_t &into, QString intoName);
    bool readNS(int &into, QString intoName, int maxVal);
    bool readSU(int &into, QString intoName, int numBits);

    void logValue(int value, QString valueName, QString meaning = "");
    void logValue(int value, QString valueName, QStringList meanings) { logValue(value, valueName, getMeaningValue(meanings, value)); }
    void logValue(int value, QString valueName, QMap<int,QString> meanings) { logValue(value, valueName, getMeaningValue(meanings, value)); }
    void logValue(int value, QString valueName, QString coding, QString code, QString meaning);
    void logValue(QString value, QString valueName, QString meaning = "");
    void logInfo(QString info);

    bool addErrorMessageChildItem(QString msg) { return addErrorMessageChildItem(msg, currentTreeLevel); }
    static bool addErrorMessageChildItem(QString msg, TreeItem *item);

    TreeItem *getCurrentItemTree() { return currentTreeLevel; }

    // Some functions passed thourgh from the sub_byte_reader
    bool          more_rbsp_data()             { return sub_byte_reader::more_rbsp_data();             }
    bool          payload_extension_present()  { return sub_byte_reader::payload_extension_present();  }
    unsigned int  nrBytesRead()                { return sub_byte_reader::nrBytesRead();                }
    unsigned int  nrBytesLeft()                { return sub_byte_reader::nrBytesLeft();                }
    bool          testReadingBits(int nrBits)  { return sub_byte_reader::testReadingBits(nrBits);      }
    void          disableEmulationPrevention() {        sub_byte_reader::disableEmulationPrevention(); }
    QByteArray    readBytes(int nrBytes)       { return sub_byte_reader::readBytes(nrBytes);           }
  protected:
    // TODO: This is just too much. Replace by one function maybe ...
    /*
    template <typename F, typename...Args>
    static auto HandledCall(const F& function, Args&&...args) -> typename ReturnType<F>::type
    {
        try
        {
            return function(std::forward<Args>(args)...);
        }
        catch(...)
        {
            return typename ReturnType<F>::type{0};
        }
    }
    */
    bool readBits_catch(unsigned int &into, int numBits, QString &code);
    bool readBits64_catch(uint64_t &into, int numBits, QString &code);
    bool readUEV_catch(unsigned int &into, int &bit_count, QString &code);
    bool readSEV_catch(int &into, int &bit_count, QString &code);
    bool readLeb128_catch(uint64_t &into, int &bit_count, QString &code);
    bool readUVLC_catch(uint64_t &into, int &bit_count, QString &code);
    bool readNS_catch(int &into, int maxVal, int &bit_count, QString &code);
    bool readSU_catch(int &into, int numBits, QString &code);

    QString getMeaningValue(QStringList meanings, unsigned int val);
    QString getMeaningValue(QMap<int,QString> meanings, int val);

    QList<TreeItem*> itemHierarchy;
    TreeItem *currentTreeLevel { nullptr };
  };

  // A simple wrapper for reader_helper.addLogSubLevel / reader_helper->removeLogSubLevel
  class reader_sub_level
  {
  public:
    reader_sub_level(reader_helper &reader, QString name) { reader.addLogSubLevel(name); r = &reader; }
    ~reader_sub_level() { r->removeLogSubLevel(); }
  private:
    reader_helper *r;
  };

  // The item model which is used to display packets from the bitstream. This can be AVPackets or other units from the bitstream (NAL units e.g.)
  class PacketItemModel : public QAbstractItemModel
  {
  public:
    PacketItemModel(QObject *parent);
    ~PacketItemModel();

    // The functions that must be overridden from the QAbstractItemModel
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual QModelIndex parent(const QModelIndex &index) const Q_DECL_OVERRIDE;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE { Q_UNUSED(parent); return 5; }

    // The root of the tree
    QScopedPointer<TreeItem> rootItem;
    TreeItem *getRootItem() { return rootItem.data(); }
    bool isNull() { return rootItem.isNull(); }

    unsigned int getNumberFirstLevelChildren() { return rootItem.isNull() ? 0 : rootItem->childItems.size(); }
    void setUseColorCoding(bool colorCoding);
    void setShowVideoStreamOnly(bool showVideoOnly);

    void setNewNumberModelItems(unsigned int);
  private:
    // This is the current number of first level child items which we show right now.
    // The brackground parser will add more items and it will notify the bitstreamAnalysisWindow
    // about them. The bitstream analysis window will then update this count and the view to show the new items.
    unsigned int nrShowChildItems {0};

    static QList<QColor> streamIndexColors;
    bool useColorCoding { true };
    bool showVideoOnly  { false };
  };

  class FilterByStreamIndexProxyModel : public QSortFilterProxyModel
  {
    Q_OBJECT

  public:
    FilterByStreamIndexProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {};

    int filterStreamIndex() const { return streamIndex; }
    void setFilterStreamIndex(int idx);

  protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

  private:
    int streamIndex { -1 };
  };
}

#endif // PARSERCOMMON_H
