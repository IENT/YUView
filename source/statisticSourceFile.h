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

#ifndef STATISTICSOURCEFILE_H
#define STATISTICSOURCEFILE_H

#include "statisticSource.h"
#include <QFuture>
#include <QFile>

class statisticSourceFile : public QObject, public statisticSource
{
  Q_OBJECT 

public:
  statisticSourceFile(QString fileName);
  ~statisticSourceFile();

  // Get the file name
  virtual QString getName() { return p_FileName; }
  virtual QString getPath() { return p_filePath; }
  virtual QString getCreatedtime() { return p_createdtime; }
  virtual QString getModifiedtime() { return p_modifiedtime; }
  virtual qint64  getNumberBytes() { return p_fileSize; }
  virtual qint64  getNumberFrames() { return p_nrFrames; }

  virtual QSize getSize() { return p_size; }
  virtual int getFrameRate() { return p_frameRate; }

signals:
  // Just emit if some property of the object changed without the user (the GUI) being the reason for it.
  // This could for example be a background process that updates the number of frames or the status text ...
  void signal_statisticInformationChanged();

private:
  //! Scan the header: What types are saved in this file?
  void readHeaderFromFile();
  //! Parser the whole file and get the positions where a new POC/type starts. Save this position in p_pocTypeStartList.
  //! This is performed in the background
  void readFrameAndTypePositionsFromFile();
  //! Load the statistics with frameIdx/type from file and put it into the cache.
  //! If the statistics file is in an interleaved format (types are mixed within one POC) this function also parses
  //! types which were not requested by the given 'type'.
  void loadStatisticToCache(int frameIdx, int type);
    
  QStringList parseCSVLine(QString line, char delimiter);
  
  QFile *p_srcFile;
  QString p_srcFileName;

  QString p_FileName;
  QString p_filePath;
  QString p_createdtime;
  QString p_modifiedtime;
  qint64  p_fileSize;
  qint64  p_nrFrames;
  
  // The size (width/height) and frameRate of the statistics. After reading the header this should be set.
  QSize p_size;
  int p_frameRate;
  
  // Set if the file is sorted by POC and the types are 'random' within this POC (true)
  // or if the file is sorted by typeID and the POC is 'random'
  bool bFileSortedByPOC;

  QFuture<void> p_backgroundParserFuture;
  bool p_cancelBackgroundParser;

  // A list of file positions where each POC/type starts
  QMap<int, QMap<int, qint64> > p_pocTypeStartList;

  //! Error while parsing. Set the error message that will be returned by status(). Also set p_numberFrames to 0, clear p_pocStartList.
  void setErrorState(QString sError);
};


#endif