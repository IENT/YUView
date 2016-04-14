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

playlistItemText::playlistItemText()
  : playlistItem( QString("Text: \"%1\"").arg(PLAYLISTITEMTEXT_DEFAULT_TEXT) )
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_text.png"));
  // Nothing can be dropped onto a text item
  setFlags(flags() & ~Qt::ItemIsDropEnabled);

  color = Qt::black;
  text = PLAYLISTITEMTEXT_DEFAULT_TEXT;
  duration = PLAYLISTITEMTEXT_DEFAULT_DURATION;
}

playlistItemText::~playlistItemText()
{
}

void playlistItemText::createPropertiesWidget()
{
  // Absolutely always only call this once// 
  assert (propertiesWidget == NULL);
  
  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;
  setupUi( propertiesWidget );
  propertiesWidget->setLayout( topVBoxLayout );

  // Set min/max duration
  durationSpinBox->setMaximum(100000);
  durationSpinBox->setValue(duration);

  // Set the text
  textEdit->setPlainText(text);
    
  on_textEdit_textChanged();

  // Connect events
  connect(selectFontButton, SIGNAL(clicked()), this, SLOT(on_selectFontButton_clicked()));
  connect(selectColorButton, SIGNAL(clicked()), this, SLOT(on_selectColorButton_clicked()));
  connect(durationSpinBox, SIGNAL(valueChanged(double)), this, SLOT(on_durationSpinBox_valueChanged(double)));
  connect(textEdit, SIGNAL(textChanged()), this, SLOT(on_textEdit_textChanged()));
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
  QString t = textEdit->toPlainText();
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

  setText(0, QString("Text: \"%1\"").arg(t) );

  emit signalItemChanged(true, false);
}

void playlistItemText::savePlaylist(QDomElement &root, QDir playlistDir)
{
  QDomElementYUV d = root.ownerDocument().createElement("playlistItemText");
  
  // Apppend all the properties of the text item
  d.appendProperiteChild( "duration", QString::number(duration) );
  d.appendProperiteChild( "color", color.name() );
  d.appendProperiteChild( "fontName", font.family() );
  d.appendProperiteChild( "fontSize", QString::number(font.pointSize()) );
  d.appendProperiteChild( "text", text );
      
  root.appendChild(d);
}

playlistItemText *playlistItemText::newplaylistItemText(QDomElementYUV root)
{
  playlistItemText *newText = new playlistItemText;
  
  // Get and set all the values from the playlist file
  newText->duration = root.findChildValue("duration").toDouble();
  QString fontName = root.findChildValue("fontName");
  int fontSize = root.findChildValue("fontSize").toInt();
  newText->font = QFont(fontName, fontSize);
  newText->color = QColor( root.findChildValue("color") );
  newText->text = root.findChildValue("text");
  
  return newText;
}

void playlistItemText::drawItem(QPainter *painter, int frameIdx, double zoomFactor)
{
  Q_UNUSED(frameIdx);
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