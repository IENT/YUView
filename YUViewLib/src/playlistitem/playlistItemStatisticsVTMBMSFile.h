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

#ifndef PLAYLISTITEMSTATISTICSVTMBMSFILE_H
#define PLAYLISTITEMSTATISTICSVTMBMSFILE_H

#include <QBasicTimer>
#include <QFuture>
#include <QRegularExpression>

#include "filesource/fileSource.h"
#include "playlistItemStatisticsFile.h"
#include "statistics/statisticHandler.h"

class playlistItemStatisticsVTMBMSFile : public playlistItemStatisticsFile
{
  Q_OBJECT

public:

  /*
  */
  playlistItemStatisticsVTMBMSFile(const QString &itemNameOrFileName);

  bool isFileSource() const Q_DECL_OVERRIDE { return true; };

  // ------ Statistics ----

  // Does the playlistItem provide statistics? If yes, the following functions can be
  // used to access it


  // Create a new playlistItemStatisticsFile from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemStatisticsVTMBMSFile *newplaylistItemStatisticsVTMBMSFile(const YUViewDomElement &root, const QString &playlistFilePath);

  // Add the file type filters and the extensions of files that we can load.
  static void getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters);

  // ----- Detection of source/file change events -----
  virtual void reloadItemSource() Q_DECL_OVERRIDE;
public slots:
  //! Load the statistics with frameIdx/type from file and put it into the cache.
  //! If the statistics file is in an interleaved format (types are mixed within one POC) this function also parses
  //! types which were not requested by the given 'type'.
  void loadStatisticToCache(int frameIdxInternal, int type);

private:

  QString getPlaylistTag() const Q_DECL_OVERRIDE { return "playlistItemStatisticsVTMBMSFile"; }

  //! Scan the header: What types are saved in this file?
  void readHeaderFromFile();
  
  // A list of file positions where each POC starts
  QMap<int, qint64> pocStartList;

  // --------------- background parsing ---------------
  //! Parser the whole file and get the positions where a new POC/type starts. Save this position in p_pocTypeStartList.
  //! This is performed in the background using a QFuture.
  void readFramePositionsFromFile();
};

#endif // PLAYLISTITEMSTATISTICSVTMBMSFILE_H
