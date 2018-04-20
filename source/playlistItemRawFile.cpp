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

#include "playlistItemRawFile.h"

#include <QFileInfo>
#include <QPainter>
#include <QtConcurrent>
#include <QUrl>
#include <QVBoxLayout>

using namespace YUV_Internals;

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYLISTITEMRAWFILE_DEBUG_LOADING 0
#if PLAYLISTITEMRAWFILE_DEBUG_LOADING && !NDEBUG
#define DEBUG_RAWFILE qDebug
#else
#define DEBUG_RAWFILE(fmt,...) ((void)0)
#endif

playlistItemRawFile::playlistItemRawFile(const QString &rawFilePath, const QSize &frameSize, const QString &sourcePixelFormat, const QString &fmt)
  : playlistItemWithVideo(rawFilePath, playlistItem_Indexed)
{
  // High DPI support for icons:
  // Set the Qt::AA_UseHighDpiPixmaps attribute and then just use QIcon(":image.png")
  // If there is also a image@2x.png in the qrc, Qt will use this for high DPI
  isY4MFile = false;

  // Set the properties of the playlistItem
  setIcon(0, convertIcon(":img_video.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  dataSource.openFile(rawFilePath);

  if (!dataSource.isOk())
  {
    // Opening the file failed.
    setError("Error opening the input file.");
    return;
  }

  // Create a new videoHandler instance depending on the input format
  QFileInfo fi(rawFilePath);
  QString ext = fi.suffix();
  ext = ext.toLower();
  if (ext == "yuv" || ext == "nv21" || fmt.toLower() == "yuv" || ext == "y4m")
  {
    video.reset(new videoHandlerYUV);
    rawFormat = YUV;
  }
  else if (ext == "rgb" || ext == "gbr" || ext == "bgr" || ext == "brg" || fmt.toLower() == "rgb")
  {
    video.reset(new videoHandlerRGB);
    rawFormat = RGB;
  }
  else
    Q_ASSERT_X(false, "playlistItemRawFile()", "No video handler for the raw file format found.");

  if (ext == "y4m")
  {
    // A y4m file has a header and indicators for ever frame
    isY4MFile = true;
    if (!parseY4MFile())
      return;
  }
  else if (frameSize == QSize(-1,-1) && sourcePixelFormat.isEmpty())
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

  if (video->isFormatValid())
    startEndFrame = getStartEndFrameLimits();

  // If the videHandler requests raw data, we provide it from the file
  connect(video.data(), SIGNAL(signalRequestRawData(int, bool)), this, SLOT(loadRawData(int)), Qt::DirectConnection);
  connect(video.data(), &videoHandler::signalUpdateFrameLimits, this,  &playlistItemRawFile::slotUpdateFrameLimits);

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();

  // A raw file can be cached.
  cachingEnabled = true;
}

qint64 playlistItemRawFile::getNumberFrames() const
{
  if (!dataSource.isOk() || !video->isFormatValid())
  {
    // File could not be loaded or there is no valid format set (width/height/rawFormat)
    return 0;
  }

  if (isY4MFile)
    return y4mFrameIndices.count();

  // The file was opened successfully
  qint64 bpf = getBytesPerFrame();
  return (bpf == 0) ? -1 : dataSource.getFileSize() / bpf;
}

infoData playlistItemRawFile::getInfo() const
{
  infoData info((rawFormat == YUV) ? "YUV File Info" : "RGB File Info");

  // At first append the file information part (path, date created, file size...)
  info.items.append(dataSource.getFileInfoList());

  info.items.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  info.items.append(infoItem("Bytes per Frame", QString("%1").arg(getBytesPerFrame())));

  if (dataSource.isOk() && video->isFormatValid() && !isY4MFile)
  {
    // Check if the size of the file and the number of bytes per frame can be divided
    // without any remainder. If not, then there is probably something wrong with the
    // selected YUV format / width / height ...

    qint64 bpf = getBytesPerFrame();
    if ((dataSource.getFileSize() % bpf) != 0)
    {
      // Add a warning
      info.items.append(infoItem("Warning", "The file size and the given video size and/or raw format do not match."));
    }
  }

  return info;
}

bool playlistItemRawFile::parseY4MFile()
{
  // Read a chunck of data from the file. Thecnically, the header can be arbitrarily long, but in practice, 
  // 512 bytes should cover the length of all headers
  QByteArray rawData;
  dataSource.readBytes(rawData, 0, 512);

  // A Y4M file must start with the signature string "YUV4MPEG2 ".
  if (rawData.left(10) != "YUV4MPEG2 ")
    return setError("Y4M File header does not start with YUV4MPEG2 header signature.");

  // Next, there can be any number of parameters. Each paramter starts with a space.
  // The only requirement is, that width, height and framerate are specified.
  qint64 offset = 9;
  int width = -1;
  int height = -1;
  yuvPixelFormat format = yuvPixelFormat(YUV_420, 8, Order_YUV);

  while (rawData.at(offset++) == ' ')
  {
    char parameterIndicator = rawData.at(offset++);
    if (parameterIndicator == 'W' || parameterIndicator == 'H')
    {
      QByteArray number;
      QChar c = rawData.at(offset);
      while (c.isDigit())
      {
        number.append(c);
        c = rawData.at(++offset);
      }

      bool ok = true;
      if (parameterIndicator == 'W')
      {
        width = number.toInt(&ok);
        if (!ok)
          return setError("Error parsing the Y4M header: Invalid width value.");
      }
      if (parameterIndicator == 'H')
      {
        height = number.toInt(&ok);
        if (!ok)
          return setError("Error parsing the Y4M header: Invalid height value.");
      }
    }
    else if (parameterIndicator == 'F')
    {
      // The format is: "25:1" (nom:den)
      QByteArray nominator;
      QChar c = rawData.at(offset);
      while (c.isDigit())
      {
        nominator.append(c);
        c = rawData.at(++offset);
      }
      bool ok = true;
      int nom = nominator.toInt(&ok);
      if (!ok)
        return setError("Error parsing the Y4M header: Invalid framerate nominator.");

      c = rawData.at(offset);
      if (c != ':')
        return setError("Error parsing the Y4M header: Invalid framerate delimiter.");

      QByteArray denominator;
      c = rawData.at(++offset);
      while (c.isDigit())
      {
        denominator.append(c);
        c = rawData.at(++offset);
      }
      int den = denominator.toInt(&ok);
      if (!ok)
        return setError("Error parsing the Y4M header: Invalid framerate denominator.");

      frameRate = double(nom) / double(den);
    }
    else if (parameterIndicator == 'I' || parameterIndicator == 'A' || parameterIndicator == 'X')
    {
      // Interlace mode, pixel aspect ration or comment. Just go to the next (potential) parameter.
    }
    else if (parameterIndicator == 'C')
    {
      // Get 3 bytes and check them
      QByteArray formatName;
      formatName.append(rawData.at(offset++));
      formatName.append(rawData.at(offset++));
      formatName.append(rawData.at(offset++));

      // The YUV format. By default, YUV420 is setup.
      // TDOO: What is the difference between these two formats?
      // 'C420' = 4:2:0 with coincident chroma planes
      // 'C420jpeg' = 4:2 : 0 with biaxially - displaced chroma planes
      // 'C420paldv' = 4 : 2 : 0 with vertically - displaced chroma planes
      if (formatName == "422")
        format.subsampling = YUV_422;
      else if (formatName == "444")
        format.subsampling = YUV_444;

      if (rawData.at(offset) == 'p' && rawData.at(offset+1) == '1' && rawData.at(offset+2) == '0')
      {
        format.bitsPerSample = 10;
        format.planar = true;
        offset += 3;
      }
    }

    // If not already there, seek to the next space (a 0x0A ends the header).
    while (rawData.at(offset) != ' ' && rawData.at(offset) != 10)
    {
      offset++;
      if (offset >= rawData.count())
        // End of bufer
        break;
    }

    // Peek the next byte. If it is 0x0A, the header ends.
    if (rawData.at(offset) == 10)
    {
      offset++;  // Go to the byte after 0x0A.
      break;
    }
  }

  if (width == -1 || height == -1)
    return setError("Error parsing the Y4M header: The size could not be obtained from the header.");

  // Next, all frames should follow. Each frame starts with the sequence 'FRAME', followed by a set of
  // paramters for the frame. The 'FRAME' indicator is terminated by a 0x0A. The list of parameters is 
  // also terminated by 0x0A.

  // The offset in bytes to the next frame
  int stride = width * height * 3 / 2;
  if (format.subsampling == YUV_422)
    stride = width * height * 2;
  else if (format.subsampling == YUV_444)
    stride = width * height * 3;
  if (format.bitsPerSample > 8)
    stride *= 2;
  
  while (true)
  {
    // Seek the file to 'offset' and read a few bytes
    if (dataSource.readBytes(rawData, offset, 20) < 20)
      return setError("Error parsing the Y4M header: The file ended unexpectedly.");

    QByteArray frameIndicator = rawData.mid(0, 5);
    if (frameIndicator != "FRAME")
      return setError("Error parsing the Y4M header: Could not locate the next 'FRAME' indicator.");

    // We will now ignore all frame parameters by searching for the next 0x0A byte. I don't know what
    // we could do with these parameters. 
    offset += 5;
    int internalOffset = 5;
    while (rawData.at(internalOffset) != 10 && internalOffset < 20)
    {
      offset++;
      internalOffset++;
    }

    // Go to the first byte of the YUV frame
    offset++;
    internalOffset++;

    if (internalOffset == 19)
      return setError("Error parsing the Y4M header: The file ended unexpectedly.");

    // Add the frame offset value
    y4mFrameIndices.append(offset);

    offset += stride;
    if (offset >= dataSource.getFileSize())
      break;
  }

  // Success. Set the format and return true;
  video->setFrameSize(QSize(width, height));
  getYUVVideo()->setYUVPixelFormat(format);
  return true;
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

    // We were able to extract width and height from the file name using
    // regular expressions. Try to get the pixel format by checking with the file size.
    video->setFormatFromSizeAndName(frameSize, bitDepth, dataSource.getFileSize(), dataSource.getFileInfo());
    if (rate != -1)
      frameRate = rate;
  }
}

void playlistItemRawFile::createPropertiesWidget()
{
  Q_ASSERT(!propertiesWidget);

  preparePropertiesWidget(QStringLiteral("playlistItemRawFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then videoHandler controls (format,...)
  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(line);
  if (rawFormat == YUV)
    vAllLaout->addLayout(getYUVVideo()->createYUVVideoHandlerControls());
  else if (rawFormat == RGB)
    vAllLaout->addLayout(getRGBVideo()->createRGBVideoHandlerControls());

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);
}

void playlistItemRawFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the raw file. We save both in the playlist.
  QUrl fileURL(dataSource.getAbsoluteFilePath());
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(dataSource.getAbsoluteFilePath());

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemRawFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the raw file (the path to the file. Relative and absolute)
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
playlistItemRawFile *playlistItemRawFile::newplaylistItemRawFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemRawFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  QString type = root.findChildValue("type");

  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // For a RAW file we can load the following values
  int width = root.findChildValue("width").toInt();
  int height = root.findChildValue("height").toInt();
  QString sourcePixelFormat = root.findChildValue("pixelFormat");

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemRawFile *newFile = new playlistItemRawFile(filePath, QSize(width,height), sourcePixelFormat, type);

  // Load the propertied of the playlistItem
  playlistItem::loadPropertiesFromPlaylist(root, newFile);

  return newFile;
}

void playlistItemRawFile::loadRawData(int frameIdxInternal)
{
  if (!video->isFormatValid())
    return;

  DEBUG_RAWFILE("playlistItemRawFile::loadRawData %d", frameIdx);

  // Load the raw data for the given frameIdx from file and set it in the video
  qint64 fileStartPos;
  if (isY4MFile)
    fileStartPos = y4mFrameIndices.at(frameIdxInternal);
  else
    fileStartPos = frameIdxInternal * getBytesPerFrame();
  qint64 nrBytes = getBytesPerFrame();

  if (rawFormat == YUV)
  {
    if (dataSource.readBytes(getYUVVideo()->rawYUVData, fileStartPos, nrBytes) < nrBytes)
      return; // Error
    getYUVVideo()->rawYUVData_frameIdx = frameIdxInternal;
  }
  else if (rawFormat == RGB)
  {
    if (dataSource.readBytes(getRGBVideo()->rawRGBData, fileStartPos, nrBytes) < nrBytes)
      return; // Error
    getRGBVideo()->rawRGBData_frameIdx = frameIdxInternal;
  }

  DEBUG_RAWFILE("playlistItemRawFile::loadRawData %d Done", frameIdxInternal);
}

ValuePairListSets playlistItemRawFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);
  return ValuePairListSets((rawFormat == YUV) ? "YUV" : "RGB", video->getPixelValues(pixelPos, frameIdxInternal));
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
  allExtensions.append("nv21");
  allExtensions.append("y4m");

  filters.append("Raw YUV File (*.yuv *.nv21)");
  filters.append("Raw RGB File (*.rgb *.rbg *.grb *.gbr *.brg *.bgr)");
  filters.append("YUV4MPEG2 File (*.y4m)");
}

qint64 playlistItemRawFile::getBytesPerFrame() const
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
  emit signalItemChanged(true, RECACHE_NONE);
}
