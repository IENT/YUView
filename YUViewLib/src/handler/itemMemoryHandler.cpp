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

#include "itemMemoryHandler.h"

#include <QDateTime>
#include <QSettings>

#define ITEMMEMORYHANDLER_DEBUG 0
#if ITEMMEMORYHANDLER_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_MEMORY(msg) qDebug() << msg
#else
#define DEBUG_MEMORY(msg) ((void)0)
#endif

namespace itemMemoryHandler
{

struct ItemData
{
  QString filePath;
  QDateTime itemChangedLast;
  QString format;
  QString toString() const { return QString("File: %1 Last Changed: %2 Format: %3").arg(filePath).arg(itemChangedLast.toString("yy-M-d H-m-s")).arg(format); }
};

QList<ItemData> getAllValidItems()
{
  QSettings settings;
  QList<ItemData> validItems;
  auto timeYesterday = QDateTime::currentDateTime().addDays(-1);

  auto size = settings.beginReadArray("itemMemory");
  for (int i = 0; i < size; ++i) 
  {
    settings.setArrayIndex(i);
    ItemData data;
    data.filePath = settings.value("filePath").toString();
    data.itemChangedLast = settings.value("dateChanged").toDateTime();
    data.format = settings.value("format").toString();
    if (data.itemChangedLast >= timeYesterday)
    {
      validItems.append(data);
      DEBUG_MEMORY("getAllValidItems Read Valid Item " << data.toString());
    }
    else
    {
      DEBUG_MEMORY("getAllValidItems Read invalid Item " << data.toString() << " - discarded");
    }
  }
  settings.endArray();

  return validItems;
}

void writeNewItemList(QList<ItemData> newItemList)
{
  QSettings settings;
  settings.remove("itemMemory");  // Delete the old list

  settings.beginWriteArray("itemMemory");
  for (int i = 0; i < newItemList.size(); ++i) 
  {
    const auto &item = newItemList[i];
    settings.setArrayIndex(i);
    settings.setValue("filePath", item.filePath);
    settings.setValue("dateChanged", QVariant(item.itemChangedLast));
    settings.setValue("format", item.format);
    DEBUG_MEMORY("writeNewItemList Written item " << item.toString());
  }
  settings.endArray();
}

void itemMemoryAddFormat(QString filePath, QString format)
{
  auto validItems = getAllValidItems();

  bool itemUpdated = false;
  for (auto item : validItems)
  {
    if (item.filePath == filePath)
    {
      item.itemChangedLast = QDateTime::currentDateTime();
      item.format = format;
      itemUpdated = true;
      DEBUG_MEMORY("itemMemoryAddFormat Modified item " << item.toString());
      break;
    }
  }

  if (!itemUpdated)
  {
    ItemData newItem;
    newItem.filePath = filePath;
    newItem.itemChangedLast = QDateTime::currentDateTime();
    newItem.format = format;
    validItems.append(newItem);
    DEBUG_MEMORY("itemMemoryAddFormat Added new item " << newItem.toString());
  }
  
  writeNewItemList(validItems);
}

QString itemMemoryGetFormat(QString filePath)
{
  auto validItems = getAllValidItems();

  for (auto item : validItems)
    if (item.filePath == filePath)
      return item.format;

  return {};
}

} // namespace itemMemoryHandler
