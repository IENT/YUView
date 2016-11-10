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

#include "playlistItemImageFileSequence.h"

#include <QImageReader>
#include <QDebug>
#include <QSettings>

playlistItemImageFileSequence::playlistItemImageFileSequence(QString rawFilePath)
  : playlistItemIndexed(rawFilePath)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  loadPlaylistFrameMissing = false;

  // Connect the video signalRequestFrame to this::loadFrame
  connect(&video, SIGNAL(signalRequestFrame(int)), this, SLOT(loadFrame(int)), Qt::DirectConnection);
  connect(&video, SIGNAL(signalHandlerChanged(bool,bool)), this, SLOT(slotEmitSignalItemChanged(bool,bool)));
  
  if (!rawFilePath.isEmpty())
  {
    // Get the frames to use as a sequence
    fillImageFileList(imageFiles, rawFilePath);

    setInternals(rawFilePath);
  }

  // No file changed yet
  fileChanged = false;

  connect(&fileWatcher, SIGNAL(fileChanged(const QString)), this, SLOT(fileSystemWatcherFileChanged(const QString)));

  // Install a file watcher if file watching is active.
  updateFileWatchSetting();
}

bool playlistItemImageFileSequence::isImageSequence(QString filePath)
{
  QStringList files;
  fillImageFileList(files, filePath);
  return files.count() > 1;
}

void playlistItemImageFileSequence::fillImageFileList(QStringList &imageFiles, QString filePath)
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
  QFileInfoList fileList = currentDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
  QMap<int, QString> unsortedFiles;
  foreach(QFileInfo file, fileList)
  {
    if (file.baseName().startsWith(absBaseName) && file.suffix() == fi.suffix())
    {
      // Check if the reamining part is all digits
      QString remainder = file.baseName().right( file.baseName().length() - absBaseName.length() );
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
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemRawFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  // First add the parents controls (first video controls (width/height...)
  vAllLaout->addLayout( createIndexControllers() );
    
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

QList<infoItem> playlistItemImageFileSequence::getInfoList()
{
  QList<infoItem> infoList;

  QSize videoSize = video.getFrameSize();
  infoList.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  infoList.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
  infoList.append(infoItem("Frames Cached",QString::number(video.getNrFramesCached())));
  
  if (loadPlaylistFrameMissing)
    infoList.append(infoItem("Warging","Frames missing", "At least one frame could not be found when loading from playlist."));

  return infoList;
}

void playlistItemImageFileSequence::savePlaylist(QDomElement &root, QDir playlistDir)
{
  QDomElementYUView d = root.ownerDocument().createElement("playlistItemImageFileSequence");

  // Append the properties of the playlistItemIndexed
  playlistItemIndexed::appendPropertiesToPlaylist(d);

  // Put a list of all input files into the playlist
  for (int i = 0; i < imageFiles.length(); i++)
  {
    // Determine the relative path to the file. We save both in the playlist.
    QUrl fileURL( imageFiles[i] );
    fileURL.setScheme("file");
    QString relativePath = playlistDir.relativeFilePath( imageFiles[i] );

    // Apppend the relative and absolute path to the file
    d.appendProperiteChild( QString("file%1_absolutePath").arg(i), fileURL.toString() );
    d.appendProperiteChild( QString("file%1_relativePath").arg(i), relativePath );
  }
  
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemRawFile.
*/
playlistItemImageFileSequence *playlistItemImageFileSequence::newplaylistItemImageFileSequence(QDomElementYUView root, QString playlistFilePath)
{
  playlistItemImageFileSequence *newSequence = new playlistItemImageFileSequence();

  // Parse all the image files in the sequence
  int i = 0;
  while (true)
  {
    // Parse the dom element. It should have all values of a playlistItemRawFile
    QString absolutePath = root.findChildValue( QString("file%1_absolutePath").arg(i) );
    QString relativePath = root.findChildValue( QString("file%1_relativePath").arg(i) );

    if (absolutePath.isEmpty() && relativePath.isEmpty())
      break;
  
    // check if file with absolute path exists, otherwise check relative path
    QString filePath = fileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
    
    // Check if the file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile())
    {
      // The file does not exist
      qDebug() << "Error while loading playlistItemImageFileSequence. The file " << absolutePath << "could not be found.";
      newSequence->loadPlaylistFrameMissing = true;
    }

    // Add the file to the list of files
    newSequence->imageFiles.append(filePath);
    i++;
  }
  
  // Load the propertied of the playlistItemIndexed
  playlistItemIndexed::loadPropertiesFromPlaylist(root, newSequence);
  
  newSequence->setInternals(newSequence->imageFiles[0]);

  return newSequence;
}

void playlistItemImageFileSequence::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  Q_UNUSED(playback);

  if (frameIdx != -1)
    video.drawFrame(painter, frameIdx, zoomFactor);
}

void playlistItemImageFileSequence::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  QList<QByteArray> formats = QImageReader::supportedImageFormats();

  QString filter = "Static Image (";
  foreach(QByteArray fmt, formats)
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

void playlistItemImageFileSequence::loadFrame(int frameIdx)
{
  if (frameIdx >= 0 && frameIdx < imageFiles.count())
  {
    video.requestedFrame = QPixmap(imageFiles[frameIdx]);
    video.requestedFrame_idx = frameIdx;
  }
}

void playlistItemImageFileSequence::setInternals(QString filePath)
{
  // Set start end frame and frame size if it has not been set yet.
  if (startEndFrame == indexRange(-1,-1))
    startEndFrame = getstartEndFrameLimits();

  // Open frame 0 and set the size of it
  QPixmap frame0 = QPixmap(imageFiles[0]);
  video.setFrameSize(frame0.size());

  cachingEnabled = true;  

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
  video.invalidateAllBuffers();
}

void playlistItemImageFileSequence::updateFileWatchSetting()
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