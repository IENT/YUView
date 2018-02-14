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
  setIcon(0, convertIcon(":img_television.png"));
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
  updateSettings();
}

void playlistItemImageFile::loadFrame(int frameIndex, bool playing, bool loadRawdata, bool emitSignals)
{
  Q_UNUSED(frameIndex);
  Q_UNUSED(playing);
  Q_UNUSED(loadRawdata);

  frame.loadCurrentImageFromFile(plItemNameOrFileName);
  imageLoading = false;
  needToLoadImage = false;

  if (emitSignals)
    emit signalItemChanged(true, RECACHE_NONE);
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
  Q_UNUSED(frameIdx);

  ValuePairListSets newSet;
  newSet.append("RGB", frame.getPixelValues(pixelPos, -1));
  return newSet;
}

infoData playlistItemImageFile::getInfo() const
{
  infoData info("Image Info");
  if (imageLoading == false)
  {
    info.items.append(infoItem("File", plItemNameOrFileName));

    // Append the file information (path, date created, file size...)
    info.items.append(frame.getFileInfoList());
  }


  /**
  if (frame.isFormatValid())
  {
    QSize frameSize = frame.getFrameSize();
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(frameSize.width()).arg(frameSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Bit depth", QString::number(frame.getImageBitDepth()), "The bit depth of the image."));
  }
  else if (isLoading())
    info.items.append(infoItem("Status", "Loading...", "The image is being loaded. Please wait."));
  else
    info.items.append(infoItem("Status", "Error", "There was an error loading the image."));
  **/

  return info;
}

void playlistItemImageFile::updateSettings()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(plItemNameOrFileName);
  else
    fileWatcher.removePath(plItemNameOrFileName);
}


QMap<QString, QList<QList<QVariant>>>* playlistItemImageFile::getData (indexRange range, bool reset)
{
  QMap<QString, QList<QList<QVariant>>> data;

  if(reset)
  {
    this->mStatisticData.clear();
    QSize imageSize = this->getSize();

    for (int frame = range.first; frame <= range.second; frame++)
    {
      QMap<QString, QList<QVariant>> map;

      for (int width = 0; width < 50; width++)
      {
        for (int height = 0; height < 50; height++)
        {
          ValuePairListSets pixelValueSets = this->getPixelValues(QPoint(width, height),  frame);

          ValuePairList pixelValueList = pixelValueSets.at(0).second;

          for (int pos = 0; pos < pixelValueList.count(); pos++)
          {
            ValuePair pixelValue = pixelValueList.at(pos);

            // getting the old / new datalist
            QList<QVariant> dataList = map.value(pixelValue.first);

            // appending the new data
            dataList.append(QVariant::fromValue(pixelValue.second));          

            // insert new datalist to refresh
            map.insert(pixelValue.first, dataList);
          }
        }

        foreach (QString key, map.keys())
        {
          QList<QVariant> dataList = map.value(key);
          QList<QList<QVariant>> resultList;
          resultList.append(dataList);
          data.insert(key, resultList);
        }

      }
    }

  this->mStatisticData = data;

  //QList<QList<QVariant>> listen = this->mStatisticData.value("R");
  //QList<QVariant> liste = listen.at(0);
  //qDebug() << liste.count();

  }

  return &this->mStatisticData;
}

QList<collectedData>* playlistItemImageFile::sortAndCategorizeData(const QString aType, const int aFrameIndex)
{
  Q_UNUSED(aFrameIndex)

  QList<QPair<QVariant, int>*> mValue;

  //prepare the result
  QMap<QString, QMap<int, int*>*>* dataMap = new QMap<QString, QMap<int, int*>*>;

  //check if data was loaded
  if(!(&this->mStatisticData))
    this->getData(this->getFrameIdxRange(), true);

  // getting allData from the type
  QList<QList<QVariant>> allData = this->mStatisticData.value(aType);

  // getting the data depends on the actual selected frameIndex / POC
  //QList<QVariant> data = allData.at(aFrameIndex);

  QList<collectedData>* resultData = new QList<collectedData>;
  collectedData data;
  QString key = aType;
  data.mLabel = key;

  //qDebug() << this->mStatisticData.values(aType);

  QList<QList<QVariant>> resultLists = this->mStatisticData.value(key);
  QMap<int, int> map;

  for (int i = 0; i < resultLists.count(); i++)
  {
    QList<QVariant> dataList = resultLists.at(i);

    for (int j = 0; j < dataList.count(); j++)
    {
      QVariant variant = dataList.at(j);

      int colorValue = variant.toInt();

      int value = map.value(colorValue);
      value++;

      map.insert(colorValue, value);

      //qDebug() << map.value(245);

    }

  }

  foreach (int key, map.keys())
  {
    int* arr = new int[2];
    arr[0] = key;
    arr[1] = map.value(key);

    data.mValueList.append(arr);
  }



  QList<collectedData>* result = new QList<collectedData>();
  result->append(data);


  return result;
}


QList<collectedData>* playlistItemImageFile::sortAndCategorizeDataByRange(const QString aType, const indexRange aRange)
{
  //if we have the same frame --> just one frame we look at
  if(aRange.first == aRange.second) // same frame --> just one frame same as current frame
    return this->sortAndCategorizeData(aType, aRange.first);

  QList<collectedData>* result = new QList<collectedData>();
  return result;
}

