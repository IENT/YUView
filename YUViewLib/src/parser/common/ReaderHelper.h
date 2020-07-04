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

#include <QMap>

#include "SubByteReader.h"
#include "TreeItem.h"

typedef QString (*meaning_callback_function)(unsigned int);

// This is a wrapper around the sub_byte_reader that adds the functionality to log the read symbold to TreeItems
class ReaderHelper
{
public:
  ReaderHelper() = default;
  ReaderHelper(SubByteReader &reader, TreeItem *item, QString new_sub_item_name = "");
  ReaderHelper(const QByteArray &inArr, TreeItem *item, QString new_sub_item_name = "");

  // Add another hierarchical log level to the tree or go back up. Don't call these directly but use the ReaderSubLevel wrapper.
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
  bool          more_rbsp_data()              const { return reader.more_rbsp_data();             }
  bool          payload_extension_present()   const { return reader.payload_extension_present();  }
  unsigned int  nrBytesRead()                 const { return reader.nrBytesRead();                }
  unsigned int  nrBytesLeft()                 const { return reader.nrBytesLeft();                }
  bool          isByteAligned()               const { return reader.isByteAligned();              }
  bool          testReadingBits(int nrBits)   const { return reader.testReadingBits(nrBits);      }
  void          disableEmulationPrevention()        {        reader.disableEmulationPrevention(); }
  QByteArray    readBytes(int nrBytes)              { return reader.readBytes(nrBytes);           }

private:
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

  QString getMeaningValue(QStringList &meanings, unsigned int val);
  QString getMeaningValue(QMap<int,QString> &meanings, int val);

  QList<TreeItem*> itemHierarchy;
  TreeItem *currentTreeLevel { nullptr };
  SubByteReader reader;
};

// A simple wrapper for ReaderHelper.addLogSubLevel / ReaderHelper->removeLogSubLevel
class ReaderSubLevel
{
public:
  ReaderSubLevel(ReaderHelper &reader, QString name) { reader.addLogSubLevel(name); r = &reader; }
  ~ReaderSubLevel() { r->removeLogSubLevel(); }
private:
  ReaderHelper *r;
};
