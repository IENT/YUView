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

#include "playlistitemStatic.h"

playlistItemStatic::playlistItemStatic(const QString &itemNameOrFileName) :
    playlistItem(itemNameOrFileName)
{
  duration = PLAYLISTITEMTEXT_DEFAULT_DURATION;
}

QLayout *playlistItemStatic::createStaticTimeController()
{
  // Absolutely always only call this function once!
  assert(!ui.created());
    
  ui.setupUi();
  // Set min/max duration
  ui.durationSpinBox->setMaximum(100000);
  ui.durationSpinBox->setValue(duration);

  connect(ui.durationSpinBox, SIGNAL(valueChanged(double)), this, SLOT(on_durationSpinBox_valueChanged(double)));

  return ui.horizontalLayout;
}

// For a static item we save the duration to playlist
void playlistItemStatic::appendPropertiesToPlaylist(QDomElementYUView &d)
{
  // Append the playlist item properties
  playlistItem::appendPropertiesToPlaylist(d);

  d.appendProperiteChild( "duration", QString::number(duration) );
}

// Load the duration from playlist
void playlistItemStatic::loadPropertiesFromPlaylist(const QDomElementYUView &root, playlistItemStatic *newItem)
{
  newItem->duration = root.findChildValue("duration").toDouble();
  playlistItem::loadPropertiesFromPlaylist(root, newItem);
}

void playlistItemStatic::createPropertiesWidget()
{
  // Absolutely always only call this once
  assert(!propertiesWidget);
  
  // Create a new widget and populate it with controls
  preparePropertiesWidget(QStringLiteral("playlistItemIndexed"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame(propertiesWidget.data());
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (duration) then the text spcific controls (font, text...)
  vAllLaout->addLayout( createStaticTimeController() );
  vAllLaout->addWidget( line );
  
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);
}