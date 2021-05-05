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

#include "playlistItemContainer.h"

#include "statistics/StatisticUIHandler.h"
#include <QPainter>
#include <algorithm>

playlistItemContainer::playlistItemContainer(const QString &itemNameOrFileName)
    : playlistItem(itemNameOrFileName, Type::Indexed)
{
  this->setFlags(this->flags() | Qt::ItemIsDropEnabled);
  this->containerStatLayout.setContentsMargins(0, 0, 0, 0);
}

bool playlistItemContainer::acceptDrops(playlistItem *) const
{
  return (this->maxItemCount == -1 || childCount() < this->maxItemCount);
}

void playlistItemContainer::updateChildList()
{
  // Disconnect all signalItemChanged events from the children to the "childChanged" function from
  // this container. All the original connections from the items to the playlistTreeWidget will be
  // retained. The user can still select the child items individually so the individual connections
  // must also be there.
  for (int i = 0; i < this->childCount(); i++)
  {
    auto item = this->getChildPlaylistItem(i);
    if (item)
    {
      disconnect(
          item, &playlistItem::signalItemChanged, this, &playlistItemContainer::childChanged);
      if (item->properties().providesStatistics)
        item->getStatisticsUIHandler()->deleteSecondaryStatisticsHandlerControls();
    }
  }

  // Connect all child items
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = this->getChildPlaylistItem(i);
    if (childItem)
      this->connect(
          childItem, &playlistItem::signalItemChanged, this, &playlistItemContainer::childChanged);
  }

  // Remove all widgets (the lines and spacer) that are still in the layout
  QLayoutItem *childLayout;
  while ((childLayout = containerStatLayout.takeAt(0)) != 0)
    delete childLayout;

  // Now add the statistics controls from all items that can provide statistics
  for (int i = 0; i < childCount(); i++)
  {
    auto item = getChildPlaylistItem(i);

    if (item && item->properties().providesStatistics)
    {
      // Add a line and the statistics controls also to the overlay widget
      auto line = new QFrame;
      line->setObjectName(QStringLiteral("line"));
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);

      containerStatLayout.addWidget(line);
      containerStatLayout.addWidget(
          item->getStatisticsUIHandler()->getSecondaryStatisticsHandlerControls());
    }
  }

  // Finally, we have to update the start/end Frame
  childChanged(false, RECACHE_NONE);
  emit signalItemChanged(true, RECACHE_NONE);

  childLlistUpdateRequired = false;
}

void playlistItemContainer::itemAboutToBeDeleted(playlistItem *item)
{
  // Remove the item from childList and disconnect signals/slots.
  // Just delete the pointer. Do not delete the item itself. This is done by
  // the video caching handler.
  for (int i = 0; i < childCount(); i++)
  {
    auto listItem = getChildPlaylistItem(i);
    if (listItem && listItem == item)
    {
      disconnect(
          listItem, &playlistItem::signalItemChanged, this, &playlistItemContainer::childChanged);
      if (listItem->properties().providesStatistics)
        listItem->getStatisticsUIHandler()->deleteSecondaryStatisticsHandlerControls();
      takeChild(i);
    }
  }
}

QList<playlistItem *> playlistItemContainer::getAllChildPlaylistItems() const
{
  QList<playlistItem *> returnList;
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      returnList.append(childItem);
      auto containerItem = dynamic_cast<playlistItemContainer *>(childItem);
      if (containerItem && containerItem->childCount() > 0)
        returnList.append(containerItem->getAllChildPlaylistItems());
    }
  }
  return returnList;
}

QList<playlistItem *> playlistItemContainer::takeAllChildItemsRecursive()
{
  QList<playlistItem *> returnList;
  for (int i = childCount() - 1; i >= 0; i--)
  {
    playlistItem *childItem = getChildPlaylistItem(i);
    if (childItem)
    {
      // First, take all the children of the children
      auto containerItem = dynamic_cast<playlistItemContainer *>(childItem);
      if (containerItem && containerItem->childCount() > 0)
        returnList.append(containerItem->takeAllChildItemsRecursive());
      // Now add the child and take it from this item
      returnList.append(childItem);
    }
    this->takeChild(i);
  }
  return returnList;
}

void playlistItemContainer::childChanged(bool redraw, recacheIndicator recache)
{
  this->prop.startEndRange = indexRange(-1, -1);
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    if (childItem->properties().isIndexedByFrame())
    {
      auto itemRange = childItem->properties().startEndRange;
      if (this->prop.startEndRange == indexRange(-1, -1))
        this->prop.startEndRange = itemRange;

      if (frameLimitsMax)
      {
        // As much as any of the items allows
        this->prop.startEndRange.first = std::min(this->prop.startEndRange.first, itemRange.first);
        this->prop.startEndRange.second =
            std::max(this->prop.startEndRange.second, itemRange.second);
      }
      else
      {
        // Only "overlapping" range
        this->prop.startEndRange.first = std::max(this->prop.startEndRange.first, itemRange.first);
        this->prop.startEndRange.second =
            std::min(this->prop.startEndRange.second, itemRange.second);
      }
    }
  }

  if (redraw || (recache != RECACHE_NONE))
    // A child item changed and it needs redrawing, so we need to re-layout everything and also
    // redraw
    emit signalItemChanged(redraw, recache);
}

bool playlistItemContainer::isSourceChanged()
{
  // Check the children. Always call isSourceChanged() on all children because this function
  // also resets the flag.
  bool changed = false;
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    if (childItem->isSourceChanged())
      changed = true;
  }

  return changed;
}

void playlistItemContainer::reloadItemSource()
{
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    childItem->reloadItemSource();
  }
}

void playlistItemContainer::updateSettings()
{
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    childItem->updateSettings();
  }
}

playlistItem *playlistItemContainer::getChildPlaylistItem(int index) const
{
  if (index < 0 || index > childCount())
    return nullptr;
  return dynamic_cast<playlistItem *>(child(index));
}

void playlistItemContainer::savePlaylistChildren(QDomElement &root, const QDir &playlistDir) const
{
  for (int i = 0; i < childCount(); i++)
  {
    auto childItem = getChildPlaylistItem(i);
    if (childItem)
      childItem->savePlaylist(root, playlistDir);
  }
}
