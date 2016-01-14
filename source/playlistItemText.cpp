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

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout;
  vAllLaout->setContentsMargins( 0, 0, 0, 0 );

  // Create the grid layout that contains duration, font, color
  QGridLayout *topGrid = new QGridLayout;
  vAllLaout->addLayout( topGrid );
  topGrid->setContentsMargins( 0, 0, 0, 0 );

  // Create/add/connect controls
  topGrid->addWidget( new QLabel("Duration (seconds)", propertiesWidget), 0, 0 );
  QDoubleSpinBox *durationSpinBox = new QDoubleSpinBox(propertiesWidget);
  durationSpinBox->setMaximum(100000);
  durationSpinBox->setValue(duration);
  QObject::connect(durationSpinBox, SIGNAL(valueChanged(double)), this, SLOT(slotDurationChanged(double)));
  topGrid->addWidget( durationSpinBox, 0, 1 );

  topGrid->addWidget( new QLabel("Duration", propertiesWidget), 1, 0 );
  QPushButton *selectFontButton = new QPushButton("Select Font", propertiesWidget);
  QObject::connect(selectFontButton, SIGNAL(clicked()), this, SLOT(slotSelectFont()));
  topGrid->addWidget( selectFontButton, 1, 1 );

  topGrid->addWidget( new QLabel("Color", propertiesWidget), 2, 0 );
  QPushButton *selectColorButton = new QPushButton("Select Color", propertiesWidget);
  QObject::connect(selectColorButton, SIGNAL(clicked()), this, SLOT(slotSelectColor()));
  topGrid->addWidget( selectColorButton, 2, 1 );

  // Add a group Box containing a text editor
  QGroupBox *textGroupBox = new QGroupBox("Text");
  QHBoxLayout *textGroupBoxLayout = new QHBoxLayout;
  vAllLaout->addWidget(textGroupBox);
  textGroupBox->setLayout( textGroupBoxLayout );

  textEdit = new QTextEdit(text, propertiesWidget);
  QObject::connect(textEdit, SIGNAL(textChanged()), this, SLOT(slotTextChanged()));
  textGroupBoxLayout->addWidget(textEdit);
        
  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(2, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );

  slotTextChanged();
}

void playlistItemText::slotSelectFont()
{
  bool ok;
  QFont newFont = QFontDialog::getFont(&ok, font, NULL);
  if (ok)
    font = newFont;
}

void playlistItemText::slotSelectColor()
{
  QColor newColor = QColorDialog::getColor(color, NULL, tr("Select font color"), QColorDialog::ShowAlphaChannel);
  color = newColor;
}

void playlistItemText::slotTextChanged()
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
}

void playlistItemText::savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir)
{
  root.appendChild( createTextElement(doc, "key", "Class") );
  root.appendChild( createTextElement(doc, "string", "TextFrameProvider") );
  root.appendChild( createTextElement(doc, "key", "Properties") );

  QDomElement d = doc.createElement("dict");
  
  QString durationString; durationString.setNum(getDuration());
  d.appendChild( createTextElement(doc, "key", "duration") );       // <key>duration</key>
  d.appendChild( createTextElement(doc, "real", durationString) );  // <real>5.2</real>

  d.appendChild( createTextElement(doc, "key", "fontColor") );      // <key>fontColor</key>
  d.appendChild( createTextElement(doc, "string", color.name()) );  // <string>#000000</string>
  
  d.appendChild( createTextElement(doc, "key", "fontName") );       // <key>fontName</key>
  d.appendChild( createTextElement(doc, "string", font.family()) ); // <string>Arial</string>

  d.appendChild( createTextElement(doc, "key", "fontSize") );                            // <key>fontSize</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(font.pointSize())) ); // <integer>48</integer>

  d.appendChild( createTextElement(doc, "key", "text") );   // <key>text</key>
  d.appendChild( createTextElement(doc, "string", text) );  // <string>Text</string>
  
  root.appendChild(d);
}

playlistItemText *playlistItemText::newplaylistItemText(QDomElement stringElement)
{
  // stringElement should be the <string>TextFrameProvider</string> element
  assert(stringElement.text() == "TextFrameProvider");

  QDomElement propertiesKey = stringElement.nextSiblingElement();
  if (propertiesKey.tagName() != QLatin1String("key") || propertiesKey.text() != "Properties")
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "<key>Properties</key> not found in TextFrameProvider entry";
    return NULL;
  }

  QDomElement propertiesDict = propertiesKey.nextSiblingElement();
  if (propertiesDict.tagName() != QLatin1String("dict"))
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "<dict> not found in TextFrameProvider properties entry";
    return NULL;
  }

  // Parse all the properties
  QDomElement it = propertiesDict.firstChildElement();

  double duration;
  QString fontColor, fontName, text;
  int fontSize;
  try
  {
    duration = parseDoubleFromPlaylist(it, "duration");
    fontColor = parseStringFromPlaylist(it, "fontColor");
    fontName = parseStringFromPlaylist(it, "fontName");
    fontSize = parseIntFromPlaylist(it, "fontSize");
    text = parseStringFromPlaylist(it, "text");
  }
  catch (parsingException err)
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << err;
    return NULL;
  }

  playlistItemText *newText = new playlistItemText;
  newText->duration = duration;
  newText->font = QFont(fontName, fontSize);
  newText->color = QColor(fontColor);
  newText->text = text;
  return newText;
}