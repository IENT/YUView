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

#include "playlistItemRawFile.h"

#include <QFileInfo>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QUrl>
#include <QTime>
#include <QDebug>
#include <QPainter>

playlistItemRawFile::playlistItemRawFile(QString rawFilePath, QSize frameSize, QString sourcePixelFormat, QString fmt)
  : playlistItemIndexed(rawFilePath), video(NULL)
{
  /*For those coming from the internets looking for a quick fix, currently I'd recommend
    Setting the Qt::AA_UseHighDpiPixmaps attribute
    And using QIcon::addFile(":/file.png");
  If your qrc / folder has both file.png and file@2x.png, then things render appropriately based on the current machine's device pixel ratio. */

  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_video.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  dataSource.openFile(rawFilePath);

  if (!dataSource.isOk())
    // Opening the file failed.
    return;

  // Create a new videoHandler instance depending on the input format
  QFileInfo fi(rawFilePath);
  QString ext = fi.suffix();
  ext = ext.toLower();
  if (ext == "yuv" || fmt.toLower() == "yuv")
  {
    video = new videoHandlerYUV;
    rawFormat = YUV;
  }
  else if (ext == "rgb" || ext == "gbr" || ext == "bgr" || ext == "brg" || fmt.toLower() == "rgb")
  {
    video = new videoHandlerRGB;
    rawFormat = RGB;
  }
  else
    Q_ASSERT_X(false, "playlistItemRawFile()", "No video handler for the raw file format found.");

  if (frameSize == QSize(-1,-1) && sourcePixelFormat == "")
  {
    // Try to get the frame format from the file name. The fileSource can guess this.
    setFormatFromFileName();

    if (!video->isFormatValid())
    {
      // Load 24883200 bytes from the input and try to get the format from the correlation. 
      QByteArray rawData;
      dataSource.readBytes(rawData, 0, 24883200);
      video->setFormatFromCorrelation(rawData, dataSource.getFileSize());
    }

    if (video->isFormatValid())
      startEndFrame = getstartEndFrameLimits();
  }
  else
  {
    // Just set the given values
    video->setFrameSize(frameSize);
    if (rawFormat == YUV)
      getYUVVideo()->setYUVPixelFormatByName(sourcePixelFormat);
    else if (rawFormat == RGB)
      getRGBVideo()->setRGBPixelFormatByName(sourcePixelFormat);
  }

  // If the videHandler requests raw data, we provide it from the file
  connect(video, SIGNAL(signalRequesRawData(int, bool)), this, SLOT(loadRawData(int, bool)), Qt::DirectConnection);
  connect(video, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
  connect(video, SIGNAL(signalUpdateFrameLimits()), this, SLOT(slotUpdateFrameLimits()));

  // A raw file can be cached.
  cachingEnabled = true;
}

playlistItemRawFile::~playlistItemRawFile()
{
  if (video)
    delete video;
}

qint64 playlistItemRawFile::getNumberFrames()
{
  if (!dataSource.isOk() || !video->isFormatValid())
  {
    // File could not be loaded or there is no valid format set (width/height/rawFormat)
    return 0;
  }

  // The file was opened successfully
  qint64 bpf = getBytesPerFrame();
  //unsigned int nrFrames = (unsigned int)(dataSource.getFileSize() / bpf);

  return (bpf == 0) ? -1 : dataSource.getFileSize() / bpf;
}

QList<infoItem> playlistItemRawFile::getInfoList()
{
  QList<infoItem> infoList;

  // At first append the file information part (path, date created, file size...)
  infoList.append(dataSource.getFileInfoList());

  infoList.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  infoList.append(infoItem("Bytes per Frame", QString("%1").arg(getBytesPerFrame())));

  if (dataSource.isOk() && video->isFormatValid())
  {
    // Check if the size of the file and the number of bytes per frame can be divided
    // without any remainder. If not, then there is probably something wrong with the
    // selected YUV format / width / height ...

    qint64 bpf = getBytesPerFrame();
    if ((dataSource.getFileSize() % bpf) != 0)
    {
      // Add a warning
      infoList.append(infoItem("Warning", "The file size and the given video size and/or raw format do not match."));
    }
  }
  infoList.append(infoItem("Frames Cached", QString::number(video->getNrFramesCached())));

  return infoList;
}

void playlistItemRawFile::setFormatFromFileName()
{
  // Try to extract info on the width/height/rate/bitDepth from the file name
  QSize frameSize;
  int rate, bitDepth;
  dataSource.formatFromFilename(frameSize, rate, bitDepth);

  if(frameSize.isValid())
  {
    video->setFrameSize(frameSize);

    // We were able to extrace width and height from the file name using
    // regular expressions. Try to get the pixel format by checking with the file size.
    video->setFormatFromSizeAndName(frameSize, bitDepth, dataSource.getFileSize(), dataSource.getFileInfo());
    if (rate != -1)
      frameRate = rate;
  }
}

void playlistItemRawFile::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemRawFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  QFrame *line = new QFrame(propertiesWidget);
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  
  // First add the parents controls (first video controls (width/height...) then videoHandler controls (format,...)
  vAllLaout->addLayout( createIndexControllers(propertiesWidget) );
  vAllLaout->addWidget( line );
  if (rawFormat == YUV)
    vAllLaout->addLayout( getYUVVideo()->createYUVVideoHandlerControls(propertiesWidget) );
  else if (rawFormat == RGB)
    vAllLaout->addLayout( getRGBVideo()->createRGBVideoHandlerControls(propertiesWidget) );
  
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemRawFile::savePlaylist(QDomElement &root, QDir playlistDir)
{
  // Determine the relative path to the raw file. We save both in the playlist.
  QUrl fileURL( dataSource.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(dataSource.getAbsoluteFilePath());

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemRawFile");

  // Append the properties of the playlistItemIndexed
  playlistItemIndexed::appendPropertiesToPlaylist(d);
  
  // Apppend all the properties of the raw file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  d.appendProperiteChild("type", (rawFormat == YUV) ? "YUV" : "RGB");

  // Append the video handler properties
  d.appendProperiteChild("width", QString::number(video->getFrameSize().width()));
  d.appendProperiteChild("height", QString::number(video->getFrameSize().height()));
  
  // Append the videoHandler properties
  if (rawFormat == YUV)
    d.appendProperiteChild("pixelFormat", getYUVVideo()->getRawYUVPixelFormatName());
  else if (rawFormat == RGB)
    d.appendProperiteChild("pixelFormat", getRGBVideo()->getRawRGBPixelFormatName());
      
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemRawFile.
*/
playlistItemRawFile *playlistItemRawFile::newplaylistItemRawFile(QDomElementYUView root, QString playlistFilePath)
{
  // Parse the dom element. It should have all values of a playlistItemRawFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  QString type = root.findChildValue("type");
  
  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return NULL;

  // For a RAW file we can load the following values
  int width = root.findChildValue("width").toInt();
  int height = root.findChildValue("height").toInt();
  QString sourcePixelFormat = root.findChildValue("pixelFormat");
  
  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemRawFile *newFile = new playlistItemRawFile(filePath, QSize(width,height), sourcePixelFormat, type);

  // Load the propertied of the playlistItemIndexed
  playlistItemIndexed::loadPropertiesFromPlaylist(root, newFile);
  
  return newFile;
}

void playlistItemRawFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  Q_UNUSED(playback);

  if (frameIdx != -1)
    video->drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemRawFile::loadRawData(int frameIdx, bool forceLoadingNow)
{
  // TODO: We could also load the raw data in the backgroudn showing a "loading..." screen.
  // This could help especially if loading over a network connection.
  Q_UNUSED(forceLoadingNow);

  if (!video->isFormatValid())
    return;

  // Load the raw data for the given frameIdx from file and set it in the video
  qint64 fileStartPos = frameIdx * getBytesPerFrame();
  qint64 nrBytes = getBytesPerFrame();
  if (rawFormat == YUV)
  {
    if (dataSource.readBytes(getYUVVideo()->rawYUVData, fileStartPos, nrBytes) < nrBytes)
      return; // Error
    getYUVVideo()->rawYUVData_frameIdx = frameIdx;
  }
  else if (rawFormat == RGB)
  {
    if (dataSource.readBytes(getRGBVideo()->rawRGBData, fileStartPos, nrBytes) < nrBytes)
      return; // Error
    getRGBVideo()->rawRGBData_frameIdx = frameIdx;
  }
}

ValuePairListSets playlistItemRawFile::getPixelValues(QPoint pixelPos, int frameIdx) 
{ 
  return ValuePairListSets((rawFormat == YUV) ? "YUV" : "RGB", video->getPixelValues(pixelPos, frameIdx)); 
}

void playlistItemRawFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("yuv");
  allExtensions.append("rgb");
  allExtensions.append("rbg");
  allExtensions.append("grb");
  allExtensions.append("gbr");
  allExtensions.append("brg");
  allExtensions.append("bgr");

  filters.append("Raw YUV File (*.yuv)");
  filters.append("Raw RGB File (*.rgb *.rbg *.grb *.gbr *.brg *.bgr)");
}

qint64 playlistItemRawFile::getBytesPerFrame()
{
  if (rawFormat == YUV)
      return getYUVVideo()->getBytesPerFrame();
    else if (rawFormat == RGB)
      return getRGBVideo()->getBytesPerFrame();
  return -1;
}

void playlistItemRawFile::reloadItemSource()
{
  // Reopen the file
  dataSource.openFile(plItemNameOrFileName);
  if (!dataSource.isOk())
    // Opening the file failed.
    return;

  video->invalidateAllBuffers();

  // Emit that the item needs redrawing and the cache changed.
  emit signalItemChanged(true, true);
}