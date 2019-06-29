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

#include "decoderBase.h"

#include <QDir>
#include <QSettings>

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define DECODERBASE_DEBUG_OUTPUT 0
#if DECODERBASE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERBASE_DEBUG_OUTPUT == 1
#define DEBUG_HEVCDECODERBASE if(!isCachingDecoder) qDebug
#elif DECODERBASE_DEBUG_OUTPUT == 2
#define DEBUG_HEVCDECODERBASE if(isCachingDecoder) qDebug
#elif DECODERBASE_DEBUG_OUTPUT == 3
#define DEBUG_HEVCDECODERBASE if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_DECODERBASE(fmt,...) ((void)0)
#endif

decoderBase::decoderBase(bool cachingDecoder)
{
  DEBUG_DECODERBASE("decoderBase::decoderBase create base%s", cachingDecoder ? " - caching" : "");
  isCachingDecoder = cachingDecoder;

  resetDecoder();
}

void decoderBase::resetDecoder()
{
  DEBUG_DECODERBASE("decoderBase::resetDecoder");
  decoderState = decoderNeedsMoreData;
  statsCacheCurPOC = -1;
  frameSize = QSize();
  formatYUV = yuvPixelFormat();
  rawFormat = raw_Invalid;
}

statisticsData decoderBase::getStatisticsData(int typeIdx)
{
  if (!retrieveStatistics)
    return statisticsData();

  return curPOCStats[typeIdx];
}

void decoderBaseSingleLib::loadDecoderLibrary(QString specificLibrary)
{
  // Try to load the HM library from the current working directory
  // Unfortunately relative paths like this do not work: (at least on windows)
  // library.setFileName(".\\libde265");

  bool libLoaded = false;

  // Try the specific library first
  library.setFileName(specificLibrary);
  libraryPath = specificLibrary;
  libLoaded = library.load();

  if (!libLoaded)
  {
    // Try various paths/names next
    // If the file name is not set explicitly, QLibrary will try to open
    // the decLibXXX.so file first. Since this has been compiled for linux
    // it will fail and not even try to open the decLixXXX.dylib.
    // On windows and linux ommitting the extension works
    QStringList libNames = getLibraryNames();

    // Get the additional search path from the settings
    QSettings settings;
    settings.beginGroup("Decoders");
    QString searchPath = settings.value("SearchPath", "").toString();
    if (!searchPath.endsWith("/"))
      searchPath.append("/");
    searchPath.append("%1");
    settings.endGroup();

    QStringList const libPaths = QStringList()
      << searchPath
      << QDir::currentPath() + "/%1"
      << QDir::currentPath() + "/decoder/%1"
      << QDir::currentPath() + "/libde265/%1"                       // for legacy installations before the decoders were moved to the "decoders" folder
      << QCoreApplication::applicationDirPath() + "/%1"
      << QCoreApplication::applicationDirPath() + "/decoder/%1"
      << QCoreApplication::applicationDirPath() + "/libde265/%1"
      << "%1"; // Try the system directories.

    for (auto &libName : libNames)
    {
      for (auto &libPath : libPaths)
      {
        library.setFileName(libPath.arg(libName));
        libraryPath = libPath.arg(libName);
        libLoaded = library.load();
        if (libLoaded)
          break;
      }
      if (libLoaded)
        break;
    }
  }

  if (!libLoaded)
  {
    libraryPath.clear();
    QString error = "Error loading library: " + library.errorString() + "\n";
    error += "We could not load one of the supported decoder library (";
    QStringList libNames = getLibraryNames();
    for (int i = 0; i < libNames.count(); i++)
    {
      if (i == 0)
        error += libNames[0];
      else
        error += ", " + libNames[i];
    }
    error += ").\n";
    error += "\n";
    error += "We do not ship all of the decoder libraries.\n";
    error += "You can find download links in Help->Downloads.";
    return setError(error);
  }

  resolveLibraryFunctionPointers();
}
