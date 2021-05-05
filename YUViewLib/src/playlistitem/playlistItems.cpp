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

#include "playlistItems.h"

#include "playlistItemCompressedVideo.h"
#include "playlistItemDifference.h"
#include "playlistItemImageFile.h"
#include "playlistItemImageFileSequence.h"
#include "playlistItemOverlay.h"
#include "playlistItemRawFile.h"
#include "playlistItemResample.h"
#include "playlistItemStatisticsFile.h"
#include "playlistItemText.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QStringList>

namespace playlistItems
{
QStringList getSupportedFormatsFilters()
{
  QStringList allExtensions, filtersList;

  playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemCompressedVideo::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemStatisticsFile::getSupportedFileExtensions(allExtensions, filtersList);

  // Append the filter for playlist files
  allExtensions.append("yuvplaylist");
  filtersList.append("YUView playlist file (*.yuvplaylist)");

  // Now build the list of filters. First append the all files filter
  QString allFiles = "All supported file formats (";
  for (auto const &ext : allExtensions)
  {
    allFiles += "*." + ext + " ";
  }
  if (allFiles.endsWith(' '))
    allFiles.chop(1);
  allFiles += ")";

  QStringList filters;
  filters.append(allFiles);
  filters.append(filtersList);
  filters.append("Any files (*)");
  return filters;
}

QStringList getSupportedNameFilters()
{
  QStringList allExtensions, filtersList;

  playlistItemCompressedVideo::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);
  playlistItemStatisticsFile::getSupportedFileExtensions(allExtensions, filtersList);

  // Append the filter for playlist files
  allExtensions.append("yuvplaylist");
  filtersList.append("YUView playlist file (*.yuvplaylist)");

  // Now build the list of name filters
  QStringList nameFilters;
  for (auto extension : allExtensions)
  {
    nameFilters.append(QString("*.") + extension);
  }

  return nameFilters;
}

playlistItem *createPlaylistItemFromFile(QWidget *parent, const QString &fileName)
{
  QFileInfo fi(fileName);
  QString   ext = fi.suffix().toLower();

  // Check playlistItemRawFile
  {
    QStringList allExtensions, filtersList;
    playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);

    if (allExtensions.contains(ext))
    {
      playlistItemRawFile *newRawFile = new playlistItemRawFile(fileName);
      return newRawFile;
    }
  }

  // Check playlistItemCompressedVideo
  {
    QStringList allExtensions, filtersList;
    playlistItemCompressedVideo::getSupportedFileExtensions(allExtensions, filtersList);

    if (allExtensions.contains(ext))
    {
      playlistItemCompressedVideo *newRawCodedVideo = new playlistItemCompressedVideo(fileName, 0);
      return newRawCodedVideo;
    }
  }

  // Check playlistItemImageFile / playlistItemImageFileSequence
  {
    QStringList allExtensions, filtersList;
    playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);

    if (allExtensions.contains(ext))
    {
      // This is definitely an image file. But could it also be an image file sequence?
      bool openAsImageSequence = false;
      if (playlistItemImageFileSequence::isImageSequence(fileName))
      {
        // This is not only one image, but a sequence of images. Ask the user how to open it.
        QMessageBox::StandardButton choice = QMessageBox::question(
            parent,
            "Open image sequence",
            "This image can be opened as an image sequence. Do you want to open it as an image "
            "sequence (Yes) or as a single static image (No)?\n",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);
        if (choice == QMessageBox::Yes)
          openAsImageSequence = true;
        else if (choice == QMessageBox::No)
          openAsImageSequence = false;
        else
          return nullptr;
      }

      if (openAsImageSequence)
      {
        // Open it as a file sequence
        playlistItemImageFileSequence *newSequence = new playlistItemImageFileSequence(fileName);
        return newSequence;
      }
      else
      {
        playlistItemImageFile *newImageFile = new playlistItemImageFile(fileName);
        return newImageFile;
      }
    }
  }

  // Check playlistItemStatisticsFile
  {
    QStringList allExtensions, filtersList;
    playlistItemStatisticsFile::getSupportedFileExtensions(allExtensions, filtersList);

    if (allExtensions.contains(ext))
    {
      playlistItemStatisticsFile *newStatFile = new playlistItemStatisticsFile(fileName);
      return newStatFile;
    }
  }

  // Unknown file type extension. Ask the user as what file type he wants to open this file.
  QStringList types = QStringList() << "Raw YUV File"
                                    << "Raw RGB File"
                                    << "Compressed file"
                                    << "Statistics File CSV"
                                    << "Statistics File VTMBMS";
  bool    ok;
  QString asType = QInputDialog::getItem(parent,
                                         "Select file type",
                                         "The file type could not be determined from the file "
                                         "extension. Please select the type of the file.",
                                         types,
                                         0,
                                         false,
                                         &ok);
  if (ok && !asType.isEmpty())
  {
    if (asType == types[0] || asType == types[1])
    {
      // Raw YUV/RGB File
      QString fmt        = (asType == types[0]) ? "yuv" : "rgb";
      auto    newRawFile = new playlistItemRawFile(fileName, QSize(-1, -1), QString(), fmt);
      return newRawFile;
    }
    else if (asType == types[2])
    {
      // Compressed video
      auto newRawCodedVideo = new playlistItemCompressedVideo(fileName, 0);
      return newRawCodedVideo;
    }
    else if (asType == types[3] || asType == types[4])
    {
      // Statistics File
      auto openMode = (asType == types[3] ? playlistItemStatisticsFile::OpenMode::CSVFile : playlistItemStatisticsFile::OpenMode::VTMBMSFile);
      auto newStatFile = new playlistItemStatisticsFile(fileName, openMode);
      return newStatFile;
    }
  }

  return nullptr;
}

