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
#include <QDebug>
#include <QPushButton>
#include <QColorDialog>
#include <QFontDialog>

playlistItemText::playlistItemText()
  : playlistItem( QString("Text: \"%1\"").arg(PLAYLISTITEMTEXT_DEFAULT_TEXT) )
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_text.png"));
  // Nothing can be dropped onto a text item
  setFlags(flags() & ~Qt::ItemIsDropEnabled);

  color = Qt::black;
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
  durationSpinBox = new QDoubleSpinBox(propertiesWidget);
  durationSpinBox->setMaximum(100000);
  durationSpinBox->setValue(5.0);
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

  textEdit = new QTextEdit(PLAYLISTITEMTEXT_DEFAULT_TEXT, propertiesWidget);
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
  QString text = textEdit->toPlainText();

  // Only show the first 50 characters
  if (text.length() > 50)
  {
    text.truncate(50);
    text.append("...");
  }

  // If there is a newline only show the first line of the text
  int newlinePos = text.indexOf(QRegExp("[\n\t\r]"));
  if (newlinePos != -1)
  {
    text.truncate(newlinePos);
    text.append("...");
  }

  setText(0, QString("Text: \"%1\"").arg(text) );
}