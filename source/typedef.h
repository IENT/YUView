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

#define INT_INVALID -1

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
// An info item is just a pair of Strings
// For example: ["File Name", "file.yuv"] or ["Number Frames", "123"]
typedef QPair<QString, QString> infoItem;
// A index range is just a QPair of ints (minimum and maximum)
typedef QPair<int,int> indexRange;

#endif // TYPEDEF_H

