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

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QTreeWidgetItem>
#include <QDomElement>
#include <QDir>
#include "typedef.h"
#include <assert.h>

class playlistItem :
  public QObject,
  public QTreeWidgetItem 
{
  Q_OBJECT

public:
  
  /* The default constructor requires the user to set a name that will be displayed in the treeWidget and
   * provide a pointer to the widget stack for the properties panels. The constructor will then call 
   * addPropertiesWidget to add the custom properties panel.
  */
  playlistItem(QString itemNameOrFileName);
  virtual ~playlistItem();

  QString getName() { return text(0); }

  // Save the element to the given xml structure. Has to be overloaded by the child classes which should
  // know how to load/save themselves.
  virtual void savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir) = 0;

  /* Is this item indexed by a frame number or by a duration
   * 
   * A playlist item may be indexed by a frame number, or it may be a static object that is shown 
   * for a set amount of time.
   * TODO: Add more info here or in the class description
  */
  virtual bool isIndexedByFrame() = 0;
 
  // Return the info title and info list to be shown in the fileInfo groupBox.
  // The default implementations will return empty strings/list.
  virtual QString getInfoTitel() { return ""; }
  virtual QList<infoItem> getInfoList() { return QList<infoItem>(); }

  /* Get the title of the properties panel. The child class has to overload this.
   * This can be different depending on the type of playlistItem.
   * For example a playlistItemYUVFile will return "YUV File properties".
  */
  virtual QString getPropertiesTitle() = 0;
  QWidget *getPropertiesWidget() { if (!propertiesWidget) createPropertiesWidget(); return propertiesWidget; }
  bool propertiesWidgetCreated() { return propertiesWidget; }

  // Does the playlist item currently accept drops of the given item?
  virtual bool acceptDrops(playlistItem *draggingItem) { return false; }

  // ----- Video ----

  // Does the playlist item prvode video? If yes the following functions can be used
  // to access it.
  virtual bool  providesVideo() { return false; }

  virtual double getFrameRate() { return 0; }
  virtual QSize  getVideoSize() { return QSize(); }
  virtual int    getNumberFrames() { return -1; }
  
  // If isIndexedByFrame() return false, the item is shown for a certain period of time (duration).
  virtual double getDuration()  { return -1; }

  virtual void drawFrame(int frameIdx, QPainter *painter) {}

  virtual int  getSampling() { return 1; }

  // ------ Statistics ----

  // Does the playlistItem provide statistics? If yes, the following functions can be
  // used to access it
  virtual bool provideStatistics() { return false; }

signals:
  // Emitted if somthing in the playlist item changed and a redraw is necessary
  void signalRedrawItem();
  
protected:
  // The widget which is put into the stack.
  QWidget *propertiesWidget;

  // Create the properties widget and set propertiesWidget to point to it.
  // Overload this function in a child class to create a custom widget. The default
  // implementation here will add an empty widget.
  virtual void createPropertiesWidget( );

  // Return a new element of the form <type>name<\type>.
  QDomElement createTextElement(QDomDocument &doc, QString type, QString name);

  // Parse the values from the playlist (or return "", -1 or -1.0 if it failed)
  static QString parseStringFromPlaylist(QDomElement &e, QString name);
  static int     parseIntFromPlaylist(QDomElement &e, QString name);
  static double  parseDoubleFromPlaylist(QDomElement &e, QString name);
  
  // This exception is thrown if something goes wrong.
  typedef QString parsingException;
  
};

#endif // PLAYLISTITEM_H
