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

#include "playlistItemText.h"
#include "typedef.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>
#include <QFontDialog>
#include <QTime>
#include <QPainter>

#include <QDebug>

playlistItemText::playlistItemText(QString initialText)
  : playlistItemStatic( QString("Text: \"%1\"").arg(initialText) )
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_text.png"));
  // Nothing can be dropped onto a text item
  setFlags(flags() & ~Qt::ItemIsDropEnabled);

  color = Qt::black;
  text = initialText;
}

// The copy contructor. Copy all the setting from the other text item.
playlistItemText::playlistItemText(playlistItemText *cloneFromTxt)
  : playlistItemStatic(cloneFromTxt->plItemNameOrFileName)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_text.png"));
  // Nothing can be dropped onto a text item
  setFlags(flags() & ~Qt::ItemIsDropEnabled);
  
  // Copy playlistItemText properties
  color = cloneFromTxt->color;
  text = cloneFromTxt->text;
  
  // Copy playlistItemStatic
  duration = cloneFromTxt->duration;
}

void playlistItemText::createPropertiesWidget()
{
  // Absolutely always only call this once
  assert(!propertiesWidget);
  
  // Create a new widget and populate it with controls
  preparePropertiesWidget(QStringLiteral("playlistItemText"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame(propertiesWidget.data());
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (duration) then the text spcific controls (font, text...)
  vAllLaout->addLayout( createStaticTimeController() );
  vAllLaout->addWidget( line );
  vAllLaout->addLayout( createTextController() );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);
}

QLayout *playlistItemText::createTextController()
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  ui.setupUi();
  
  // Set the text
  ui.textEdit->setPlainText(text);
    
  on_textEdit_textChanged();

  // Connect signals
  connect(ui.selectFontButton, SIGNAL(clicked()), this, SLOT(on_selectFontButton_clicked()));
  connect(ui.selectColorButton, SIGNAL(clicked()), this, SLOT(on_selectColorButton_clicked()));
  connect(ui.textEdit, SIGNAL(textChanged()), this, SLOT(on_textEdit_textChanged()));

  return ui.topVBoxLayout;
}

void playlistItemText::on_selectFontButton_clicked()
{
  bool ok;
  QFont newFont = QFontDialog::getFont(&ok, font, NULL);
  if (ok)
    font = newFont;

  emit signalItemChanged(true, false);
}

void playlistItemText::on_selectColorButton_clicked()
{
  QColor newColor = QColorDialog::getColor(color, NULL, tr("Select font color"), QColorDialog::ShowAlphaChannel);
  color = newColor;

  emit signalItemChanged(true, false);
}

void playlistItemText::on_textEdit_textChanged()
{
  QString t = ui.textEdit->toPlainText();
  text = t;

  // Only show the first 50 characters
  if (t.length() > 50)
  {
    t.truncate(50);
    t.append("...");
  }

  // If there is a newline only show the first line of the text
  int newlinePos = t.indexOf(QRegExp("[\n\t\r]"));
  if (newlinePos != -1)
  {
    t.truncate(newlinePos);
    t.append("...");
  }

  setName(QString("Text: \"%1\"").arg(t) );

  emit signalItemChanged(true, false);
}

void playlistItemText::savePlaylist(QDomElement &root, QDir playlistDir)
{
  Q_UNUSED(playlistDir);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemText");

  // Append the properties of the playlistItemStatic
  playlistItemStatic::appendPropertiesToPlaylist(d);
  
  // Apppend all the properties of the text item
  d.appendProperiteChild( "color", color.name() );
  d.appendProperiteChild( "fontName", font.family() );
  d.appendProperiteChild( "fontSize", QString::number(font.pointSize()) );
  d.appendProperiteChild( "text", text );
      
  root.appendChild(d);
}

playlistItemText *playlistItemText::newplaylistItemText(QDomElementYUView root)
{
  // Get the text and create a new playlistItemText
  QString text = root.findChildValue("text");
  playlistItemText *newText = new playlistItemText(text);

  // Load the playlistItemStatic properties
  playlistItemStatic::loadPropertiesFromPlaylist(root, newText);
  
  // Get and set all the values from the playlist file
  QString fontName = root.findChildValue("fontName");
  int fontSize = root.findChildValue("fontSize").toInt();
  newText->font = QFont(fontName, fontSize);
  newText->color = QColor( root.findChildValue("color") );
    
  return newText;
}

void playlistItemText::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback)
{
  Q_UNUSED(frameIdx);
  Q_UNUSED(playback);
  // Center the text so that the center is at (0,0).

  // Set font and color. Scale the font size with the zoom factor.
  QFont displayFont = font;
  displayFont.setPointSizeF( font.pointSizeF() * zoomFactor );
  painter->setFont( displayFont );
  painter->setPen( color );
    
  // Get the size of the text and create a rect of that size which is centered at (0,0)
  QSize textSize = getSize() * zoomFactor;
  QRect textRect;
  textRect.setSize( textSize );
  textRect.moveCenter( QPoint(0,0) );

  // Draw the text
  painter->drawText( textRect, text );
}

QSize playlistItemText::getSize()
{
  QFontMetrics metrics(font);
  return metrics.size(0, text);
}
