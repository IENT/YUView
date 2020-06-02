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

#include "ReaderHelper.h"

ReaderHelper::ReaderHelper(SubByteReader &reader, TreeItem *item, QString new_sub_item_name)
{
  this->reader = reader;
  if (item)
  {
    if (new_sub_item_name.isEmpty())
      currentTreeLevel = item;
    else
      currentTreeLevel = new TreeItem(new_sub_item_name, item);
  }
  itemHierarchy.append(currentTreeLevel);
}

ReaderHelper::ReaderHelper(const QByteArray &inArr, TreeItem *item, QString new_sub_item_name)
{
  this->reader = SubByteReader(inArr);
  if (item)
  {
    if (new_sub_item_name.isEmpty())
      currentTreeLevel = item;
    else
      currentTreeLevel = new TreeItem(new_sub_item_name, item);
  }
  itemHierarchy.append(currentTreeLevel);
}

void ReaderHelper::addLogSubLevel(const QString name)
{
  assert(!name.isEmpty());
  if (itemHierarchy.last() == nullptr)
    return;
  currentTreeLevel = new TreeItem(name, itemHierarchy.last());
  itemHierarchy.append(currentTreeLevel);
}

void ReaderHelper::removeLogSubLevel()
{
  if (itemHierarchy.length() <= 1)
    // Don't remove the root
    return;
  itemHierarchy.removeLast();
  currentTreeLevel = itemHierarchy.last();
}

// TODO: Fixed length / variable length codes logging
bool ReaderHelper::readBits(int numBits, unsigned int &into, QString intoName, QString meaning)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, meaning, currentTreeLevel);
  return true;
}

bool ReaderHelper::readBits(int numBits, uint64_t &into, QString intoName, QString meaning)
{
  QString code;
  if (!readBits64_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, meaning, currentTreeLevel);
  return true;
}

bool ReaderHelper::readBits(int numBits, unsigned int &into, QString intoName, QStringList meanings)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool ReaderHelper::readBits(int numBits, unsigned int &into, QString intoName, QMap<int,QString> meanings)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool ReaderHelper::readBits(int numBits, unsigned int &into, QString intoName, meaning_callback_function pMeaning)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("u(v) -> u(%1)").arg(numBits), code, pMeaning(into), currentTreeLevel);
  return true;
}

bool ReaderHelper::readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx)
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

bool ReaderHelper::readBits(int numBits, QList<unsigned int> &into, QString intoName, int idx, meaning_callback_function pMeaning)
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

bool ReaderHelper::readBits(int numBits, QByteArray &into, QString intoName, int idx)
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

bool ReaderHelper::readBits(int numBits, unsigned int &into, QMap<int, QString> intoNames)
{
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  if (currentTreeLevel)    
    new TreeItem(getMeaningValue(intoNames, into), into, QString("u(v) -> u(%1)").arg(numBits), code, currentTreeLevel);
  return true;
}

bool ReaderHelper::readZeroBits(int numBits, QString intoName)
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

bool ReaderHelper::ignoreBits(int numBits)
{
  unsigned int into;
  QString code;
  if (!readBits_catch(into, numBits, code))
    return false;
  return true;
}

bool ReaderHelper::readFlag(bool &into, QString intoName, QString meaning)
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

bool ReaderHelper::readFlag(QList<bool> &into, QString intoName, int idx, QString meaning)
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

bool ReaderHelper::readFlag(bool &into, QString intoName, QStringList meanings)
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

