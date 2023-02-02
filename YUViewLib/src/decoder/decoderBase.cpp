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

using namespace std::string_literals;

namespace decoder
{

namespace
{

std::string getLibSearchPathFromSettings()
{
  QSettings settings;
  settings.beginGroup("Decoders");
  auto searchPath = settings.value("SearchPath", "").toString().toStdString();
  if (searchPath.back() != '/')
    searchPath += "/";
  settings.endGroup();
  return searchPath;
}

StringVec getPathsToTry(const std::string &libName)
{
  const auto currentPath    = std::filesystem::absolute(std::filesystem::current_path()).string();
  const auto currentAppPath = QCoreApplication::applicationDirPath().toStdString();

  return {getLibSearchPathFromSettings() + libName,
          currentPath + "/" + libName,
          currentPath + "/decoder/" + libName,
          currentPath + "/libde265/" + libName, // for legacy installations before the decoders were
                                                // moved to the "decoders" folder
          currentAppPath + "/" + libName,
          currentAppPath + "/decoder/" + libName,
          currentAppPath + "/libde265/" + libName,
          libName}; // Try the system directories.
}

} // namespace

// Debug the decoder ( 0:off 1:interactive decoder only 2:caching decoder only 3:both)
#define DECODERBASE_DEBUG_OUTPUT 0
#if DECODERBASE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#if DECODERBASE_DEBUG_OUTPUT == 1
#define DEBUG_HEVCDECODERBASE                                                                      \
  if (!isCachingDecoder)                                                                           \
  qDebug
#elif DECODERBASE_DEBUG_OUTPUT == 2
#define DEBUG_HEVCDECODERBASE                                                                      \
  if (isCachingDecoder)                                                                            \
  qDebug
#elif DECODERBASE_DEBUG_OUTPUT == 3
#define DEBUG_HEVCDECODERBASE                                                                      \
  if (isCachingDecoder)                                                                            \
    qDebug("c:");                                                                                  \
  else                                                                                             \
    qDebug("i:");                                                                                  \
  qDebug
#endif
#else
#define DEBUG_DECODERBASE(fmt, ...) ((void)0)
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
  this->decoderState = DecoderState::NeedsMoreData;
}

bool decoderBase::isSignalDifference(int signalID) const
{
  Q_UNUSED(signalID);
  return false;
}

void decoderBase::setDecodeSignal(int signalID, bool &decoderResetNeeded)
{
  if (signalID >= 0 && signalID < nrSignalsSupported())
    this->decodeSignal = signalID;
  decoderResetNeeded = false;
}

stats::FrameTypeData decoderBase::getCurrentFrameStatsForType(int typeId) const
{
  if (!this->statisticsEnabled())
    return {};

  return statisticsData->getFrameTypeData(typeId);
}

void decoderBase::setError(const std::string &reason)
{
  this->decoderState = DecoderState::Error;
  this->errorString  = reason;
}

bool decoderBase::setErrorB(const std::string &reason)
{
  this->setError(reason);
  return false;
}

void decoderBaseSingleLib::loadDecoderLibrary(const std::string &specificLibrary)
{
  // Try to load the HM library from the current working directory
  // Unfortunately relative paths like this do not work: (at least on windows)
  // library.setFileName(".\\libde265");

  // Try the specific library first
  this->library.setFileName(QString::fromStdString(specificLibrary));
  this->libraryPath = specificLibrary;
  auto libLoaded    = this->library.load();

  if (!libLoaded)
  {
    // Try various paths/names next
    // If the file name is not set explicitly, QLibrary will try to open
    // the decLibXXX.so file first. Since this has been compiled for linux
    // it will fail and not even try to open the decLixXXX.dylib.
    // On windows and linux ommitting the extension works

    for (const auto &libName : this->getLibraryNames())
    {
      for (auto &libPath : getPathsToTry(libName))
      {
        this->library.setFileName(QString::fromStdString(libPath));
        this->libraryPath = libPath;
        libLoaded         = library.load();
        if (libLoaded)
          break;
      }
      if (libLoaded)
        break;
    }
  }

  if (!libLoaded)
  {
    this->libraryPath.clear();
    auto error = "Error loading library: "s + this->library.errorString().toStdString() + "\n";
    error += "We could not load one of the supported decoder library (";
    auto libNames = this->getLibraryNames();
    error += to_string(libNames);
    error += "\n";
    error += "We do not ship all of the decoder libraries.\n";
    error += "You can find download links in Help->Downloads.";
    this->setError(error);
    return;
  }

  this->resolveLibraryFunctionPointers();
}

} // namespace decoder
