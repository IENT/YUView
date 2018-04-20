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

#ifndef TYPEDEF_H
#define TYPEDEF_H

#include <cassert>
#include <cstring>
#include <QDomElement>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QRect>
#include <QString>

// Maximum possible value for int
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#define INT_INVALID -1

// Convenience macro definitions which can be used in if clauses:
// if (is_Q_OS_MAC) ...
#ifdef Q_OS_MAC
enum { is_Q_OS_MAC = 1 };
#else
enum { is_Q_OS_MAC = 0 };
#endif

#ifdef Q_OS_WIN
enum { is_Q_OS_WIN = 1 };
#else
enum { is_Q_OS_WIN = 0 };
#endif

#ifdef Q_OS_LINUX
enum { is_Q_OS_LINUX = 1 };
#else
enum { is_Q_OS_LINUX = 0 };
#endif

// Set this to one to enable the code that handles single instances.
// Basically, we use a QLocalServer to try to communicate with already running instances of YUView.
// However, it is not yet clear what to do if the user wants/needs a second instance.
#define WIN_LINUX_SINGLE_INSTANCE 0

// Activate SSE YUV conversion
// Do not activate. This is not supported right now.
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

// A small class comparable to QByteArray but aligned to 16 byte addresses
class byteArrayAligned
{
public:
  byteArrayAligned() : _data(NULL), _size(-1) {}
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

// The default frame rate that will be used when we could not guess it.
#define DEFAULT_FRAMERATE 20.0

// The default duration in seconds of static items (like text, images ...)
#define PLAYLISTITEMTEXT_DEFAULT_DURATION 5.0

// If the zoom factor is >= this value, the raw values will be drawn alongside the pixels.
#define SPLITVIEW_DRAW_VALUES_ZOOMFACTOR 64

// If the zoom factor is >= this value, the statistics values will be drawn alongside the blocks.
#define STATISTICS_DRAW_VALUES_ZOOM 16

// If this macro is set to true, YUView will try to self update if an update is available.
// If it is set to false, we will still check for updates, but the update feature is 
// disabled. Do not set this manually in your own build because the update feature will
// probably not work then. This macro is set to true by our buildbot server. Only builds
// from that server will use the update feature (that way we can ensure that the same build
// environment is used).
#define UPDATE_FEATURE_ENABLE 0

#ifndef YUVIEW_VERSION
#define YUVIEW_VERSION "Unknown"
#endif

#ifndef YUVIEW_HASH
#define VERSION_CHECK 0
#define YUVIEW_HASH 0
#else
#define VERSION_CHECK 1
#endif

#define MAX_RECENT_FILES 10

template <typename T> inline T clip(const T n, const T lower, const T upper) { return (n < lower) ? lower : (n > upper) ? upper : n; }

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
  ValuePairListSets(const QString &title, const ValuePairList &valueList)
  {
    append(title, valueList);
  }
  // Append a pair of QString and ValuePairList
  void append(const QString &title, const ValuePairList &valueList)
  {
    QList::append(QPair<QString, ValuePairList>(title, valueList));
  }
  // Append a list to this list
  void append(const ValuePairListSets &list)
  {
    QList::append(list);
  }
};

Q_DECL_CONSTEXPR inline QPoint centerRoundTL(const QRect & r) Q_DECL_NOTHROW
{
  // The cast avoids overflow on addition.
  return QPoint(int((qint64(r.left())+r.right()-1)/2), int((qint64(r.top())+r.bottom()-1)/2));
}

