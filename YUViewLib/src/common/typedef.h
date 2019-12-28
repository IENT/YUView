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
#include <QLabel>
#include <QList>
#include <QPair>
#include <QRect>
#include <QString>

namespace YUView
{
  Q_NAMESPACE;

  typedef enum
  {
    raw_Invalid,
    raw_YUV,
    raw_RGB
  } RawFormat;

  typedef enum
  {
    inputInvalid = -1,
    inputAnnexBHEVC,    // Raw HEVC annex B file
    inputAnnexBAVC,     // Raw AVC annex B file
    inputAnnexBVVC,     // Raw VVC annex B file
    inputLibavformat,   // This is some sort of container file which we will read using libavformat
    input_NUM
  } inputFormat;

  typedef enum
  {
    decoderEngineInvalid = -1,  // invalid value
    decoderEngineLibde265,      // The libde265 decoder
    decoderEngineHM,            // The HM reference software decoder
    decoderEngineVTM,           // The VTM reference software decoder
    decoderEngineDav1d,         // The dav1d AV1 decoder
    decoderEngineFFMpeg,        // The FFMpeg decoder
    decoderEngineNum
  } decoderEngine;
}

// Maximum possible value for int
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#define INT_INVALID -1
#ifndef INT_MIN
#define INT_MIN (-2147483647 - 1)
#endif

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
#define SSE_CONVERSION_420_ALT 1  // Alternate method for SSE Conversion, Testing only

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
#endif // SSE_CONVERSION

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

/// ---- Custom types
typedef QPair<uint64_t, uint64_t> QUint64Pair;
typedef QPair<QString, QString> QStringPair;
typedef QList<QStringPair> QStringPairList;
typedef QPair<int,int> indexRange;  // QPair of integers (minimum and maximum)
typedef QPair<int,int> QIntPair;
typedef QPair<unsigned int, unsigned int> QUIntPair;

struct RangeInt
{
  int min;
  int max;
};

// A list of value pair lists, where every list has a string (title)
class ValuePairListSets : public QList<QPair<QString, QStringPairList>>
{
public:
  // Create an empty list
  ValuePairListSets() {}
  // Create a ValuePairListSets from one list of values with a title.
  ValuePairListSets(const QString &title, const QStringPairList &valueList)
  {
    append(title, valueList);
  }
  // Append a pair of QString and ValuePairList
  void append(const QString &title, const QStringPairList &valueList)
  {
    QList::append(QPair<QString, QStringPairList>(title, valueList));
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
  return QPoint(int((int64_t(r.left())+r.right()-1)/2), int((int64_t(r.top())+r.bottom()-1)/2));
}

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