// Load one playlist item. Load it and return it. This function is separate so it can be called
// recursively if an item has children.
playlistItem *loadPlaylistItem(const QDomElement &elem, const QString &filePath)
{
  playlistItem *newItem       = nullptr;
  bool          parseChildren = false;

  auto tag = elem.tagName();
  if (tag == "playlistItemRawFile")
  {
    newItem = playlistItemRawFile::newplaylistItemRawFile(elem, filePath);
  }
  // For backwards compability (playlistItemCompressedFile used to be called
  // playlistItemRawCodedVideo or playlistItemHEVCFile)
  else if (tag == "playlistItemCompressedVideo" || tag == "playlistItemCompressedFile" ||
           tag == "playlistItemFFmpegFile" || tag == "playlistItemHEVCFile" ||
           tag == "playlistItemRawCodedVideo")
  {
    newItem = playlistItemCompressedVideo::newPlaylistItemCompressedVideo(elem, filePath);
  }
  else if (tag == "playlistItemStatisticsFile" || tag == "playlistItemStatisticsCSVFile" || tag == "playlistItemStatisticsVTMBMSFile")
  {
    auto openMode = playlistItemStatisticsFile::OpenMode::Extension;
    if (tag == "playlistItemStatisticsCSVFile")
      openMode = playlistItemStatisticsFile::OpenMode::CSVFile;
    if (tag == "playlistItemStatisticsVTMBMSFile")
      openMode = playlistItemStatisticsFile::OpenMode::VTMBMSFile;

    newItem = playlistItemStatisticsFile::newplaylistItemStatisticsFile(elem, filePath, openMode);
  }
  else if (tag == "playlistItemText")
  {
    newItem = playlistItemText::newplaylistItemText(elem);
  }
  else if (tag == "playlistItemDifference")
  {
    newItem       = playlistItemDifference::newPlaylistItemDifference(elem);
    parseChildren = true;
  }
  else if (tag == "playlistItemOverlay")
  {
    newItem       = playlistItemOverlay::newPlaylistItemOverlay(elem, filePath);
    parseChildren = true;
  }
  else if (tag == "playlistItemImageFile")
  {
    newItem = playlistItemImageFile::newplaylistItemImageFile(elem, filePath);
  }
  else if (tag == "playlistItemImageFileSequence")
  {
    newItem = playlistItemImageFileSequence::newplaylistItemImageFileSequence(elem, filePath);
  }
  else if (tag == "playlistItemResample")
  {
    newItem       = playlistItemResample::newPlaylistItemResample(elem);
    parseChildren = true;
  }

  if (newItem != nullptr && parseChildren)
  {
    // The playlistItem can have children. Parse them.
    auto children = elem.childNodes();

    for (int i = 0; i < children.length(); i++)
    {
      // Parse the child items
      auto childElem = children.item(i).toElement();
      auto childItem = loadPlaylistItem(childElem, filePath);

      if (childItem)
        newItem->addChild(childItem);
    }

    auto container = dynamic_cast<playlistItemContainer *>(newItem);
    if (container)
      container->updateChildItems();
  }

  return newItem;
}
} // namespace playlistItems
