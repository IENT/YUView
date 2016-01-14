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
#include <QSpinBox>
#include <QTextEdit>

// The default Text that is set for the playlistItemText
#define PLAYLISTITEMTEXT_DEFAULT_TEXT "Text"

class playlistItemText :
  public playlistItem
{
  Q_OBJECT

public:
  playlistItemText();
  ~playlistItemText();

  // This item is not indexed by a frame number. It is a static text that is shown
  // for a fixed amount of time.
  bool isIndexedByFrame() { return false; }

  // ------ Overload from playlistItem

  virtual QString getInfoTitel() { return "Text Info"; }

  virtual QString getPropertiesTitle() { return "Text Properties"; }
  
  // A text item can provide a "video" but no statistics
  virtual bool providesVideo() { return true; }

protected:
  // Overload from playlistItem. Create a properties widget custom to the text item
  // and set propertiesWidget to point to it.
  virtual void createPropertiesWidget();

private:
  
  QDoubleSpinBox *durationSpinBox;
  QTextEdit      *textEdit;

  QColor color;
  QFont  font;

private slots:
  void slotSelectFont();
  void slotSelectColor();
  void slotTextChanged();
  
};

#endif // PLAYLISTITEMTEXT_H