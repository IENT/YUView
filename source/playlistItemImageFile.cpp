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

#include "playlistItemImageFile.h"

#include <QImageReader>
#include <QPainter>
#include <QSettings>
#include <QtConcurrent>
#include "fileSource.h"

#define IMAGEFILE_ERROR_TEXT "The given image file could not be loaded."

playlistItemImageFile::playlistItemImageFile(const QString &filePath) 
  : playlistItem(filePath, playlistItem_Static),
  needToLoadImage(true), imageLoading(false)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  // Nothing can be dropped onto an image file
  setFlags(flags() & ~Qt::ItemIsDropEnabled);

  // The image file is unchanged
  fileChanged = false;

  // Does the file exits?
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return;

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &playlistItemImageFile::fileSystemWatcherFileChanged);

  // Install a file watcher if file watching is active.
  updateFileWatchSetting();
}

void playlistItemImageFile::loadFrame(int frameIndex, bool playing, bool loadRawdata)
{
  Q_UNUSED(frameIndex);
  Q_UNUSED(playing);
  Q_UNUSED(loadRawdata);

  imageLoading = true;
  frame.loadCurrentImageFromFile(plItemNameOrFileName);
  imageLoading = false;
  needToLoadImage = false;

  emit signalItemChanged(true);
}

void playlistItemImageFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the raw file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemImageFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);
  
  // Append all the properties of the raw file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);
  
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemImageFile.
*/
playlistItemImageFile *playlistItemImageFile::newplaylistItemImageFile(const QDomElementYUView &root, const QString &playlistFilePath)
{
  // Parse the DOM element. It should have all values of a playlistItemImageFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  
  // check if file with absolute path exists, otherwise check relative path
  QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  playlistItemImageFile *newImage = new playlistItemImageFile(filePath);
  
  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newImage);
  
  return newImage;
}

void playlistItemImageFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  Q_UNUSED(frameIdx);
  
  if (!frame.isFormatValid())
  {
    // The image could not be loaded. Draw a text instead.
    // Get the size of the text and create a QRect of that size which is centered at (0,0)
    QFont displayFont = painter->font();
    displayFont.setPointSizeF(painter->font().pointSizeF() * zoomFactor);
    painter->setFont(displayFont);
    QSize textSize = painter->fontMetrics().size(0, IMAGEFILE_ERROR_TEXT);
    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(QPoint(0,0));

    // Draw the text
    painter->drawText(textRect, IMAGEFILE_ERROR_TEXT);
  }
  else
    // Draw the frame
    frame.drawFrame(painter, zoomFactor, drawRawData);
}

void playlistItemImageFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  const QList<QByteArray> formats = QImageReader::supportedImageFormats();

  QString filter = "Static Image (";
  for (auto &fmt : formats)
  {
    QString formatString = QString(fmt);
    allExtensions.append(formatString);
    filter += "*." + formatString + " ";
  }

  if (filter.endsWith(' '))
    filter.chop(1);

  filter += ")";

  filters.append(filter);
}

ValuePairListSets playlistItemImageFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  ValuePairListSets newSet;
  newSet.append("RGB", frame.getPixelValues(pixelPos, frameIdx));
  return newSet;
}

infoData playlistItemImageFile::getInfo() const
{
  infoData info("Image Info");

  info.items.append(infoItem("File", plItemNameOrFileName));
  if (frame.isFormatValid())
  {
    QSize frameSize = frame.getFrameSize();
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(frameSize.width()).arg(frameSize.height()), "The video resolution in pixel (width x height)"));
    QImage img = frame.getCurrentFrameAsImage();
    info.items.append(infoItem("Bit depth", QString::number(img.depth()), "The bit depth of the image."));
  }
  else if (isLoading())
    info.items.append(infoItem("Status", "Loading...", "The image is being loaded. Please wait."));
  else
    info.items.append(infoItem("Status", "Error", "There was an error loading the image."));

  return info;
}

void playlistItemImageFile::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(plItemNameOrFileName);
  else
    fileWatcher.removePath(plItemNameOrFileName);
}
