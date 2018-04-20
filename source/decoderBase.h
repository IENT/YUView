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

#ifndef DECODERBASE_H
#define DECODERBASE_H

#include <QLibrary>
#include "fileSourceAnnexBFile.h"
#include "statisticHandler.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

/* This class is the abstract base class for all non FFMpeg decoders that read from a raw source file.
*/
class decoderBase
{
public:
  decoderBase(bool cachingDecoder=false);
  virtual ~decoderBase() {};

  // Is retrieving of statistics enabled? It is automatically enabled the first time statistics are requested by loadStatisticsData().
  bool statisticsEnabled() const { return retrieveStatistics; }

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  // If another decoder is given, don't parse the annex B bitstream again.
  virtual bool openFile(QString fileName, decoderBase *otherDecoder = nullptr) = 0;

  // Get some infos on the file
  QList<infoItem> getFileInfoList() const { return annexBFile->getFileInfoList(); }
  int getNumberPOCs() const { return annexBFile->getNumberPOCs(); }
  bool isFileChanged() { return annexBFile->isFileChanged(); }
  void updateFileWatchSetting() { annexBFile->updateFileWatchSetting(); }

  // Which signal should we read from the decoder? Reconstruction(0, default), Prediction(1) or Residual(2)
  void setDecodeSignal(int signalID);

  // Load the raw YUV data for the given frame
  virtual QByteArray loadYUVFrameData(int frameIdx) = 0;

  // Get the statistics values for the given frame (decode if necessary)
  virtual statisticsData getStatisticsData(int frameIdx, int typeIdx) = 0;

  // Reload the input file
  virtual bool reloadItemSource() = 0;

  virtual yuvPixelFormat getYUVPixelFormat();
  QSize getFrameSize() { return frameSize; }

  // Two types of error can occur. The decoder library can not be loaded (errorInDecoder) or 
  // an error when parsing the bitstream within YUView can occur (errorParsingBitstream).
  bool errorInDecoder() const { return decoderError; }
  bool errorParsingBitstream() const { return parsingError; }
  QString decoderErrorString() const { return errorString; }

  // does the loaded library support the extraction of internals/statistics?
  bool wrapperInternalsSupported() const { return internalsSupported; } 
  // does the loaded library support the extraction of prediction/residual data?
  int wrapperNrSignalsSupported() const { return nrSignalsSupported; }
  virtual QStringList wrapperGetSignalNames() const { return QStringList() << "Reconstruction"; }

  // Get the full path and filename to the decoder library that is being used
  QString getLibraryPath() const { return libraryPath; }

  // Add the statistics that this deocder can retrieve
  virtual void fillStatisticList(statisticHandler &statSource) const = 0;

  // Get the deocder name (everyting that is needed to identify the deocder library)
  // If needed, also version information (like HM 16.4)
  virtual QString getDecoderName() const = 0;

  // Get a pointer to the fileSource
  fileSourceAnnexBFile *getFileSource() { return annexBFile.data(); }

protected:
  void loadDecoderLibrary(QString specificLibrary);

  virtual QStringList getLibraryNames() = 0;
  // Try to resolve all the required function pointers from the library
  virtual void resolveLibraryFunctionPointers() = 0;

  void setError(const QString &reason);

  // if set to true the decoder will also get statistics from each decoded frame and put them into the local cache
  bool retrieveStatistics;

  QLibrary library;
  bool decoderError;
  bool parsingError;
  bool internalsSupported;
  int nrSignalsSupported;
  QString errorString;
  bool isCachingDecoder; //< Is this the caching or the interactive decoder?

  int nrBitsC0;
  QSize frameSize;
  YUVSubsamplingType pixelFormat;

  // Reconstruction(0, default), Prediction(1) or Residual(2)
  int decodeSignal;

  // Statistics caching
  QHash<int, statisticsData> curPOCStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurPOC;                    // the POC of the statistics that are in the curPOCStats

  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;

  // This holds the file path to the loaded library
  QString libraryPath;

  // A pointer to the source file. This is either a fileSourceAnnexBFile or in case of an HEVC bitstream, 
  // a fileSourceHEVCAnnexBFile
  QScopedPointer<fileSourceAnnexBFile> annexBFile;
};

#endif // DECODERBASE_H