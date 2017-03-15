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

#ifndef PLAYLISTITEMSTATIC_H
#define PLAYLISTITEMSTATIC_H

#include "playlistItem.h"
#include "typedef.h"
#include <assert.h>

#include "ui_playlistItemStatic.h"

// The default duration in seconds
#define PLAYLISTITEMTEXT_DEFAULT_DURATION 5.0

class playlistItemStatic :
  public playlistItem
{
  Q_OBJECT

public:

  playlistItemStatic(const QString &itemNameOrFileName);

  // This is a static item and it is not indexed by frame.
  virtual bool isIndexedByFrame() const Q_DECL_OVERRIDE Q_DECL_FINAL { return false; }

  // A static item is shown for a certain duration
  virtual double getDuration() const Q_DECL_OVERRIDE Q_DECL_FINAL { return duration; }

public slots:
  void on_durationSpinBox_valueChanged(double val) { duration = val; }

protected:
  
  // Overload from playlistItem. Create a properties widget custom to the playlistItemStatic
  // and set propertiesWidget to point to it. This function will just create the "duration" spin box.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // Add the control for the time that this item is shown to
  QLayout *createStaticTimeController();

  // Load/Save from/to playlist
  void appendPropertiesToPlaylist(QDomElementYUView &d) const;
  static void loadPropertiesFromPlaylist(const QDomElementYUView &root, playlistItemStatic *newItem);

  // The duration that this item is shown for
  double duration;

private:
  SafeUi<Ui::PlaylistItemStatic> ui;

};

#endif // PLAYLISTITEMSTATIC_H
