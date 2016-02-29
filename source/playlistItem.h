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
  virtual void savePlaylist(QDomElement &root, QDir playlistDir) = 0;

  /* Is this item indexed by a frame number or by a duration
   *
   * A playlist item may be indexed by a frame number, or it may be a static object that is shown
   * for a set amount of time.
   * TODO: Add more info here or in the class description
  */
  virtual bool isIndexedByFrame() = 0;
  virtual indexRange getFrameIndexRange() { return indexRange(0,0); }

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
  virtual bool acceptDrops(playlistItem *draggingItem) { Q_UNUSED(draggingItem); return false; }

  // ----- Video ----

  // Does the playlist item prvode video? If yes the following functions can be used
  // to access it.
  virtual bool  providesVideo() { return false; }

  virtual double getFrameRate() { return 0; }
  virtual QSize  getVideoSize() { return QSize(); }

  // If isIndexedByFrame() return false, the item is shown for a certain period of time (duration).
  virtual double getDuration()  { return -1; }

  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor) { Q_UNUSED(painter); Q_UNUSED(frameIdx); Q_UNUSED(zoomFactor); }

  virtual int  getSampling() { return 1; }

  // Return the source values under the given pixel position.
  // For example a YUV source will provide Y,U and V values. An RGB source might provide RGB values,
  // A difference item will return values from both items and the differences.
  virtual ValuePairList getPixelValues(QPoint pixelPos) { Q_UNUSED(pixelPos); return ValuePairList(); }

  virtual bool isCaching() { return false; }

  // ------ Statistics ----

  // Does the playlistItem provide statistics? If yes, the following functions can be
  // used to access it
  virtual bool provideStatistics() { return false; }

signals:
  // Something in the item changed. If redraw is set, a redraw of the item is necessary.
  void signalItemChanged(bool redraw);

 public slots:
  virtual void startCaching(indexRange range) = 0;
  virtual void stopCaching() = 0;
  virtual void removeFromCache(indexRange range) = 0;


protected:
  // The widget which is put into the stack.
  QWidget *propertiesWidget;

  // Create the properties widget and set propertiesWidget to point to it.
  // Overload this function in a child class to create a custom widget. The default
  // implementation here will add an empty widget.
  virtual void createPropertiesWidget( );

  // Identical to a QDomElement, but we add some convenience functions (findChildValue and appendProperiteChild)
  // for putting values into the playlist and reading them from the playlist.
  class QDomElementYUV : public QDomElement
  {
  public:
    // Copy contructor so we can initialize from a QDomElement
    QDomElementYUV(QDomElement &a) : QDomElement(a) {};
    // Look through all the child items. If one child element exists with the given tagName, return it's text node.
    QString findChildValue(QString tagName)
    {
      for (QDomNode n = firstChild(); !n.isNull(); n = n.nextSibling())
        if (n.isElement() && n.toElement().tagName() == tagName)
          return n.toElement().text();
      return "";
    }
    // Append a new child to this element with the given type, and name (as text node).
    void appendProperiteChild(QString type, QString name)
    {
      QDomElement newChild = ownerDocument().createElement(type);
      newChild.appendChild( ownerDocument().createTextNode(name) );
      appendChild( newChild );
    }
  };

};

#endif // PLAYLISTITEM_H
