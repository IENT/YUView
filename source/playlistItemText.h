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

#ifndef PLAYLISTITEMTEXT_H
#define PLAYLISTITEMTEXT_H

#include "playlistitem.h"
#include "typedef.h"
#include <QTextEdit>

#include "ui_playlistItemText.h"

// The default Text that is set for the playlistItemText
#define PLAYLISTITEMTEXT_DEFAULT_TEXT "Text"

class playlistItemText :
  public playlistItem
{
  Q_OBJECT

public:
  playlistItemText(QString initialText = PLAYLISTITEMTEXT_DEFAULT_TEXT);
  playlistItemText(playlistItemText *cloneFromTxt);
  ~playlistItemText();

  // ------ Overload from playlistItem

  virtual QString getInfoTitle() Q_DECL_OVERRIDE { return "Text Info"; }

  virtual QString getPropertiesTitle() Q_DECL_OVERRIDE { return "Text Properties"; }

  // Get the text size (using the current text, font/text size ...)
  virtual QSize getSize() const Q_DECL_OVERRIDE;

  // Overload from playlistItem. Save the text item to playlist.
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) Q_DECL_OVERRIDE;
  // Create a new playlistItemText from the playlist file entry. Return NULL if parsing failed.
  static playlistItemText *newplaylistItemText(QDomElementYUView stringElement);

  // Draw the text item. Since isIndexedByFrame() returned false, this item is not indexed by frames
  // and the given value of frameIdx will be ignored.
  virtual void drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool playback) Q_DECL_OVERRIDE;
  
protected:
  // Overload from playlistItem. Create a properties widget custom to the text item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget() Q_DECL_OVERRIDE;

  // Create the text specific controls (font, color, text)
  QLayout *createTextController();

private:

  QColor  color;
  QFont   font;
  QString text;

  SafeUi<Ui::playlistItemText> ui;

private slots:
  // Slots for the controls (automatically connected by the UI)
  void on_selectFontButton_clicked();
  void on_selectColorButton_clicked();
  void on_textEdit_textChanged();

};

#endif // PLAYLISTITEMTEXT_H
