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

#include <QInputDialog>
#include <QMessageBox>
#include <QStringList>

namespace playlistItems
{
  QStringList getSupportedFormatsFilters()
  {
    QStringList allExtensions, filtersList;

    playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemRawCodedVideo::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemFFmpegFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemStatisticsCSVFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemStatisticsVTMBMSFile::getSupportedFileExtensions(allExtensions, filtersList);

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

    playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemRawCodedVideo::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemFFmpegFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemStatisticsCSVFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemStatisticsVTMBMSFile::getSupportedFileExtensions(allExtensions, filtersList);

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
    QString ext = fi.suffix().toLower();

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

    // Check playlistItemRawCodedVideo
    {
      QStringList allExtensions, filtersList;
      playlistItemRawCodedVideo::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemRawCodedVideo::decoderEngine engine = playlistItemRawCodedVideo::askForDecoderEngine(parent);
        if (engine == playlistItemRawCodedVideo::decoderInvalid)
          return nullptr;
        playlistItemRawCodedVideo *newRawCodedVideo = new playlistItemRawCodedVideo(fileName, 0, engine);
        return newRawCodedVideo;
      }
    }

    // Check playlistItemFFmpegFile
    {
      QStringList allExtensions, filtersList;
      playlistItemFFmpegFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemFFmpegFile *newFFMPEGFile = new playlistItemFFmpegFile(fileName);
        return newFFMPEGFile;
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
          QMessageBox::StandardButton choice = QMessageBox::question(parent, "Open image sequence", "This image can be opened as an image sequence. Do you want to open it as an image sequence (Yes) or as a single static image (No)?\n", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
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

    // Check playlistItemStatisticsCSVFile
    {
      QStringList allExtensions, filtersList;
      playlistItemStatisticsCSVFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemStatisticsCSVFile *newStatFile = new playlistItemStatisticsCSVFile(fileName);
        return newStatFile;
      }
    }

    // Check playlistItemVTMBMSStatisticsFile
    {
      QStringList allExtensions, filtersList;
      playlistItemStatisticsVTMBMSFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemStatisticsVTMBMSFile *newStatFile = new playlistItemStatisticsVTMBMSFile(fileName);
        return newStatFile;
      }
    }

    // Unknown file type extension. Ask the user as what file type he wants to open this file.
    QStringList types = QStringList() << "Raw YUV File" << "Raw RGB File" << "HEVC File (Raw Annex-B)" << "FFmpeg file" << "Statistics File" << "VTM/BMS Statistics File";
    bool ok;
    QString asType = QInputDialog::getItem(parent, "Select file type", "The file type could not be determined from the file extension. Please select the type of the file.", types, 0, false, &ok);
    if (ok && !asType.isEmpty())
    {
      if (asType == types[0] || asType == types[1])
      {
        // Raw YUV/RGB File
        QString fmt = (asType == types[0]) ? "yuv" : "rgb";
        playlistItemRawFile *newRawFile = new playlistItemRawFile(fileName, QSize(-1, -1), QString(), fmt);
        return newRawFile;
      }
      else if (asType == types[2])
      {
        // HEVC file
        playlistItemRawCodedVideo::decoderEngine engine = playlistItemRawCodedVideo::askForDecoderEngine(parent);
        if (engine == playlistItemRawCodedVideo::decoderInvalid)
          return nullptr;
        playlistItemRawCodedVideo *newRawCodedVideo = new playlistItemRawCodedVideo(fileName, 0, engine);
        return newRawCodedVideo;
      }
      else if (asType == types[3])
      {
        // FFmpeg file
        playlistItemFFmpegFile *newFFmpegFile = new playlistItemFFmpegFile(fileName);
        return newFFmpegFile;
      }
      else if (asType == types[4])
      {
        // Statistics File
        playlistItemStatisticsFile *newStatFile = new playlistItemStatisticsFile(fileName);
        return newStatFile;
      }
      else if (asType == types[5])
      {
        // Statistics File
        playlistItemStatisticsVTMBMSFile *newStatFile = new playlistItemStatisticsVTMBMSFile(fileName);
        return newStatFile;
      }
    }

    return nullptr;
  }

  // Load one playlist item. Load it and return it. This function is separate so it can be called
  // recursively if an item has children.
  playlistItem *loadPlaylistItem(const QDomElement &elem, const QString &filePath)
  {
    playlistItem *newItem = nullptr;
    bool parseChildren = false;

    // Parse the item
    if (elem.tagName() == "playlistItemRawFile")
    {
      // This is a playlistItemYUVFile. Create a new one and add it to the playlist
      newItem = playlistItemRawFile::newplaylistItemRawFile(elem, filePath);
    }
    // For backwards compability (the playlistItemRawCodedVideo used to be called playlistItemHEVCFile)
    else if (elem.tagName() == "playlistItemHEVCFile" || elem.tagName() == "playlistItemRawCodedVideo")
    {
      // Load the playlistItemHEVCFile
      newItem = playlistItemRawCodedVideo::newplaylistItemRawCodedVideo(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemFFmpegFile")
    {
      // Load the playlistItemFFmpegFile
      newItem = playlistItemFFmpegFile::newplaylistItemFFmpegFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemStatisticsCSVFile")
    {
      // Load the playlistItemStatisticsFile
      newItem = playlistItemStatisticsCSVFile::newplaylistItemStatisticsCSVFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemStatisticsVTMBMSFile")
    {
      // Load the playlistItemVTMBMSStatisticsFile
      newItem = playlistItemStatisticsVTMBMSFile::newplaylistItemStatisticsVTMBMSFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemText")
    {
      // This is a playlistItemText. Load it from file.
      newItem = playlistItemText::newplaylistItemText(elem);
    }
    else if (elem.tagName() == "playlistItemDifference")
    {
      // This is a playlistItemDifference. Load it from file.
      newItem = playlistItemDifference::newPlaylistItemDifference(elem);
      parseChildren = true;
    }
    else if (elem.tagName() == "playlistItemOverlay")
    {
      // This is a playlistItemOverlay. Load it from file.
      newItem = playlistItemOverlay::newPlaylistItemOverlay(elem, filePath);
      parseChildren = true;
    }
    else if (elem.tagName() == "playlistItemImageFile")
    {
      // This is a playlistItemImageFile. Load it.
      newItem = playlistItemImageFile::newplaylistItemImageFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemImageFileSequence")
    {
      // This is a playlistItemImageFileSequence. Load it.
      newItem = playlistItemImageFileSequence::newplaylistItemImageFileSequence(elem, filePath);
    }

    if (newItem != nullptr && parseChildren)
    {
      // The playlistItem can have children. Parse them.
      QDomNodeList children = elem.childNodes();

      for (int i = 0; i < children.length(); i++)
      {
        // Parse the child items
        QDomElement childElem = children.item(i).toElement();
        playlistItem *childItem = loadPlaylistItem(childElem, filePath);

        if (childItem)
          newItem->addChild(childItem);
      }

      playlistItemContainer *container = dynamic_cast<playlistItemContainer*>(newItem);
      if (container)
        container->updateChildItems();
    }

    return newItem;
  }
}
