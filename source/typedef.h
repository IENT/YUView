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
#include <assert.h>

#define INT_INVALID -1

// Activate SSE YUV conversion 
#define SSE_CONVERSION 1
#if SSE_CONVERSION
#define HAVE_SSE4_1 1

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

// A small class comparable to QByteArray but aligned to 16 bit adresses
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
  char *data() { return _data; }
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
// This class has an additional (and optional title string)
class ValuePairList : public QList<ValuePair>
{
public:
  ValuePairList(QString t="Values")
  {
    title = t;
  }
  QString title;
};

// An info item is just a pair of Strings
// For example: ["File Name", "file.yuv"] or ["Number Frames", "123"]
typedef QPair<QString, QString> infoItem;
// A index range is just a QPair of ints (minimum and maximum)
typedef QPair<int,int> indexRange;

class CacheIdx
 {
 public:
     CacheIdx(const QString &name, const unsigned int idx) { fileName=name; frameIdx=idx; }
     QString fileName;
     unsigned int frameIdx;
 };

 inline bool operator==(const CacheIdx &e1, const CacheIdx &e2)
 {
     return e1.fileName == e2.fileName && e1.frameIdx == e2.frameIdx;
 }

 inline uint qHash(const CacheIdx &cIdx)
 {
     uint tmp = qHash(cIdx.fileName) ^ qHash(cIdx.frameIdx);
     return tmp;
 }

#endif // TYPEDEF_H

