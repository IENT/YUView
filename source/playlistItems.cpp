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

#include <QStringList>
#include "playlistItems.h"

namespace playlistItems
{
  void getSupportedFormatsFilters(QStringList &filters)
  {
    QStringList allExtensions, filtersList;

    playlistItemRawFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemHEVCFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);
    playlistItemStatisticsFile::getSupportedFileExtensions(allExtensions, filtersList);

    // Append the filter for playlist files
    allExtensions.append("yuvplaylist");
    filtersList.append("YUView playlist file (*.yuvplaylist)");

    // Now build the list of filters. First append the all files filter
    QString allFiles = "All supported file formats (";
    foreach(QString ext, allExtensions)
    {
      allFiles += "*." + ext + " ";
    }
    if (allFiles.endsWith(' '))
      allFiles.chop(1);
    allFiles += ")";

    filters.append(allFiles);
    filters.append(filtersList);
  }

  playlistItem *createPlaylistItemFromFile(const QString &fileName)
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

    // Check playlistItemHEVCFile
    {
      QStringList allExtensions, filtersList;
      playlistItemHEVCFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemHEVCFile *newRawFile = new playlistItemHEVCFile(fileName);
        return newRawFile;
      }
    }

    // Check playlistItemImageFile / playlistItemImageFileSequence
    {
      QStringList allExtensions, filtersList;
      playlistItemImageFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        // This is definitely an image file. But could it also be an image file sequence?
        if (playlistItemImageFileSequence::isImageSequence(fileName))
        {
          // This is not only one image, but a sequence of images. Open it as a file sequence
          playlistItemImageFileSequence *newSequence = new playlistItemImageFileSequence(fileName);
          return newSequence;
        }
        else
        {
          playlistItemImageFile *newRawFile = new playlistItemImageFile(fileName);
          return newRawFile;
        }
      }
    }

    // Check playlistItemStatisticsFile
    {
      QStringList allExtensions, filtersList;
      playlistItemStatisticsFile::getSupportedFileExtensions(allExtensions, filtersList);

      if (allExtensions.contains(ext))
      {
        playlistItemStatisticsFile *newRawFile = new playlistItemStatisticsFile(fileName);
        return newRawFile;
      }
    }
    
    // Unknown file type
    return NULL;
  }

  // Load one playlist item. Load it and return it. This function is seperate so it can be called
  // recursively if an item has children.
  playlistItem *loadPlaylistItem(const QDomElement &elem, const QString &filePath)
  {
    playlistItem *newItem = NULL;
    bool parseChildren = false;

    // Parse the item
    if (elem.tagName() == "playlistItemRawFile")
    {
      // This is a playlistItemYUVFile. Create a new one and add it to the playlist
      newItem = playlistItemRawFile::newplaylistItemRawFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemHEVCFile")
    {
      // Load the playlistItemHEVCFile
      newItem = playlistItemHEVCFile::newplaylistItemHEVCFile(elem, filePath);
    }
    else if (elem.tagName() == "playlistItemStatisticsFile")
    {
      // Load the playlistItemStatisticsFile
      newItem = playlistItemStatisticsFile::newplaylistItemStatisticsFile(elem, filePath);
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

    if (newItem != NULL && parseChildren)
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

      newItem->updateChildItems();
    }

    return newItem;
  }
}