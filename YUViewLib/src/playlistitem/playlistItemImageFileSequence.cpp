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

#include "playlistItemImageFileSequence.h"

#include <QImageReader>
#include <QSettings>
#include <QUrl>

#include "common/functions.h"
#include "filesource/fileSource.h"

playlistItemImageFileSequence::playlistItemImageFileSequence(const QString &rawFilePath)
  : playlistItemWithVideo(rawFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  setIcon(0, functions::convertIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  loadPlaylistFrameMissing = false;
  isFrameLoading = false;

  // Create the video handler
  video.reset(new videoHandler());

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();

  // Connect the video signalRequestFrame to this::loadFrame
  connect(video.data(), &videoHandler::signalRequestFrame, this, &playlistItemImageFileSequence::slotFrameRequest);
  
  if (!rawFilePath.isEmpty())
  {
    // Get the frames to use as a sequence
    fillImageFileList(imageFiles, rawFilePath);

    setInternals(rawFilePath);
  }

  // No file changed yet
  fileChanged = false;

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &playlistItemImageFileSequence::fileSystemWatcherFileChanged);

  // Install a file watcher if file watching is active.
  updateSettings();
}

bool playlistItemImageFileSequence::isImageSequence(const QString &filePath)
{
  QStringList files;
  fillImageFileList(files, filePath);
  return files.count() > 1;
}

void playlistItemImageFileSequence::fillImageFileList(QStringList &imageFiles, const QString &filePath)
{
  // See if the filename ends with a number
  QFileInfo fi(filePath);
  QString fileName = fi.fileName();
  QString base = fi.baseName();

  int lastN = 0;
  for (int i = base.count() - 1; i >= 0; i--)
  {
    // Get the char and see if it is a number
    if (base[i].isDigit())
      lastN++;
    else
      break;
  }

  if (lastN == 0)
  {
    // No number at the end of the file name
    imageFiles.append(filePath);
    return;
  }

  // The base name without the indexing number at the end
  QString absBaseName = base.left(base.count() - lastN);

  // List all files in the directory and get all that have the same pattern.
  QDir currentDir(fi.path());
  const QFileInfoList fileList = currentDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  QMap<int, QString> unsortedFiles;
  for (auto &file : fileList)
  {
    if (file.baseName().startsWith(absBaseName) && file.suffix() == fi.suffix())
    {
      // Check if the remaining part is all digits
      QString remainder = file.baseName().right(file.baseName().length() - absBaseName.length());
      bool isNumber;
      int num = remainder.toInt(&isNumber);
      if (isNumber)
        unsortedFiles.insert(num, file.absoluteFilePath());
    }
  }

  int count = unsortedFiles.count();
  int i = 0;
  while(count > 0 && i < INT_MAX)
  {
    if (unsortedFiles.contains(i))
    {
      imageFiles.append(unsortedFiles[i]);
      count--;
    }
    i++;
  }
}

void playlistItemImageFileSequence::createPropertiesWidget()
{
  // Absolutely always only call this once
  assert(!propertiesWidget);

  preparePropertiesWidget(QStringLiteral("playlistItemRawFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  // First add the parents controls (first video controls (width/height...)
  vAllLaout->addLayout(createPlaylistItemControls());
    
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);
}

infoData playlistItemImageFileSequence::getInfo() const
{
  infoData info("Image Sequence Info");

  if (video->isFormatValid())
  {
    QSize videoSize = video->getFrameSize();
    info.items.append(infoItem("Num Frames", QString::number(getNumberFrames())));
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixels (width x height)"));
  }
  else
    info.items.append(infoItem("Status", "Error", "There was an error loading the image."));
  
  if (loadPlaylistFrameMissing)
    info.items.append(infoItem("Warning", "Frames missing", "At least one frame could not be found when loading from playlist."));

  return info;
}

void playlistItemImageFileSequence::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemImageFileSequence");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Put a list of all input files into the playlist
  for (int i = 0; i < imageFiles.length(); i++)
  {
    // Determine the relative path to the file. We save both in the playlist.
    QUrl fileURL(imageFiles[i]);
    fileURL.setScheme("file");
    QString relativePath = playlistDir.relativeFilePath(imageFiles[i]);

    // Append the relative and absolute path to the file
    d.appendProperiteChild(QString("file%1_absolutePath").arg(i), fileURL.toString());
    d.appendProperiteChild(QString("file%1_relativePath").arg(i), relativePath);
  }
  
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemRawFile.
*/
playlistItemImageFileSequence *playlistItemImageFileSequence::newplaylistItemImageFileSequence(const YUViewDomElement &root, const QString &playlistFilePath)
{
  playlistItemImageFileSequence *newSequence = new playlistItemImageFileSequence();

  // Parse all the image files in the sequence
  int i = 0;
  while (true)
  {
    // Parse the DOM element. It should have all values of a playlistItemRawFile
    QString absolutePath = root.findChildValue(QString("file%1_absolutePath").arg(i));
    QString relativePath = root.findChildValue(QString("file%1_relativePath").arg(i));

    if (absolutePath.isEmpty() && relativePath.isEmpty())
      break;
  
    // check if file with absolute path exists, otherwise check relative path
    QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
    
    // Check if the file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
      // The file does not exist
      //qDebug() << "Error while loading playlistItemImageFileSequence. The file " << absolutePath << "could not be found.";
      newSequence->loadPlaylistFrameMissing = true;
    }

    // Add the file to the list of files
    newSequence->imageFiles.append(filePath);
    i++;
  }
  
  // Load the propertied of the playlistItemIndexed
  playlistItem::loadPropertiesFromPlaylist(root, newSequence);
  
  newSequence->setInternals(newSequence->imageFiles[0]);

  return newSequence;
}

void playlistItemImageFileSequence::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
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

void playlistItemImageFileSequence::slotFrameRequest(int frameIdxInternal, bool caching)
{
  Q_UNUSED(caching);

  // Does the index/file exist?
  if (frameIdxInternal < 0 || frameIdxInternal >= imageFiles.count())
    return;
  QFileInfo fileInfo(imageFiles[frameIdxInternal]);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return;
  
  // Load the given frame
  video->requestedFrame = QImage(imageFiles[frameIdxInternal]);
  video->requestedFrame_idx = frameIdxInternal;
}

void playlistItemImageFileSequence::setInternals(const QString &filePath)
{
  // Set start end frame and frame size if it has not been set yet.
  if (startEndFrame == indexRange(-1,-1))
    startEndFrame = getStartEndFrameLimits();

  // Open frame 0 and set the size of it
  QImage frame0 = QImage(imageFiles[0]);
  video->setFrameSize(frame0.size());

  cachingEnabled = false;  

  // Set the internal name
  QFileInfo fi(filePath);
  QString fileName = fi.fileName();
  QString base = fi.baseName();

  for (int i = base.count() - 1; i >= 0; i--)
  {
    // Get the char and see if it is a number
    if (base[i].isDigit())
      base.replace(i, 1, 'x');
    else
      break;
  }

  internalName = QString(fi.path()) + base + "." + fi.suffix();
  setName(internalName);
}

void playlistItemImageFileSequence::reloadItemSource()
{
  // Clear the video's buffers. The video will ask to reload the images.
  video->invalidateAllBuffers();
}

void playlistItemImageFileSequence::updateSettings()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    // Install watchers for all image files.
    fileWatcher.addPaths(imageFiles);
  else
    // Remove watchers for all image files.
    fileWatcher.removePaths(imageFiles);
}
