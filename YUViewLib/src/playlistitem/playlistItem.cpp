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

#include "playlistItem.h"
#include <QPainter>

unsigned int playlistItem::idCounter = 0;

playlistItem::playlistItem(const QString &itemNameOrFileName, Type type)
{
  this->setName(itemNameOrFileName);
  this->setType(type);
  
  // Whenever a playlistItem is created, we give it an ID (which is unique for this instance of YUView)
  this->prop.id = idCounter++;
}

playlistItem::~playlistItem()
{
}

void playlistItem::setName(const QString &name)
{ 
  this->prop.name = name;
  // For the text that is shown in the playlist, remove all newline characters.
  setText(0, name.simplified());
}

void playlistItem::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues)
{
  Q_UNUSED(frameIdx);
  Q_UNUSED(drawRawValues);

  // Draw an error text in the view instead of showing an empty image
  // Get the size of the text and create a QRect of that size which is centered at (0,0)
  QFont displayFont = painter->font();
  displayFont.setPointSizeF(painter->font().pointSizeF() * zoomFactor);
  painter->setFont(displayFont);
  QSize textSize = painter->fontMetrics().size(0, infoText);
  QRect textRect;
  textRect.setSize(textSize);
  textRect.moveCenter(QPoint(0,0));

  // Draw the text
  painter->drawText(textRect, infoText);
}

QSize playlistItem::getSize() const
{ 
  // Return the size of the text that is drawn on screen.
  QPainter painter;
  QFont displayFont = painter.font();
  return painter.fontMetrics().size(0, infoText);
}

void playlistItem::setType(Type newType)
{
  if (ui.created())
  {
    // Show/hide the right controls
    auto showIndexed = (newType == Type::Indexed);
    ui.labelRate->setVisible(showIndexed);
    ui.rateSpinBox->setVisible(showIndexed);

    auto showStatic  = (newType == Type::Static);
    ui.durationLabel->setVisible(showStatic);
    ui.durationSpinBox->setVisible(showStatic);
  }

  this->prop.type = newType;
}

// For an indexed item we save the start/end, sampling and frame rate to the playlist
void playlistItem::appendPropertiesToPlaylist(YUViewDomElement &d) const
{
  // Append the playlist item properties
  d.appendProperiteChild("id", QString::number(this->prop.id));

  if (this->properties().type == Type::Indexed)
    d.appendProperiteChild("frameRate", QString::number(this->properties().frameRate));
  else
    d.appendProperiteChild("duration", QString::number(this->properties().duration));

  d.appendProperiteChild("viewCenterOffsetView0X", QString::number(savedCenterOffset[0].x()));
  d.appendProperiteChild("viewCenterOffsetView0Y", QString::number(savedCenterOffset[0].y()));
  d.appendProperiteChild("viewZoomFactorView0", QString::number(savedZoom[0]));
  d.appendProperiteChild("viewCenterOffsetView1X", QString::number(savedCenterOffset[1].x()));
  d.appendProperiteChild("viewCenterOffsetView1Y", QString::number(savedCenterOffset[1].y()));
  d.appendProperiteChild("viewZoomFactorView1", QString::number(savedZoom[1]));
}

// Load the start/end frame, sampling and frame rate from playlist
void playlistItem::loadPropertiesFromPlaylist(const YUViewDomElement &root, playlistItem *newItem)
{
  newItem->prop.playlistID = root.findChildValue("id").toInt();

  if (newItem->properties().type == Type::Indexed)
    newItem->prop.frameRate = root.findChildValue("frameRate").toInt();
  else
    newItem->prop.duration = root.findChildValue("duration").toDouble();

  newItem->savedCenterOffset[0].setX(root.findChildValueInt("viewCenterOffsetView0X", 0));
  newItem->savedCenterOffset[0].setY(root.findChildValueInt("viewCenterOffsetView0Y", 0));
  newItem->savedZoom[0] = root.findChildValueDouble("viewZoomFactorView0", 1.0);
  newItem->savedCenterOffset[1].setX(root.findChildValueInt("viewCenterOffsetView1X", 0));
  newItem->savedCenterOffset[1].setY(root.findChildValueInt("viewCenterOffsetView1Y", 0));
  newItem->savedZoom[1] = root.findChildValueDouble("viewZoomFactorView1", 1.0);
}

void playlistItem::slotVideoControlChanged()
{
  if (this->properties().type == Type::Static)
  {
    this->prop.duration = ui.durationSpinBox->value();
  }
  else
  {
    this->prop.frameRate = ui.rateSpinBox->value();
    // The current frame in the buffer is not invalid, but emit that something has changed.
    // Also no frame in the cache is invalid.
    emit signalItemChanged(false, RECACHE_NONE);
  }
}

void playlistItem::slotUpdateFrameLimits()
{
  // The current frame in the buffer is not invalid, but emit that something has changed.
  // Also no frame in the cache is invalid.
  emit signalItemChanged(false, RECACHE_NONE);
}

QLayout *playlistItem::createPlaylistItemControls()
{
  Q_ASSERT_X(!this->ui.created(), "createTextController", "UI already exists");

  ui.setupUi();

  // Set min/max duration for a playlistItem_Static
  ui.durationSpinBox->setMaximum(100000);
  ui.durationSpinBox->setValue(this->properties().duration);

  // Set default values for a playlistItem_Indexed
  ui.rateSpinBox->setMaximum(1000);
  ui.rateSpinBox->setValue(this->properties().frameRate);

  this->setType(this->properties().type);

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.rateSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &playlistItem::slotVideoControlChanged);
  connect(ui.durationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &playlistItem::slotVideoControlChanged);

  return ui.gridLayout;
}

void playlistItem::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  preparePropertiesWidget(QStringLiteral("playlistItem"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  // First add the parents controls (duration) then the text specific controls (font, text...)
  vAllLaout->addLayout(createPlaylistItemControls());

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);
}

void playlistItem::preparePropertiesWidget(const QString &name)
{
  assert(!propertiesWidget);

  propertiesWidget.reset(new QWidget);
  propertiesWidget->setObjectName(name);
}
