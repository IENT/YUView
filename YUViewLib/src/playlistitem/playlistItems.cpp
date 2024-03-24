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

namespace
{

playlistItem *openImageFileOrSequence(QWidget *parent, const QString &fileName)
{
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
    return new playlistItemImageFileSequence(fileName);
  else
    return new playlistItemImageFile(fileName);
}

playlistItem *guessFileTypeFromFileAndCreatePlaylistItem(QWidget *parent, const QString &fileName)
{
  QFileInfo  fi(fileName);
  const auto extension = fi.suffix().toLower();

  auto checkForExtension = [&extension](auto getSupportedFileExtensions) {
    QStringList allExtensions, filtersList;
    getSupportedFileExtensions(allExtensions, filtersList);
    return allExtensions.contains(extension);
  };

  if (checkForExtension(playlistItemRawFile::getSupportedFileExtensions))
    return new playlistItemRawFile(fileName);
  if (checkForExtension(playlistItemCompressedVideo::getSupportedFileExtensions))
    return new playlistItemCompressedVideo(fileName);
  if (checkForExtension(playlistItemImageFile::getSupportedFileExtensions))
    return openImageFileOrSequence(parent, fileName);
  if (checkForExtension(playlistItemStatisticsFile::getSupportedFileExtensions))
    return new playlistItemStatisticsFile(fileName);

  return {};
}

playlistItem *askUserForFileTypeAndCreatePlalistItem(QWidget *  parent,
                                                     QString    fileName,
                                                     const bool determineFileTypeAutomatically)
{
  const auto types = QStringList() << "Raw YUV File"
                                   << "Raw RGB File"
                                   << "Compressed file"
                                   << "Image file"
                                   << "Statistics File CSV"
                                   << "Statistics File VTMBMS";
  bool    ok{};
  QString message = "Unable to detect file type from file extension.";
  if (!determineFileTypeAutomatically)
    message = "File type detection from extension is disabled (see Settings -> General).";
  const auto asType = QInputDialog::getItem(parent,
                                            "Select file type",
                                            message + " Please select how to open the file.",
                                            types,
                                            0,
                                            false,
                                            &ok);
  if (ok && !asType.isEmpty())
  {
    if (asType == types[0] || asType == types[1])
    {
      const QString fmt = (asType == types[0]) ? "yuv" : "rgb";
      return new playlistItemRawFile(fileName, QSize(-1, -1), QString(), fmt);
    }
    else if (asType == types[2])
    {
      return new playlistItemCompressedVideo(fileName);
    }
    else if (asType == types[3])
    {
      return openImageFileOrSequence(parent, fileName);
    }
    else if (asType == types[4] || asType == types[5])
    {
      auto openMode = (asType == types[3] ? playlistItemStatisticsFile::OpenMode::CSVFile
                                          : playlistItemStatisticsFile::OpenMode::VTMBMSFile);
      return new playlistItemStatisticsFile(fileName, openMode);
    }
  }

  return nullptr;
}

} // namespace

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
    allFiles += "*." + ext + " ";
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
    nameFilters.append(QString("*.") + extension);

  return nameFilters;
}

playlistItem *createPlaylistItemFromFile(QWidget *parent, const QString &fileName)
{
  QSettings  settings;
  const auto determineFileTypeAutomatically = settings.value("AutodetectFileType", true).toBool();

  playlistItem *newPlaylistItem{};
  if (determineFileTypeAutomatically)
    newPlaylistItem = guessFileTypeFromFileAndCreatePlaylistItem(parent, fileName);
  if (!newPlaylistItem)
    newPlaylistItem =
        askUserForFileTypeAndCreatePlalistItem(parent, fileName, determineFileTypeAutomatically);

  return newPlaylistItem;
}

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
  else if (tag == "playlistItemStatisticsFile" || tag == "playlistItemStatisticsCSVFile" ||
           tag == "playlistItemStatisticsVTMBMSFile")
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

  if (newItem && parseChildren)
  {
    const auto children = elem.childNodes();

    for (int i = 0; i < children.length(); i++)
    {
      const auto childElement = children.item(i).toElement();
      if (const auto childItem = loadPlaylistItem(childElement, filePath))
        newItem->addChild(childItem);
    }

    if (const auto container = dynamic_cast<playlistItemContainer *>(newItem))
      container->updateChildItems();
  }

  return newItem;
}

} // namespace playlistItems
