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

#ifndef DE265DECODER_H
#define DE265DECODER_H

#include "de265.h"
#include "de265_internals.h"
#include "fileInfoWidget.h"
#include "fileSourceHEVCAnnexBFile.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"
#include <QLibrary>

using namespace YUV_Internals;

struct de265Functions
{
  de265Functions();

  de265_decoder_context *(*de265_new_decoder)          ();
  void                   (*de265_set_parameter_bool)   (de265_decoder_context*, de265_param, int);
  void                   (*de265_set_parameter_int)    (de265_decoder_context*, de265_param, int);
  void                   (*de265_disable_logging)      ();
  void                   (*de265_set_verbosity)        (int);
  de265_error            (*de265_start_worker_threads) (de265_decoder_context*, int);
  void                   (*de265_set_limit_TID)        (de265_decoder_context*, int);
  const char*            (*de265_get_error_text)       (de265_error);
  de265_chroma           (*de265_get_chroma_format)    (const de265_image*);
  int                    (*de265_get_image_width)      (const de265_image*, int);
  int                    (*de265_get_image_height)     (const de265_image*, int);
  const uint8_t*         (*de265_get_image_plane)      (const de265_image*, int, int*);
  int                    (*de265_get_bits_per_pixel)   (const de265_image*, int);
  de265_error            (*de265_decode)               (de265_decoder_context*, int*);
  de265_error            (*de265_push_data)            (de265_decoder_context*, const void*, int, de265_PTS, void*);
  de265_error            (*de265_flush_data)           (de265_decoder_context*);
  const de265_image*     (*de265_get_next_picture)     (de265_decoder_context*);
  de265_error            (*de265_free_decoder)         (de265_decoder_context*);

  // libde265 decoder library function pointers for internals
  void (*de265_internals_get_CTB_Info_Layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_CTB_sliceIdx)			   (const de265_image*, uint16_t*);
  void (*de265_internals_get_CB_Info_Layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_CB_info)				       (const de265_image*, uint16_t*);
  void (*de265_internals_get_PB_Info_layout)		   (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_PB_info)				       (const de265_image*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*);
  void (*de265_internals_get_IntraDir_Info_layout) (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_intraDir_info)			   (const de265_image*, uint8_t*, uint8_t*);
  void (*de265_internals_get_TUInfo_Info_layout)	 (const de265_image*, int*, int*, int*);
  void (*de265_internals_get_TUInfo_info)			     (const de265_image*, uint8_t*);

  // libde265 decoder library pointers for retrieval of prediction and residual
  const uint8_t* (*de265_internals_get_image_plane)    (const struct de265_image* img, de265_internals_param signal, int channel, int* out_stride);
  void           (*de265_internals_set_parameter_bool) (de265_decoder_context*, enum de265_internals_param param, int value);
};

// This class wraps the de265 library in a demand-load fashion.
// To easily access the functions, one can use protected inheritance:
// class de265User : ..., protected de265Wrapper
// This API is similar to the QOpenGLFunctions API family.
class de265Decoder : public de265Functions 
{
public:
  de265Decoder(int signalID=0);

  // Is retrieving of statistics enabled? It is automatically enabled the first time statistics are requested by loadStatisticsData().
  bool statisticsEnabled() const { return retrieveStatistics; }

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  bool openFile(QString fileName, de265Decoder *otherDecoder = nullptr);

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

  // The current pixel format and size. Whenever a picture is decoded, this is updated.
  de265_chroma pixelFormat;
  int nrBitsC0;
  QSize frameSize;

  // Reconstruction(0, default), Prediction(1) or Residual(2)
  int decodeSignal;

  // Was there an error? If everything is OK it will be DE265_OK.
  de265_error decError;

  de265_decoder_context* decoder;
  
  // The Annex B source file
  fileSourceHEVCAnnexBFile annexBFile;

  // Statistics caching
  void cacheStatistics(const de265_image *img);
  QHash<int, statisticsData> curPOCStats;  // cache of the statistics for the current POC [statsTypeID]
  int statsCacheCurPOC;                    // the POC of the statistics that are in the curPOCStats
  // With the given partitioning mode, the size of the CU and the prediction block index, calculate the
  // sub-position and size of the prediction block
  void getPBSubPosition(int partMode, int CUSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const;
  void cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int log2TUSize, int trDepth);

  // Convert intra direction mode into vector
  static const int vectorTable[35][2];

  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, QByteArray &dst);   // Copy the raw data from the de265_image source *src to the byte array
#endif
  
  // This holds the file path to the loaded library
  QString libraryPath;
};

#endif // DE265DECODER_H
