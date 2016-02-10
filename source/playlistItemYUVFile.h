/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLAYLISTITEMYUVFILE_H
#define PLAYLISTITEMYUVFILE_H

#include "playlistItem.h"
#include "typedef.h"
#include "fileSource.h"
#include <QString>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include "playlistItemYuvSource.h"

class playlistItemYUVFile :
  public playlistItemYuvSource
{
  Q_OBJECT

public:
  playlistItemYUVFile(QString yuvFilePath, bool tryFormatGuess=true);
  ~playlistItemYUVFile();

  // Overload from playlistItem. Save the yuv file item to playlist.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE;

  // Override from playlistItem. Return the info title and info list to be shown in the fileInfo groupBox.
  virtual QString getInfoTitel() Q_DECL_OVERRIDE { return "YUV File Info"; }
  virtual QList<infoItem> getInfoList() Q_DECL_OVERRIDE;

  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "YUV File Properties"; }

  // Create a new playlistItemYUVFile from the playlist file entry. Return NULL if parsing failed.
  static playlistItemYUVFile *newplaylistItemYUVFile(QDomElement stringElement, QString playlistFilePath);

protected:

  // Try to get and set the format from file name. If after calling this function isFormatValid()
  // returns false then it failed.
  void setFormatFromFileName();
  
  // Try to guess and set the format (frameSize/srcPixelFormat) from the file itself.
  // If after calling this function isFormatValid() returns false then it failed.
  void setFormatFromCorrelation();

  // Override from playlistItemVideo
  virtual qint64 getNumberFrames() Q_DECL_OVERRIDE;

  // Override from playlistItemVideo. Load the given frame from file and convert it to pixmap.
  virtual void loadFrame(int frameIdx) Q_DECL_OVERRIDE;
  
private:

  // Overload from playlistItem. Create a properties widget custom to the YUVFile
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;
  
  // We keep a temporary byte array for one frame in YUV format to save the overhead of
  // creating/resizing it every time we want to convert an image.
  QByteArray tempYUVFrameBuffer;

  fileSource dataSource;

};

#endif
