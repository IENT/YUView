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
#define DEBUG_HEVCDECODERBASE(fmt,...) ((void)0)
#endif

decoderBase::decoderBase(bool cachingDecoder) :
  decoderError(false),
  parsingError(false),
  internalsSupported(false),
  nrSignalsSupported(1)
{
  retrieveStatistics = false;
  statsCacheCurPOC = -1;
  isCachingDecoder = cachingDecoder;
  decodeSignal = 0;

  nrBitsC0 = -1;
  pixelFormat = YUV_NUM_SUBSAMPLINGS;

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;
}

void decoderBase::setDecodeSignal(int signalID)
{
  if (nrSignalsSupported == 1)
    return;

  DEBUG_HEVCDECODERBASE("hmDecoder::setDecodeSignal old %d new %d", decodeSignal, signalID);

  if (signalID != decodeSignal)
  {
    // A different signal was selected
    decodeSignal = signalID;

    // We will have to decode the current frame again to get the internals/statistics
    // This can be done like this:
    currentOutputBufferFrameIndex = -1;
    // Now the next call to loadYUVFrameData will load the frame again...
  }
}

void decoderBase::setError(const QString &reason)
{
  decoderError = true;
  errorString = reason;
}

void decoderBase::loadDecoderLibrary(QString specificLibrary)
{
  // Try to load the libde265 library from the current working directory
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
      << QDir::currentPath() + "/libde265/%1"
      << QCoreApplication::applicationDirPath() + "/%1"
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
    return setError(library.errorString());
  }

  resolveLibraryFunctionPointers();
}

yuvPixelFormat decoderBase::getYUVPixelFormat()
{
  if (pixelFormat >= YUV_444 && pixelFormat <= YUV_400 && nrBitsC0 >= 8 && nrBitsC0 <= 16)
    return yuvPixelFormat(pixelFormat, nrBitsC0);
  return yuvPixelFormat();
}