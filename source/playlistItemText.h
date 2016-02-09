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

#include "playlistItem.h"
#include "typedef.h"
#include <QTextEdit>

#include "ui_playlistItemText.h"

// The default Text that is set for the playlistItemText
#define PLAYLISTITEMTEXT_DEFAULT_TEXT "Text"
// The default duration in seconds
#define PLAYLISTITEMTEXT_DEFAULT_DURATION 5.0

class playlistItemText :
  public playlistItem, private Ui_playlistItemText
{
  Q_OBJECT

public:
  playlistItemText();
  ~playlistItemText();

  // Overload from playlistItem. Save the text item to playlist.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir);

  // This item is not indexed by a frame number. It is a static text that is shown
  // for a fixed amount of time.
  bool isIndexedByFrame() { return false; }

  // ------ Overload from playlistItem

  virtual QString getInfoTitel() { return "Text Info"; }

  virtual QString getPropertiesTitle() { return "Text Properties"; }
  
  // A text item can provide a "video" but no statistics
  virtual bool providesVideo() { return true; }

  // A text item is only shown for a certain time in seconds. The item does not change over time
  // and there is no concept of "frames" or "frame indices".
  virtual double getDuration() { return duration; }

  // Create a new playlistItemText from the playlist file entry. Return NULL if parsing failed.
  static playlistItemText *newplaylistItemText(QDomElement stringElement);

  // Draw the text item. Since isIndexedByFrame() returned false, this item is not indexed by frames
  // and the given value of frameIdx will be ignored.
  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor);

protected:
  // Overload from playlistItem. Create a properties widget custom to the text item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget();

private:
  
  QColor  color;
  QFont   font;
  double  duration;
  QString text;

private slots:
  // Slots for the controls (automatically connected by the UI)
  void on_durationSpinBox_valueChanged(double val) { duration = val; }
  void on_selectFontButton_clicked();
  void on_selectColorButton_clicked();
  void on_textEdit_textChanged();
  
};

#endif // PLAYLISTITEMTEXT_H
