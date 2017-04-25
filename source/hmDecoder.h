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

#ifndef HMDECODER_H
#define HMDECODER_H

#include "libHMDecoder.h"
#include "fileInfoWidget.h"
#include "fileSourceHEVCAnnexBFile.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"
#include <QLibrary>

using namespace YUV_Internals;

struct hmDecoderFunctions
{
  hmDecoderFunctions();

  libHMDec_context      *(*libHMDec_new_decoder)            (void);
  libHMDec_error         (*libHMDec_free_decoder)           (libHMDec_context*);
  void                   (*libHMDec_set_SEI_Check)          (libHMDec_context*, bool check_hash);
  void                   (*libHMDec_set_max_temporal_layer) (libHMDec_context*, int max_layer);
  libHMDec_error         (*libHMDec_push_nal_unit)          (libHMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

  libHMDec_picture     *(*libHMDec_get_picture)        (libHMDec_context*);
  int                   (*libHMDEC_get_POC)            (libHMDec_picture *pic);
  int                   (*libHMDEC_get_picture_width)  (libHMDec_picture *pic, libHMDec_ColorComponent c);
  int                   (*libHMDEC_get_picture_height) (libHMDec_picture *pic, libHMDec_ColorComponent c);
  int                   (*libHMDEC_get_picture_stride) (libHMDec_picture *pic, libHMDec_ColorComponent c);
  short                *(*libHMDEC_get_image_plane)    (libHMDec_picture *pic, libHMDec_ColorComponent c);
  libHMDec_ChromaFormat (*libHMDEC_get_chroma_format)  (libHMDec_picture *pic);

  int (*libHMDEC_get_internal_bit_depth) (libHMDec_ColorComponent c);
};

// This class wraps the de265 library in a demand-load fashion.
// To easily access the functions, one can use protected inheritance:
// class de265User : ..., protected de265Wrapper
// This API is similar to the QOpenGLFunctions API family.
class hmDecoder : public hmDecoderFunctions 
{
public:
  hmDecoder(int signalID=0, bool cachingDecoder=false);

  // Is retrieving of statistics enabled? It is automatically enabled the first time statistics are requested by loadStatisticsData().
  bool statisticsEnabled() const { return retrieveStatistics; }

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  bool openFile(QString fileName, hmDecoder *otherDecoder = nullptr);

  // Get some infos on the file
  QList<infoItem> getFileInfoList() const { return annexBFile.getFileInfoList(); }
  int getNumberPOCs() const { return annexBFile.getNumberPOCs(); }
  bool isFileChanged() { return annexBFile.isFileChanged(); }
  void updateFileWatchSetting() { annexBFile.updateFileWatchSetting(); }

  // Which signal should we read from the decoder? Reconstruction(0, default), Prediction(1) or Residual(2)
  void setDecodeSignal(int signalID);

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx);

  // Get the statistics values for the given frame (decode if necessary)
  statisticsData getStatisticsData(int frameIdx, int typeIdx);

  // Reload the input file
  bool reloadItemSource();

  yuvPixelFormat getYUVPixelFormat();
  QSize getFrameSize() { return frameSize; }

  // Two types of error can occur. The decoder library (libde265) can not be loaded (errorInDecoder) or 
  // an error when parsing the bitstream within YUView can occur (errorParsingBitstream).
  bool errorInDecoder() const { return decoderError; }
  bool errorParsingBitstream() const { return parsingError; }
  QString decoderErrorString() const { return errorString; }

  // does the loaded library support the extraction of internals/statistics?
  bool wrapperInternalsSupported() const { return internalsSupported; } 
  // does the loaded library support the extraction of prediction/residual data?
  bool wrapperPredResiSupported() const { return predAndResiSignalsSupported; }

  // Get the full path and filename to the decoder library that is being used
  QString getLibraryPath() const { return libraryPath; }

private:
  void loadDecoderLibrary();

  void allocateNewDecoder();

  void setError(const QString &reason);
  QFunctionPointer resolve(const char *symbol);
  template <typename T> T resolve(T &ptr, const char *symbol);
  template <typename T> T resolveInternals(T &ptr, const char *symbol);

  // if set to true the decoder will also get statistics from each decoded frame and put them into the local cache
  bool retrieveStatistics;

  QLibrary library;
  bool decoderError;
  bool parsingError;
  bool internalsSupported;
  bool predAndResiSignalsSupported;
  QString errorString;
  bool isCachingDecoder; //< Is this the caching or the interactive decoder?

  // The current pixel format and size. Whenever a picture is decoded, this is updated.
  libHMDec_ChromaFormat pixelFormat;
  int nrBitsC0;
  QSize frameSize;

  // Reconstruction(0, default), Prediction(1) or Residual(2)
  int decodeSignal;

  libHMDec_context* decoder;

  libHMDec_error decError;
  
  // The Annex B source file
  fileSourceHEVCAnnexBFile annexBFile;
  QByteArray lastNALUnit;
  bool stateReadingFrames;

  // Statistics caching
  void cacheStatistics(const libHMDec_picture *pic);
  QHash<int, statisticsData> curPOCStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurPOC;                    // the POC of the statistics that are in the curPOCStats
  
  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(libHMDec_picture *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyImgToByteArray(libHMDec_picture *src, QByteArray &dst);   // Copy the raw data from the de265_image source *src to the byte array
#endif
  
  // This holds the file path to the loaded library
  QString libraryPath;
};

#endif // HMDECODER_H
