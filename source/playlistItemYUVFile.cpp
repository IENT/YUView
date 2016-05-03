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

#include "playlistItemYUVFile.h"
#include "typedef.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QUrl>
#include <QTime>
#include <QDebug>
#include <QPainter>

playlistItemYUVFile::playlistItemYUVFile(QString yuvFilePath, QSize frameSize, QString sourcePixelFormat )
  : playlistItemIndexed(yuvFilePath)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  dataSource.openFile(yuvFilePath);

  if (!dataSource.isOk())
    // Opening the file failed.
    return;

  if (frameSize == QSize(-1,-1) && sourcePixelFormat == "")
  {
    // Try to get the frame format from the file name. The fileSource can guess this.
    setFormatFromFileName();

    if (!yuvVideo.isFormatValid())
    {
      // Load 8294400 bytes from the input and try to get the format from the correlation. 
      QByteArray rawYUVData;
      dataSource.readBytes(rawYUVData, 0, 8294400);
      yuvVideo.setFormatFromCorrelation(rawYUVData, dataSource.getFileSize());
    }

    if (yuvVideo.isFormatValid())
      startEndFrame = getstartEndFrameLimits();
  }
  else
    // Just set the given values
    yuvVideo.loadValues(frameSize, sourcePixelFormat);

  // If the yuvVideHandler requests raw YUV data, we provide it from the file
  connect(&yuvVideo, SIGNAL(signalRequesRawYUVData(int)), this, SLOT(loadYUVData(int)), Qt::DirectConnection);
  connect(&yuvVideo, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
  connect(&yuvVideo, SIGNAL(signalGetFrameLimits()), this, SLOT(slotUpdateFrameRange()));
}

playlistItemYUVFile::~playlistItemYUVFile()
{
}

qint64 playlistItemYUVFile::getNumberFrames()
{
  if (!dataSource.isOk() || !yuvVideo.isFormatValid())
  {
    // File could not be loaded or there is no valid format set (width/height/yuvFormat)
    return 0;
  }

  // The file was opened successfully
  qint64 bpf = yuvVideo.getBytesPerYUVFrame();
  //unsigned int nrFrames = (unsigned int)(dataSource.getFileSize() / bpf);

  return (bpf == 0) ? -1 : dataSource.getFileSize() / bpf;
}

QList<infoItem> playlistItemYUVFile::getInfoList()
{
  QList<infoItem> infoList;

  // At first append the file information part (path, date created, file size...)
  infoList.append(dataSource.getFileInfoList());

  infoList.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  infoList.append(infoItem("Bytes per Frame", QString("%1").arg(yuvVideo.getBytesPerYUVFrame())));

  if (dataSource.isOk() && yuvVideo.isFormatValid())
  {
    // Check if the size of the file and the number of bytes per frame can be divided
    // without any remainder. If not, then there is probably something wrong with the
    // selected YUV format / width / height ...

    qint64 bpf = yuvVideo.getBytesPerYUVFrame();
    if ((dataSource.getFileSize() % bpf) != 0)
    {
      // Add a warning
      infoList.append(infoItem("Warning", "The file size and the given video size and/or YUV format do not match."));
    }
  }
  infoList.append(infoItem("Frames Cached",QString::number(yuvVideo.getNrFramesCached())));

  return infoList;
}

void playlistItemYUVFile::setFormatFromFileName()
{
  int width, height, rate, bitDepth, subFormat;
  dataSource.formatFromFilename(width, height, rate, bitDepth, subFormat);

  if(width > 0 && height > 0)
  {
    // We were able to extrace width and height from the file name using
    // regular expressions. Try to get the pixel format by checking with the file size.
    yuvVideo.setFormatFromSize(QSize(width,height), bitDepth, dataSource.getFileSize(), subFormat);
    frameRate = rate;
  }
}

void playlistItemYUVFile::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemYUVFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  QFrame *lineOne = new QFrame(propertiesWidget);
  lineOne->setObjectName(QStringLiteral("lineOne"));
  lineOne->setFrameShape(QFrame::HLine);
  lineOne->setFrameShadow(QFrame::Sunken);
  
  QFrame *lineTwo = new QFrame(propertiesWidget);
  lineTwo->setObjectName(QStringLiteral("lineTwo"));
  lineTwo->setFrameShape(QFrame::HLine);
  lineTwo->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout( createIndexControllers(propertiesWidget) );
  vAllLaout->addWidget( lineOne );
  vAllLaout->addLayout( yuvVideo.createVideoHandlerControls(propertiesWidget) );
  vAllLaout->addWidget( lineTwo );
  vAllLaout->addLayout( yuvVideo.createYuvVideoHandlerControls(propertiesWidget) );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(5, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemYUVFile::savePlaylist(QDomElement &root, QDir playlistDir)
{
  // Determine the relative path to the yuv file. We save both in the playlist.
  QUrl fileURL( dataSource.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( dataSource.getAbsoluteFilePath() );

  QDomElementYUV d = root.ownerDocument().createElement("playlistItemYUVFile");

  // Append the properties of the playlistItemIndexed
  playlistItemIndexed::appendPropertiesToPlaylist(d);
  
  // Apppend all the properties of the yuv file (the path to the file. Relative and absolute)
  d.appendProperiteChild( "absolutePath", fileURL.toString() );
  d.appendProperiteChild( "relativePath", relativePath  );
  
  // Append the video handler properties
  d.appendProperiteChild( "width", QString::number(yuvVideo.getFrameSize().width()) );
  d.appendProperiteChild( "height", QString::number(yuvVideo.getFrameSize().height()) );
  
  // Append the YUV properties
  d.appendProperiteChild( "pixelFormat", yuvVideo.getSrcPixelFormatName() );
      
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemYUVFile.
*/
playlistItemYUVFile *playlistItemYUVFile::newplaylistItemYUVFile(QDomElementYUV root, QString playlistFilePath)
{
  // Parse the dom element. It should have all values of a playlistItemYUVFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  
  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return NULL;

  // For a YUV file we can load the following values
  int width = root.findChildValue("width").toInt();
  int height = root.findChildValue("height").toInt();
  QString sourcePixelFormat = root.findChildValue("pixelFormat");
  
  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemYUVFile *newFile = new playlistItemYUVFile(filePath, QSize(width,height), sourcePixelFormat);

  // Load the propertied of the playlistItemIndexed
  playlistItemIndexed::loadPropertiesFromPlaylist(root, newFile);
  
  return newFile;
}

void playlistItemYUVFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  if (frameIdx != -1)
    yuvVideo.drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemYUVFile::loadYUVData(int frameIdx)
{
  // Load the raw YUV data for the given frameIdx from file and set it in the yuvVideo
  qint64 fileStartPos = frameIdx * yuvVideo.getBytesPerYUVFrame();
  dataSource.readBytes( yuvVideo.rawYUVData, fileStartPos, yuvVideo.getBytesPerYUVFrame() );
  yuvVideo.rawYUVData_frameIdx = frameIdx;
}
