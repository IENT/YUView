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

#include "playlistItemText.h"

#include <QColorDialog>
#include <QFontDialog>
#include <QPainter>
#include <QRegularExpression>

#include "common/functions.h"

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define PLAYLISTITEMTEXT_DEBUG 0
#if PLAYLISTITEMTEXT_DEBUG && !NDEBUG
#define DEBUG_TEXT qDebug
#else
#define DEBUG_TEXT(fmt,...) ((void)0)
#endif

playlistItemText::playlistItemText(const QString &initialText)
  : playlistItem(QString("Text: \"%1\"").arg(initialText), Type::Static)
{
  setIcon(0, functions::convertIcon(":img_text.png"));
  setFlags(flags() & ~Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Text Properties";

  this->text = initialText;
}

// The copy constructor. Copy all the setting from the other text item.
playlistItemText::playlistItemText(playlistItemText *cloneFromTxt)
  : playlistItem(cloneFromTxt->properties().name, Type::Static)
{
  this->setIcon(0, QIcon(":img_text.png"));
  this->setFlags(flags() & ~Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Text Properties";
  
  this->color = cloneFromTxt->color;
  this->text = cloneFromTxt->text;
  this->font = cloneFromTxt->font;

  this->prop.duration = cloneFromTxt->properties().duration;
}

void playlistItemText::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");
  
  this->preparePropertiesWidget(QStringLiteral("playlistItemText"));

  QVBoxLayout *vAllLaout = new QVBoxLayout(this->propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (duration) then the text specific controls (font, text...)
  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(createTextController());
  vAllLaout->insertStretch(3, 1); // Push controls up
}

QLayout *playlistItemText::createTextController()
{
  Q_ASSERT_X(!this->ui.created(), "createTextController", "UI already exists");

  this->ui.setupUi();
  
  // Set the text
  this->ui.textEdit->setPlainText(text);
  
  QSignalBlocker blocker(this);
  this->on_textEdit_textChanged();

  // Connect signals
  connect(this->ui.selectFontButton, &QPushButton::clicked, this,  &playlistItemText::on_selectFontButton_clicked);
  connect(this->ui.selectColorButton, &QPushButton::clicked, this, &playlistItemText::on_selectColorButton_clicked);
  connect(this->ui.textEdit, &QPlainTextEdit::textChanged, this, &playlistItemText::on_textEdit_textChanged);

  return this->ui.topVBoxLayout;
}

void playlistItemText::on_selectFontButton_clicked()
{
  bool ok;
  auto newFont = QFontDialog::getFont(&ok, font, nullptr);
  if (ok)
  {
    this->font = newFont;
    emit signalItemChanged(true, RECACHE_NONE);
  }
}

void playlistItemText::on_selectColorButton_clicked()
{
  auto newColor = QColorDialog::getColor(color, nullptr, tr("Select font color"), QColorDialog::ShowAlphaChannel);
  if (newColor != color)
  {
    this->color = newColor;
    emit signalItemChanged(true, RECACHE_NONE);
  }
}

void playlistItemText::on_textEdit_textChanged()
{
  auto t = ui.textEdit->toPlainText();
  this->text = t;

  // Only show the first 50 characters in the name
  if (t.length() > 50)
  {
    t.truncate(50);
    t.append("...");
  }

  // If there is a newline only show the first line of the text
  int newlinePos = t.indexOf(QRegularExpression("[\n\t\r]"));
  if (newlinePos != -1)
  {
    t.truncate(newlinePos);
    t.append("...");
  }

  this->setName(QString("Text: \"%1\"").arg(t));
  DEBUG_TEXT("playlistItemText::on_textEdit_textChanged New test length %d", text.length());

  emit signalItemChanged(true, RECACHE_NONE);
}

void playlistItemText::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  Q_UNUSED(playlistDir);

  auto d = YUViewDomElement(root.ownerDocument().createElement("playlistItemText"));

  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the text item
  d.appendProperiteChild("color", this->color.name());
  d.appendProperiteChild("fontName", this->font.family());
  d.appendProperiteChild("fontSize", QString::number(this->font.pointSize()));
  d.appendProperiteChild("text", this->text);
      
  root.appendChild(d);
}

playlistItemText *playlistItemText::newplaylistItemText(const YUViewDomElement &root)
{
  // Get the text and create a new playlistItemText
  auto text = root.findChildValue("text");
  auto newText = new playlistItemText(text);

  playlistItem::loadPropertiesFromPlaylist(root, newText);
  
  // Get and set all the values from the playlist file
  auto fontName = root.findChildValue("fontName");
  int fontSize = root.findChildValue("fontSize").toInt();
  newText->font = QFont(fontName, fontSize);
  newText->color = QColor(root.findChildValue("color"));
    
  return newText;
}

void playlistItemText::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  Q_UNUSED(frameIdx);
  Q_UNUSED(drawRawData);
  
  QTextDocument td;
  td.setHtml(this->text);

  auto displayFont = this->font;
  displayFont.setPointSizeF(this->font.pointSizeF() * zoomFactor);
  td.setDefaultFont(displayFont);

  QAbstractTextDocumentLayout::PaintContext ctx;
  ctx.palette.setColor(QPalette::Text, color);

  QRect textRect;
  textRect.setSize(td.size().toSize());
  textRect.moveCenter(QPoint(0,0));

  painter->translate(textRect.topLeft());
  td.documentLayout()->draw(painter, ctx);
  painter->translate(textRect.topLeft() * -1);
}

QSize playlistItemText::getSize() const
{
  QTextDocument td;
  td.setDefaultFont(this->font);
  td.setHtml(this->text);
  return td.size().toSize();
}
