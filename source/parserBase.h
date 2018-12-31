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

#ifndef PARSERBASEE_H
#define PARSERBASEE_H

#include <QAbstractItemModel>
#include <QString>
#include <QTreeWidgetItem>

#include "parserCommon.h"

/* Abstract base class that prvides features which are common to all parsers
 */
class parserBase : public QObject
{
  Q_OBJECT

public:
  parserBase(QObject *parent);
  virtual ~parserBase() = 0;

  // Get a pointer to the nal unit model. The model is only filled if you call enableModel() first.
  //QAbstractItemModel *getPacketItemModel() { return packetModel.data(); }
  QAbstractItemModel *getPacketItemModel() { return packetModel.data(); }
  QAbstractItemModel *getFilteredPacketItemModel();
  
  void setNewNumberModelItems(unsigned int n) { packetModel->setNewNumberModelItems(n); }
  void enableModel();

  // Get info about the stream organized in a tree
  virtual QList<QTreeWidgetItem*> getStreamInfo() = 0;
  virtual unsigned int getNrStreams() = 0;

  // For parsing files in the background (threading) in the bitstream analysis dialog:
  virtual bool runParsingOfFile(QString fileName) = 0;
  int getParsingProgressPercent() { return progressPercentValue; }
  void setAbortParsing() { cancelBackgroundParser = true; }

  virtual int getVideoStreamIndex() { return -1; }

  void setStreamColorCoding(bool colorCoding) { packetModel->setUseColorCoding(colorCoding); }

signals:
  // An item was added to the nal model. This is emitted whenever a NAL unit or an AVPacket is parsed.
  void nalModelUpdated(unsigned int newNumberItems);
  void backgroundParsingDone(QString error);
  
  // Signal that the getStreamInfo() function will now return an updated info
  void streamInfoUpdated();

protected:
  QScopedPointer<parserCommon::PacketItemModel> packetModel;
  QScopedPointer<parserCommon::FilterByStreamIndexProxyModel> streamIndexFilter;

  // If this variable is set (from an external thread), the parsing process should cancel immediately
  bool cancelBackgroundParser {false};
  int  progressPercentValue   {0};
};

#endif // PARSERBASEE_H
