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

#ifndef PLAYLISTITEMCONTAINER_H
#define PLAYLISTITEMCONTAINER_H

#include <QVBoxLayout>
#include "playlistItem.h"
#include "typedef.h"

class playlistItemContainer : public playlistItem
{
  Q_OBJECT

public:
  playlistItemContainer(const QString &itemNameOrFileName);

  // We accept drops if the maximum number of items is no reached yet
  virtual bool acceptDrops(playlistItem *draggingItem) const Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. 
  virtual double getFrameRate() const Q_DECL_OVERRIDE { return (getChildPlaylistItem(0) == nullptr) ? 0 : getChildPlaylistItem(0)->getFrameRate(); }
  virtual int    getSampling()  const Q_DECL_OVERRIDE { return (getChildPlaylistItem(0) == nullptr) ? 1 : getChildPlaylistItem(0)->getSampling(); }

  // The children of this item might have changed. If yes, update the properties of this item
  // and emit the signalItemChanged(true).
  void updateChildItems() { childLlistUpdateRequired = true; emit signalItemChanged(true, RECACHE_NONE); }

  // An item will be deleted. Disconnect the signals/slots of this item and remove it from the QTreeWidgetItem (takeItem)
  virtual void itemAboutToBeDeleted(playlistItem *item) Q_DECL_OVERRIDE;

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE;  // Return if one of the child item's source changed.
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;  // Reload all child items
  virtual void updateSettings()         Q_DECL_OVERRIDE;  // Install/remove the file watchers.

    // Return a list containing this item and all child items (if any).
  QList<playlistItem*> getAllChildPlaylistItems() const;

  // Return a list of all the child items (recursively) and remove (takeChild) them from the QTreeWidget tree 
  // structure and from the internal childList.
  QList<playlistItem*> takeAllChildItemsRecursive();

  // Overload from playlistItemIndexed
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE;

protected slots:
  virtual void childChanged(bool redraw, recacheIndicator recache);

protected:
  
  // How many items can this container contain? (-1 no limit)
  int maxItemCount;

  // How do we calculate the frame index ranges? If this is true, the maximum of all items will be used,
  // if it is false, the minimum will be used (the overlapping part)
  bool frameLimitsMax;

  // Return a pointer to the playlist item or null if the item does not exist (check childCount() first)
  playlistItem *getChildPlaylistItem(int index) const;

  // We keep a list of pointers to all child items. This way we can directly connect to the children signals
  void updateChildList();
  bool childLlistUpdateRequired;
    
  // Create a layout for the container item. Since this is filled depending on the child items, it is just an empty layout in the beginning.
  QLayout *createContainerItemControls() { return &containerStatLayout; }
  QVBoxLayout containerStatLayout;

  // Save all child items to playlist
  void savePlaylistChildren(QDomElement &root, const QDir &playlistDir) const;
};

#endif // PLAYLISTITEMCONTAINER_H