// Identical to a QDomElement, but we add some convenience functions (findChildValue and appendProperiteChild)
// for putting values into the playlist and reading them from the playlist.
class QDomElementYUView : public QDomElement
{
public:
  // Copy constructor so we can initialize from a QDomElement
  QDomElementYUView(const QDomElement &a) : QDomElement(a) {}
  // Look through all the child items. If one child element exists with the given tagName, return it's text node.
  // All attributes of the child (if found) are appended to attributes.
  QString findChildValue(const QString &tagName) const { ValuePairList b; return findChildValue(tagName, b); }
  QString findChildValue(const QString &tagName, ValuePairList &attributeList) const
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
    return QString();
  }
  // Append a new child to this element with the given type, and name (as text node).
  // All QString pairs in ValuePairList are appended as attributes.
  void appendProperiteChild(const QString &type, const QString &name, const ValuePairList &attributes=ValuePairList())
  {
    QDomElement newChild = ownerDocument().createElement(type);
    newChild.appendChild(ownerDocument().createTextNode(name));
    for (int i = 0; i < attributes.length(); i++)
      newChild.setAttribute(attributes[i].first, attributes[i].second);
    appendChild(newChild);
  }
};

// A index range is just a QPair of integers (minimum and maximum)
typedef QPair<int,int> indexRange;

class QWidget;
class QLayout;
void setupUi(void *ui, void(*setupUi)(void *ui, QWidget *widget));

// A safe wrapper around Ui::Form class, for delayed initialization
// and in support of widget-less setupUi.
// The Ui::Form is zeroed out as a way of catching null pointer dereferences
// before the Ui has been set up.
template <class Ui> class SafeUi : public Ui {
  bool m_created;
  static void setup_ui_helper(void *ui, QWidget *widget)
  {
    reinterpret_cast<SafeUi*>(ui)->setupUi(widget);
  }
public:
  SafeUi() { clear(); }
  void setupUi(QWidget *widget)
  {
    Q_ASSERT(!m_created);
    Ui::setupUi(widget);
    m_created = true;
  }
  void setupUi()
  {
    Q_ASSERT(!m_created);
    ::setupUi(static_cast<void*>(this), &SafeUi::setup_ui_helper);
    this->wrapperLayout = NULL; // The wrapper was deleted, don't leave a dangling pointer.
    m_created = true;
  }
  void clear()
  {
    memset(static_cast<Ui*>(this), 0, sizeof(Ui));
    m_created = false;
  }
  bool created() const { return m_created; }
};

// A label that emits a 'clicked' signal when clicked.
class QLabelClickable : public QLabel
{
  Q_OBJECT

public:
  QLabelClickable(QWidget *parent) : QLabel(parent) { pressed = false; }
  virtual void mousePressEvent(QMouseEvent *event)
  {
    Q_UNUSED(event);
    pressed = true;
  }
  virtual void mouseReleaseEvent(QMouseEvent *event)
  {
    Q_UNUSED(event);
    if (pressed)
      // The mouse was pressed and is now released.
      emit clicked();
    pressed = false;
  }
signals:
  void clicked();
private:
  bool pressed;
};

// An image format used internally by QPixmap. On a raster paint backend, the pixmap
// is backed by an image, and this returns the format of the internal QImage buffer.
// This will always return the same result as the platformImageFormat when the default
// raster backend is used.
// It is faster to call platformImageFormat instead. It will call this function as
// a fall back.
// This function is thread-safe.
QImage::Format pixmapImageFormat();

// Convert the QImage::Format to string
QString pixelFormatToString(QImage::Format f);

// The platform-specific screen-compatible image format. Using a QImage of this format
// is fast when drawing on a widget.
// This function is thread-safe.
inline QImage::Format platformImageFormat()
{
  // see https://code.woboq.org/qt5/qtbase/src/gui/image/qpixmap_raster.cpp.html#97
  // see https://code.woboq.org/data/symbol.html?root=../qt5/&ref=_ZN21QRasterPlatformPixmap18systemOpaqueFormatEv
  if (is_Q_OS_MAC)
    // https://code.woboq.org/qt5/qtbase/src/plugins/platforms/cocoa/qcocoaintegration.mm.html#117
    // https://code.woboq.org/data/symbol.html?root=../qt5/&ref=_ZN12QCocoaScreen14updateGeometryEv
    // Qt Docs: The image is stored using a 32-bit RGB format (0xffRRGGBB).
    return QImage::Format_RGB32;
  if (is_Q_OS_WIN)
    // https://code.woboq.org/qt5/qtbase/src/plugins/platforms/windows/qwindowsscreen.cpp.html#59
    // https://code.woboq.org/data/symbol.html?root=../qt5/&ref=_ZN18QWindowsScreenDataC1Ev
    // Qt Docs:
    // The image is stored using a premultiplied 32-bit ARGB format (0xAARRGGBB), i.e. the red, green, and blue channels 
    // are multiplied by the alpha component divided by 255. (If RR, GG, or BB has a higher value than the alpha channel, 
    // the results are undefined.) Certain operations (such as image composition using alpha blending) are faster using 
    // premultiplied ARGB32 than with plain ARGB32.
    return QImage::Format_ARGB32_Premultiplied;
  // Fall back on Linux and other platforms.
  return pixmapImageFormat();
}

