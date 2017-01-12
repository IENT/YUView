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

  virtual indexRange getFrameIndexRange() const Q_DECL_OVERRIDE { return startEndFrame; }

  // Overload from playlistItemVideo. 
  virtual double getFrameRate() const Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == nullptr) ? 0 : getFirstChildPlaylistItem()->getFrameRate(); }
  virtual QSize  getSize()      const Q_DECL_OVERRIDE; // Return the size of the emptyText. Overload to do something more meaningful.
  virtual int    getSampling()  const Q_DECL_OVERRIDE { return (getFirstChildPlaylistItem() == nullptr) ? 1 : getFirstChildPlaylistItem()->getSampling(); }

  // Call this function to draw the text emptyText. 
  void drawEmptyContainerText(QPainter *painter, double zoomFactor);
  
  // Overload from playlistItemIndexed
  virtual indexRange getStartEndFrameLimits() const Q_DECL_OVERRIDE;

  // An item will be deleted. Disconnect the signals/slots of this item
  virtual void itemAboutToBeDeleted(playlistItem *item) Q_DECL_OVERRIDE;

  // ----- Detection of source/file change events -----
  virtual bool isSourceChanged()        Q_DECL_OVERRIDE;  // Return if one of the child item's source changed.
  virtual void reloadItemSource()       Q_DECL_OVERRIDE;  // Reload all child items
  virtual void updateFileWatchSetting() Q_DECL_OVERRIDE;  // Install/remove the file watchers.

protected slots:
  virtual void childChanged(bool redraw);

protected:
  // How many items can this container contain? (-1 no limit)
  int maxItemCount;

  // How do we calculate the frame index ranges? If this is true, the maximum of all items will be used,
  // if it is false, the minimum will be used (the overlapping part)
  bool frameLimitsMax;

  // Return the first child item (as playlistItem) or nullptr if there is no child.
  playlistItem *getFirstChildPlaylistItem() const;

  // We keep a list of pointers to all child items. This way we can directly connect to the children signals
  void updateChildList();
  QList<playlistItem*> childList;
  bool childLlistUpdateRequired;

  // The current index range. Don't forget to update this when (one of ) the children change(s).
  indexRange startEndFrame;
  
  // This text is drawn if the container is empty
  QString emptyText;

  // Create a layout for the container item. Since this is filled depending on the child items, it is just an empty layout in the beginning.
  QLayout *createContainerItemControls() { return &containerStatLayout; }
  QVBoxLayout containerStatLayout;
  QSpacerItem *vSpacer;

  // Save all child items to playlist
  void savePlaylistChildren(QDomElement &root, const QDir &playlistDir) const;
};

#endif // PLAYLISTITEMCONTAINER_H
