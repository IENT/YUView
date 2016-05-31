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

playlistItemStatic::playlistItemStatic(QString itemNameOrFileName) :
    playlistItem(itemNameOrFileName)
{
  controlsCreated = false;
  duration = PLAYLISTITEMTEXT_DEFAULT_DURATION;
}

QLayout *playlistItemStatic::createStaticTimeController(QWidget *parentWidget)
{
  // Absolutely always only call this function once!
  assert(!controlsCreated);
    
  ui.setupUi(parentWidget);
  // Set min/max duration
  ui.durationSpinBox->setMaximum(100000);
  ui.durationSpinBox->setValue(duration);

  connect(ui.durationSpinBox, SIGNAL(valueChanged(double)), this, SLOT(on_durationSpinBox_valueChanged(double)));

  controlsCreated = true;
  return ui.horizontalLayout;
}

// For a static item we save the duration to playlist
void playlistItemStatic::appendPropertiesToPlaylist(QDomElementYUView &d)
{
  d.appendProperiteChild( "duration", QString::number(duration) );
}

// Load the duration from playlist
void playlistItemStatic::loadPropertiesFromPlaylist(QDomElementYUView root, playlistItemStatic *newItem)
{
  newItem->duration = root.findChildValue("duration").toDouble();
}

void playlistItemStatic::createPropertiesWidget()
{
  // Absolutely always only call this once// 
  assert (propertiesWidget == NULL);
  
  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemIndexed"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);

  QFrame *line = new QFrame(propertiesWidget);
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (duration) then the text spcific controls (font, text...)
  vAllLaout->addLayout( createStaticTimeController(propertiesWidget) );
  vAllLaout->addWidget( line );
  
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}