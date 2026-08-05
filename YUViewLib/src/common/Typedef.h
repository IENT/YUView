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

#include <QList>
#include <QObject>
#include <QPair>
#include <QRect>
#include <QString>
#include <QStringList>
#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
const bool is_Q_OS_MAC = true;
#else
const bool is_Q_OS_MAC = false;
#endif

#ifdef Q_OS_WIN
const bool is_Q_OS_WIN = true;
#else
const bool is_Q_OS_WIN = false;
#endif

#ifdef Q_OS_LINUX
const bool is_Q_OS_LINUX = true;
#else
const bool is_Q_OS_LINUX = false;
#endif

// Set this to one to enable the code that handles single instances.
// Basically, we use a QLocalServer to try to communicate with already running instances of YUView.
// However, it is not yet clear what to do if the user wants/needs a second instance.
#define WIN_LINUX_SINGLE_INSTANCE 0

// The default frame rate that will be used when we could not guess it.
#define DEFAULT_FRAMERATE 24.0

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

/// ---- Custom types
typedef std::pair<uint64_t, uint64_t>       pairUint64;
typedef std::pair<int64_t, int64_t>         pairInt64;
typedef std::pair<int, int>                 IntPair;
typedef std::pair<unsigned, unsigned>       UIntPair;
typedef std::pair<std::string, std::string> StringPair;
typedef std::vector<StringPair>             StringPairVec;
typedef std::vector<std::string>            StringVec;

/// ---- Legacy types that will be replaced
typedef QPair<QString, QString>           QStringPair;
typedef QList<QStringPair>                QStringPairList;
typedef QPair<int, int>                   indexRange; // QPair of integers (minimum and maximum)
typedef QPair<unsigned int, unsigned int> QUIntPair;

template <typename T> using umap_1d = std::map<unsigned, T>;
template <typename T> using umap_2d = std::map<unsigned, umap_1d<T>>;
template <typename T> using umap_3d = std::map<unsigned, umap_2d<T>>;
template <typename T> using umap_4d = std::map<unsigned, umap_3d<T>>;
template <typename T> using umap_5d = std::map<unsigned, umap_4d<T>>;

template <typename T> using vector   = std::vector<T>;
template <typename T> using vector2d = std::vector<vector<T>>;
template <typename T> using vector3d = std::vector<vector2d<T>>;
template <typename T> using vector4d = std::vector<vector3d<T>>;

template <typename T, size_t N> using array               = std::array<T, N>;
template <typename T, size_t N1, size_t N2> using array2d = std::array<std::array<T, N2>, N1>;
template <typename T, size_t N1, size_t N2, size_t N3>
using array3d = std::array<std::array<std::array<T, N3>, N2>, N1>;

template <typename T>
std::optional<std::size_t> vectorIndexOf(const std::vector<T> &vec, const T &item)
{
  auto it = std::find(vec.begin(), vec.end(), item);
  if (it == vec.end())
    return {};
  return static_cast<std::size_t>(std::distance(vec.begin(), it));
}

template <typename T> int vectorContains(const std::vector<T> &vec, const T &item)
{
  auto it = std::find(vec.begin(), vec.end(), item);
  return it != vec.end();
}

typedef std::vector<unsigned char> ByteVector;

template <typename T> struct Range
{
  T min{};
  T max{};

  bool operator!=(const Range &other) const
  {
    return this->min != other.min || this->max != other.max;
  }
};

struct Ratio
{
  int num{};
  int den{};
};

struct Size
{
  constexpr Size(unsigned width, unsigned height) : width(width), height(height) {}
  constexpr Size() = default;

  constexpr bool operator==(const Size &other) const
  {
    return this->width == other.width && this->height == other.height;
  }
  constexpr bool operator!=(const Size &other) const
  {
    return this->width != other.width || this->height != other.height;
  }
  explicit       operator bool() const { return this->isValid(); }
  constexpr bool isValid() const { return this->width > 0 && this->height > 0; }
  unsigned       width{};
  unsigned       height{};
};

struct Offset
{
  Offset(int x, int y) : x(x), y(y) {}
  Offset() = default;
  bool operator==(const Offset &other) const { return this->x == other.x && this->x == other.x; }
  bool operator!=(const Offset &other) const { return this->x != other.x || this->y != other.y; }
  int  x{};
  int  y{};
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
  void append(const ValuePairListSets &list) { QList::append(list); }
};

Q_DECL_CONSTEXPR inline QPoint centerRoundTL(const QRect &r) Q_DECL_NOTHROW
{
  // The cast avoids overflow on addition.
  return QPoint(int((int64_t(r.left()) + r.right() - 1) / 2),
                int((int64_t(r.top()) + r.bottom() - 1) / 2));
}

// When asking the playlist item if it needs loading, there are some states that the item can return
enum class ItemLoadingState
{
  LoadingNeeded,    ///< The item needs to perform loading before the given frame index can be
                    ///< displayed
  LoadingNotNeeded, ///< The item does not need loading. The item can be drawn right now.
  LoadingNeededDoubleBuffer ///< The item does not need loading for the given frame but the double
                            ///< buffer needs an update.
};

enum recacheIndicator
{
  RECACHE_NONE,  // No action from the cache required
  RECACHE_CLEAR, // Clear all cached images from this item and rethink what to cache next
  RECACHE_UPDATE // Only rethink what to cache next. Some frames in the item might have become
                 // useless in the cache.
};
Q_DECLARE_METATYPE(recacheIndicator)

#if QT_VERSION <= 0x050700
// copied from newer version of qglobal.h
template <typename... Args> struct QNonConstOverload
{
  template <typename R, typename T>
  Q_DECL_CONSTEXPR auto operator()(R (T::*ptr)(Args...)) const Q_DECL_NOTHROW->decltype(ptr)
  {
    return ptr;
  }
  template <typename R, typename T>
  static Q_DECL_CONSTEXPR auto of(R (T::*ptr)(Args...)) Q_DECL_NOTHROW -> decltype(ptr)
  {
    return ptr;
  }
};
template <typename... Args> struct QConstOverload
{
  template <typename R, typename T>
  Q_DECL_CONSTEXPR auto operator()(R (T::*ptr)(Args...) const) const Q_DECL_NOTHROW->decltype(ptr)
  {
    return ptr;
  }
  template <typename R, typename T>
  static Q_DECL_CONSTEXPR auto of(R (T::*ptr)(Args...) const) Q_DECL_NOTHROW -> decltype(ptr)
  {
    return ptr;
  }
};
template <typename... Args> struct QOverload : QConstOverload<Args...>, QNonConstOverload<Args...>
{
  using QConstOverload<Args...>::of;
  using QConstOverload<Args...>::operator();
  using QNonConstOverload<Args...>::of;
  using QNonConstOverload<Args...>::operator();
  template <typename R>
  Q_DECL_CONSTEXPR auto operator()(R (*ptr)(Args...)) const Q_DECL_NOTHROW->decltype(ptr)
  {
    return ptr;
  }
  template <typename R>
  static Q_DECL_CONSTEXPR auto of(R (*ptr)(Args...)) Q_DECL_NOTHROW -> decltype(ptr)
  {
    return ptr;
  }
};
#endif