inline int bytesPerPixel(QPixelFormat format)
{
  auto const bits = format.bitsPerPixel();
  return (bits >= 1) ? ((bits + 7) / 8) : 0;
}

inline int bytesPerPixel(QImage::Format format) { return bytesPerPixel(QImage::toPixelFormat(format)); }

// Get the optimal thread count (QThread::optimalThreadCount()-1) or at least 1
// so that one thread is "reserved" for the main GUI. I don't know if this is optimal.
unsigned int getOptimalThreadCount();

// Returns the size of system memory in megabytes.
// This function is thread safe and inexpensive to call.
unsigned int systemMemorySizeInMB();

// When asking the playlist item if it needs loading, there are some states that the item can return
enum itemLoadingState
{
  LoadingNeeded,              ///< The item needs to perform loading before the given frame index can be displayed
  LoadingNotNeeded,           ///< The item does not need loading. The item can be drawn right now.
  LoadingNeededDoubleBuffer   ///< The item does not need loading for the given frame but the double buffer needs an update.
};

enum recacheIndicator
{
  RECACHE_NONE,   // No action from the cache required
  RECACHE_CLEAR,  // Clear all cached images from this item and rethink what to cache next
  RECACHE_UPDATE  // Only rethink what to cache next. Some frames in the item might have become useless in the cache.
};
Q_DECLARE_METATYPE(recacheIndicator)

// ---------- Themes

// These are the names of the supported themes
QStringList getThemeNameList();
// Get the name of the theme in the resource file that we will load
QString getThemeFileName(QString themeName);
// For the given theme, return the primary colors to replace.
// In the qss file, we can use tags, which will be replaced by these colors. The tags are:
// #backgroundColor, #activeColor, #inactiveColor, #highlightColor
// The values to replace them by are returned in this order.
QStringList getThemeColors(QString themeName);
// Return the icon/pixmap from the given file path (inverted if necessary)
QIcon convertIcon(QString iconPath);
QPixmap convertPixmap(QString pixmapPath);

#if QT_VERSION <= 0x050700
// copied from newer version of qglobal.h 
template <typename... Args>
struct QNonConstOverload
{
    template <typename R, typename T>
    Q_DECL_CONSTEXPR auto operator()(R (T::*ptr)(Args...)) const Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
    template <typename R, typename T>
    static Q_DECL_CONSTEXPR auto of(R (T::*ptr)(Args...)) Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
};
template <typename... Args>
struct QConstOverload
{
    template <typename R, typename T>
    Q_DECL_CONSTEXPR auto operator()(R (T::*ptr)(Args...) const) const Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
    template <typename R, typename T>
    static Q_DECL_CONSTEXPR auto of(R (T::*ptr)(Args...) const) Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
};
template <typename... Args>
struct QOverload : QConstOverload<Args...>, QNonConstOverload<Args...>
{
    using QConstOverload<Args...>::of;
    using QConstOverload<Args...>::operator();
    using QNonConstOverload<Args...>::of;
    using QNonConstOverload<Args...>::operator();
    template <typename R>
    Q_DECL_CONSTEXPR auto operator()(R (*ptr)(Args...)) const Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
    template <typename R>
    static Q_DECL_CONSTEXPR auto of(R (*ptr)(Args...)) Q_DECL_NOTHROW -> decltype(ptr)
    { return ptr; }
};
#endif

#endif // TYPEDEF_H
