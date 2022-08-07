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

#pragma once

#include <common/Typedef.h>

#include "playlistItem.h"
#include "ui_playlistItemText.h"

// The default Text that is set for the playlistItemText
#define PLAYLISTITEMTEXT_DEFAULT_TEXT "Text"

class playlistItemText : public playlistItem
{
  Q_OBJECT

public:
  playlistItemText(const QString &initialText = PLAYLISTITEMTEXT_DEFAULT_TEXT);
  playlistItemText(playlistItemText *cloneFromTxt);
  // ------ Overload from playlistItem

  virtual InfoData getInfo() const override { return InfoData("Text Info"); }

  // Get the text size (using the current text, font/text size ...)
  virtual Size getSize() const override;

  // Overload from playlistItem. Save the text item to playlist.
  virtual void savePlaylist(QDomElement &root, const QDir &playlistDir) const override;
  // Create a new playlistItemText from the playlist file entry. Return nullptr if parsing failed.
  static playlistItemText *newplaylistItemText(const YUViewDomElement &stringElement);

  virtual void
  drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) override;

protected:
  // Overload from playlistItem. Create a properties widget custom to the text item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() override;

  // Create the text specific controls (font, color, text)
  QLayout *createTextController();

private:
  QColor  color{};
  QFont   font;
  QString text{};

  SafeUi<Ui::playlistItemText> ui;

private slots:
  void on_selectFontButton_clicked();
  void on_selectColorButton_clicked();
  void on_textEdit_textChanged();
};
