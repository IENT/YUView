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

#include "hevcDecoderBase.h"

#include <QDir>

// Debug the decoder ( 0:off 1:interactive deocder only 2:caching decoder only 3:both)
#define HEVCDECODERBASE_DEBUG_OUTPUT 0
#if HEVCDECODERBASE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if HEVCDECODERBASE_DEBUG_OUTPUT == 1
#define DEBUG_HEVCDECODERBASE if(!isCachingDecoder) qDebug
#elif HEVCDECODERBASE_DEBUG_OUTPUT == 2
#define DEBUG_HEVCDECODERBASE if(isCachingDecoder) qDebug
#elif HEVCDECODERBASE_DEBUG_OUTPUT == 3
#define DEBUG_HEVCDECODERBASE if (isCachingDecoder) qDebug("c:"); else qDebug("i:"); qDebug
#endif
#else
#define DEBUG_HEVCDECODERBASE(fmt,...) ((void)0)
#endif

// Conversion from intra prediction mode to vector.
// Coordinates are in x,y with the axes going right and down.
#define VECTOR_SCALING 0.25
const int hevcDecoderBase::vectorTable[35][2] = 
{
  {0,0}, {0,0},
  {32, -32},
  {32, -26}, {32, -21}, {32, -17}, { 32, -13}, { 32,  -9}, { 32, -5}, { 32, -2},
  {32,   0},
  {32,   2}, {32,   5}, {32,   9}, { 32,  13}, { 32,  17}, { 32, 21}, { 32, 26},
  {32,  32},
  {26,  32}, {21,  32}, {17,  32}, { 13,  32}, {  9,  32}, {  5, 32}, {  2, 32},
  {0,   32},
  {-2,  32}, {-5,  32}, {-9,  32}, {-13,  32}, {-17,  32}, {-21, 32}, {-26, 32},
  {-32, 32} 
};

hevcDecoderBase::hevcDecoderBase(bool cachingDecoder) :
  decoderError(false),
  parsingError(false),
  internalsSupported(false),
  predAndResiSignalsSupported(false)
{
  retrieveStatistics = false;
  statsCacheCurPOC = -1;
  isCachingDecoder = cachingDecoder;
  decodeSignal = 0;

  // The buffer holding the last requested frame (and its POC). (Empty when constructing this)
  // When using the zoom box the getOneFrame function is called frequently so we
  // keep this buffer to not decode the same frame over and over again.
  currentOutputBufferFrameIndex = -1;
}

bool hevcDecoderBase::openFile(QString fileName, hevcDecoderBase *otherDecoder)
{ 
  // Open the file, decode the first frame and return if this was successfull.
  if (otherDecoder)
    parsingError = !annexBFile.openFile(fileName, false, &otherDecoder->annexBFile);
  else
    parsingError = !annexBFile.openFile(fileName);
  if (!decoderError)
    decoderError &= (!loadYUVFrameData(0).isEmpty());
  return !parsingError && !decoderError;
}

void hevcDecoderBase::setDecodeSignal(int signalID)
{
  if (!predAndResiSignalsSupported)
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

void hevcDecoderBase::setError(const QString &reason)
{
  decoderError = true;
  errorString = reason;
}

void hevcDecoderBase::loadDecoderLibrary()
{
  // Try to load the libde265 library from the current working directory
  // Unfortunately relative paths like this do not work: (at least on windows)
  // library.setFileName(".\\libde265");

  QStringList libNames = getLibraryNames();

  QStringList const libPaths = QStringList()
    << QDir::currentPath() + "/%1"
    << QDir::currentPath() + "/libde265/%1"
    << QCoreApplication::applicationDirPath() + "/%1"
    << QCoreApplication::applicationDirPath() + "/libde265/%1"
    << "%1"; // Try the system directories.

  bool libLoaded = false;
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

  if (!libLoaded)
  {
    libraryPath.clear();
    return setError(library.errorString());
  }

  resolveLibraryFunctionPointers();
}