bool ReaderHelper::readUEV(unsigned int &into, QString intoName, QStringList meanings)
{
  QString code;
  int bit_count = 0;
  if (!readUEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ue(v) -> ue(%1)").arg(bit_count), code, getMeaningValue(meanings, into), currentTreeLevel);
  return true;
}

bool ReaderHelper::readUEV(unsigned int &into, QString intoName, QString meaning)
{
  QString code;
  int bit_count = 0;
  if (!readUEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ue(v) -> ue(%1)").arg(bit_count), code, meaning, currentTreeLevel);
  return true;
}

bool ReaderHelper::readUEV(QList<quint32> &into, QString intoName, int idx, QString meaning)
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

bool ReaderHelper::readSEV(int &into, QString intoName, QStringList meanings)
{
  QString code;
  int bit_count = 0;
  if (!readSEV_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("se(v) -> se(%1)").arg(bit_count), code, getMeaningValue(meanings, (unsigned int)into), currentTreeLevel);
  return true;
}

bool ReaderHelper::readSEV(QList<int> into, QString intoName, int idx)
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

bool ReaderHelper::readLeb128(uint64_t &into, QString intoName)
{
  QString code;
  int bit_count = 0;
  if (!readLeb128_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("leb128(v) -> leb128(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool ReaderHelper::readUVLC(uint64_t &into, QString intoName)
{
  QString code;
  int bit_count = 0;
  if (!readUVLC_catch(into, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("leb128(v) -> leb128(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool ReaderHelper::readNS(int &into, QString intoName, int maxVal)
{
  QString code;
  int bit_count = 0;
  if (!readNS_catch(into, maxVal, bit_count, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("ns(%1)").arg(bit_count), code, currentTreeLevel);
  return true;
}

bool ReaderHelper::readSU(int &into, QString intoName, int nrBits)
{
  QString code;
  if (!readSU_catch(into, nrBits, code))
    return false;
  if (currentTreeLevel)
    new TreeItem(intoName, into, QString("su(%1)").arg(nrBits), code, currentTreeLevel);
  return true;
}

void ReaderHelper::logValue(int value, QString valueName, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, "calc", meaning, currentTreeLevel);
}

void ReaderHelper::logValue(int value, QString valueName, QString coding, QString code, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, coding, code, meaning, currentTreeLevel);
}

void ReaderHelper::logValue(QString value, QString valueName, QString meaning)
{
  if (currentTreeLevel)
    new TreeItem(valueName, value, "calc", meaning, currentTreeLevel);
}

void ReaderHelper::logInfo(QString info)
{
  if (currentTreeLevel)
    new TreeItem(info, currentTreeLevel);
}

bool ReaderHelper::addErrorMessageChildItem(QString errorMessage, TreeItem *item)
{
  if (item)
  {
    new TreeItem("Error", "", "", "", errorMessage, item, true);
    item->setError();
  }
  return false;
}

bool ReaderHelper::readBits_catch(unsigned int &into, int numBits, QString &code)
{
  try
  {
    into = this->reader.readBits(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readBits64_catch(uint64_t &into, int numBits, QString &code)
{
  try
  {
    into = this->reader.readBits64(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readUEV_catch(unsigned int &into, int &bit_count, QString &code)
{
  try
  {
    into = this->reader.readUE_V(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readSEV_catch(int &into, int &bit_count, QString &code)
{
  try
  {
    into = this->reader.readSE_V(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readLeb128_catch(uint64_t &into, int &bit_count, QString &code)
{
  try
  {
    into = this->reader.readLeb128(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readUVLC_catch(uint64_t &into, int &bit_count, QString &code)
{
  try
  {
    into = this->reader.readUVLC(code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readNS_catch(int &into, int maxVal, int &bit_count, QString &code)
{
  try
  {
    into = this->reader.readNS(maxVal, code, bit_count);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

bool ReaderHelper::readSU_catch(int &into, int numBits, QString &code)
{
  try
  {
    into = this->reader.readSU(numBits, code);
  }
  catch (const std::exception& ex)
  {
    addErrorMessageChildItem("Reading error:  " + QString(ex.what()));
    return false;
  }
  return true;
}

QString ReaderHelper::getMeaningValue(QStringList &meanings, unsigned int val)
{
  if (val < (unsigned int)meanings.length())
    return meanings.at(val);
  else if (val >= (unsigned int)meanings.length() && !meanings.isEmpty())
    return meanings.last();
  return "";
}

QString ReaderHelper::getMeaningValue(QMap<int,QString> &meanings, int val)
{
  if (meanings.contains(val))
    return meanings.value(val);
  else if (meanings.contains(-1))
    return meanings.value(-1);
  return "";
}