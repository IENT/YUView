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

#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <QPair>
#include <QSize>
#include <QString>
#include <QHash>
#include <QCache>
#include <QList>
#include <QRect>
#include <QDomElement>
#include <assert.h>

#define INT_INVALID -1

// Activate SSE YUV conversion
#define SSE_CONVERSION 0
#if SSE_CONVERSION
#define HAVE_SSE4_1 1

// Alternate method for SSE Conversion, Testing only
#if SSE_CONVERSION
#define SSE_CONVERSION_420_ALT 1
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_SSE4_1
#define MEMORY_PADDING  8
#else
#define MEMORY_PADDING  0
#endif

#define STANDARD_ALIGNMENT 16

#ifdef HAVE___MINGW_ALIGNED_MALLOC
#define ALLOC_ALIGNED(alignment, size)         __mingw_aligned_malloc((size), (alignment))
#define FREE_ALIGNED(mem)                      __mingw_aligned_free((mem))
#elif _WIN32
#define ALLOC_ALIGNED(alignment, size)         _aligned_malloc((size), (alignment))
#define FREE_ALIGNED(mem)                      _aligned_free((mem))
#elif defined(HAVE_POSIX_MEMALIGN)
static inline void *ALLOC_ALIGNED(size_t alignment, size_t size) {
    void *mem = NULL;
    if (posix_memalign(&mem, alignment, size) != 0) {
        return NULL;
    }
    return mem;
};
#define FREE_ALIGNED(mem)                      free((mem))
#else
#define ALLOC_ALIGNED(alignment, size)      memalign((alignment), (size))
#define FREE_ALIGNED(mem)                   free((mem))
#endif

#define ALLOC_ALIGNED_16(size)              ALLOC_ALIGNED(16, size)

// A small class comparable to QByteArray but aligned to 16 byte adresses
class byteArrayAligned
{
public:
  byteArrayAligned() : _data(NULL), _size(-1) {};
  ~byteArrayAligned()
  {
    if (_size != -1)
    {
      assert(_data != NULL);
      FREE_ALIGNED(_data);
    }
  }
  int size() { return _size; }
  int capacity() { return _size; }
  char *data() { return _data; }
  bool isEmpty() { return _size<=0?true:false;}
  void resize(int size)
  {
    if (_size != -1)
    {
      // The array has been allocated before. Free it.
      assert(_data != NULL);
      FREE_ALIGNED(_data);
      _data = NULL;
      _size = -1;
    }
    // Allocate a new array of sufficient size
    assert(_size == -1);
    assert(_data == NULL);
    _data = (char*)ALLOC_ALIGNED_16(size + MEMORY_PADDING);
    _size = size;
  }
private:
  char* _data;
  int   _size;
};
#endif

// The default framerate that will be used when we could not guess it.
#define DEFAULT_FRAMERATE 20.0

#ifndef YUVIEW_HASH
#define VERSION_CHECK 0
#define YUVIEW_HASH 0
#else
#define VERSION_CHECK 1
#endif

#define MAX_SCALE_FACTOR 5
#define MAX_RECENT_FILES 5

template <typename T> inline T clip(const T& n, const T& lower, const T& upper) { return std::max(lower, std::min(n, upper)); }

// A pair of two strings
typedef QPair<QString, QString> ValuePair;
// A list of valuePairs (pairs of two strings)
typedef QList<ValuePair> ValuePairList;
// A list of value pair lists, where every list has a string (title)
class ValuePairListSets : public QList<QPair<QString, ValuePairList> >
{
public:
  // Create an empty list
  ValuePairListSets() {}
  // Create a ValuePairListSets from one list of values with a title.
  ValuePairListSets(QString title, ValuePairList valueList)
  {
    append(title, valueList);
  }
  // Append a pair of QString and ValuePairList
  void append(QString title, ValuePairList valueList)
  {
    QList::append( QPair<QString, ValuePairList>(title, valueList) );
  }
  // Append a list to this list
  void append(ValuePairListSets list)
  {
    QList::append(list);
  }
};

class Rect : public QRect
{
public:
  Rect()
  {
    // Init an empty rect
    setLeft(0);
    setRight(-1);
    setTop(0);
    setBottom(-1);
  }
  Rect(QRect rect) 
  {
    // Just copy the rect
    setLeft(rect.left());
    setRight(rect.right());
    setTop(rect.top());
    setBottom(rect.bottom());
  }
  QPoint centerRoundTL()
  { 
    return QPoint( (left()+right()-1)/2, (top()+bottom()-1)/2 );
  }
};

// Identical to a QDomElement, but we add some convenience functions (findChildValue and appendProperiteChild)
// for putting values into the playlist and reading them from the playlist.
class QDomElementYUV : public QDomElement
{
public:
  // Copy contructor so we can initialize from a QDomElement
  QDomElementYUV(const QDomElement &a) : QDomElement(a) {};
  // Look through all the child items. If one child element exists with the given tagName, return it's text node.
  // All attributes of the child (if found) are appended to attributes.
  QString findChildValue(QString tagName, ValuePairList &attributeList=ValuePairList())
  {
    for (QDomNode n = firstChild(); !n.isNull(); n = n.nextSibling())
      if (n.isElement() && n.toElement().tagName() == tagName)
      {
        QDomNamedNodeMap attributes = n.toElement().attributes();
        for (int i = 0; i < attributes.length(); i++)
        {
          QString name = attributes.item(i).nodeName();
          QString val  = attributes.item(i).nodeValue();
          attributeList.append(ValuePair(name, val));
        }
        return n.toElement().text();
      }
    return "";
  }
  // Append a new child to this element with the given type, and name (as text node).
  // All QString pairs in ValuePairList are appended as attributes.
  void appendProperiteChild(QString type, QString name, ValuePairList attributes=ValuePairList())
  {
    QDomElement newChild = ownerDocument().createElement(type);
    newChild.appendChild( ownerDocument().createTextNode(name) );
    for (int i = 0; i < attributes.length(); i++)
      newChild.setAttribute(attributes[i].first, attributes[i].second);
    appendChild( newChild );
  }
};

// A index range is just a QPair of ints (minimum and maximum)
typedef QPair<int,int> indexRange;



#endif // TYPEDEF_H

