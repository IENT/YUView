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

#include "playlistitemIndexed.h"

playlistItemIndexed::playlistItemIndexed(QString itemNameOrFileName) :
  playlistItem(itemNameOrFileName)
{
  frameRate = DEFAULT_FRAMERATE;
  sampling  = 1;
  startEndFrame = indexRange(-1,-1);
  startEndFrameChanged = false;
}

QLayout *playlistItemIndexed::createIndexControllers()
{
  // Absolutely always only call this function once!
  assert(!ui.created());
    
  ui.setupUi();
    
  indexRange startEndFrameLimit = getstartEndFrameLimits();
  if (startEndFrame == indexRange(-1,-1))
  {
    startEndFrame = startEndFrameLimit;
  }

  // Set default values
  ui.startSpinBox->setMinimum( startEndFrameLimit.first );
  ui.startSpinBox->setMaximum( startEndFrameLimit.second );
  ui.startSpinBox->setValue( startEndFrame.first );
  ui.endSpinBox->setMinimum( startEndFrameLimit.first );
  ui.endSpinBox->setMaximum( startEndFrameLimit.second );
  ui.endSpinBox->setValue( startEndFrame.second );
  ui.rateSpinBox->setMaximum(1000);
  ui.rateSpinBox->setValue( frameRate );
  ui.samplingSpinBox->setMinimum(1);
  ui.samplingSpinBox->setMaximum(100000);
  ui.samplingSpinBox->setValue( sampling );
    
  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(ui.endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(ui.rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(slotVideoControlChanged()));
  connect(ui.samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));

  return ui.gridLayout;
}

void playlistItemIndexed::slotVideoControlChanged()
{
  // Was this the start or end spin box?
  QObject *sender = QObject::sender();
  if (sender == ui.startSpinBox || sender == ui.endSpinBox)
    // The user changed the start end frame
    startEndFrameChanged = true;

  // Get the currently set values from the controls
  startEndFrame.first  = ui.startSpinBox->value();
  startEndFrame.second = ui.endSpinBox->value();
  frameRate = ui.rateSpinBox->value();
  sampling  = ui.samplingSpinBox->value();

  // The current frame in the buffer is not invalid, but emit that something has changed.
  emit signalItemChanged(false, false);
}

void playlistItemIndexed::setStartEndFrame(indexRange range, bool emitSignal)
{
  // Set the new start/end frame (clip it first)
  indexRange startEndFrameLimit = getstartEndFrameLimits();
  startEndFrame.first = std::max(startEndFrameLimit.first, range.first);
  startEndFrame.second = std::min(startEndFrameLimit.second, range.second);

  if (!ui.created())
    // spin boxes not created yet
    return;

  if (!emitSignal)
  {
    QObject::disconnect(ui.startSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(ui.endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  }

  ui.startSpinBox->setMinimum( startEndFrameLimit.first );
  ui.startSpinBox->setMaximum( startEndFrameLimit.second );
  ui.startSpinBox->setValue( startEndFrame.first );
  ui.endSpinBox->setMinimum( startEndFrameLimit.first );
  ui.endSpinBox->setMaximum( startEndFrameLimit.second );
  ui.endSpinBox->setValue( startEndFrame.second );

  if (!emitSignal)
  {
    connect(ui.startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    connect(ui.endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  }
}

// For an indexed item we save the start/end, sampling and frame rate to the playlist
void playlistItemIndexed::appendPropertiesToPlaylist(QDomElementYUView &d)
{
  // Append the playlist item properties
  playlistItem::appendPropertiesToPlaylist(d);

  d.appendProperiteChild( "startFrame", QString::number(startEndFrame.first) );
  d.appendProperiteChild( "endFrame", QString::number(startEndFrame.second) );
  d.appendProperiteChild( "sampling", QString::number(sampling) );
  d.appendProperiteChild( "frameRate", QString::number(frameRate) );
}

// Load the start/end frame, sampling and frame rate from playlist
void playlistItemIndexed::loadPropertiesFromPlaylist(QDomElementYUView root, playlistItemIndexed *newItem)
{
  int startFrame = root.findChildValue("startFrame").toInt();
  int endFrame = root.findChildValue("endFrame").toInt();
  newItem->startEndFrame = indexRange(startFrame, endFrame);
  newItem->sampling = root.findChildValue("sampling").toInt();
  newItem->frameRate = root.findChildValue("frameRate").toInt();

  playlistItem::loadPropertiesFromPlaylist(root, newItem);
}

void playlistItemIndexed::slotUpdateFrameLimits()
{
  // update the spin boxes
  if (!startEndFrameChanged)
  {
    // The user did not change the start/end frame yet. If the new limits increase, we also move the startEndFrame range
    indexRange startEndFrameLimit = getstartEndFrameLimits();
    setStartEndFrame(startEndFrameLimit, false);
  }
  else
  {
    // The user did change the start/end frame. If the limits increase, keep the old range.
    setStartEndFrame(startEndFrame, false);
  }

  emit signalItemChanged(false, false);
}
