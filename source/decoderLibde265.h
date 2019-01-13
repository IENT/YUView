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

#ifndef DECODERLIBDE265_H
#define DECODERLIBDE265_H

#include "de265.h"
#include "de265_internals.h"
#include "decoderBase.h"
#include "videoHandlerYUV.h"
#include <QLibrary>

struct decoderLibde265_Functions
{
  decoderLibde265_Functions();

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
  de265_error            (*de265_push_NAL)             (de265_decoder_context*, const void*, int, de265_PTS, void*);
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

// This class wraps the libde265 library in a demand-load fashion.
class decoderLibde265 : public decoderBaseSingleLib, public decoderLibde265_Functions 
{
public:
  decoderLibde265(int signalID, bool cachingDecoder=false);
  ~decoderLibde265();

  void resetDecoder() Q_DECL_OVERRIDE;

  int nrSignalsSupported() const Q_DECL_OVERRIDE { return nrSignals; }
  bool isSignalDifference(int signalID) const Q_DECL_OVERRIDE { return signalID == 2 || signalID == 3; }
  QStringList getSignalNames() const Q_DECL_OVERRIDE { return QStringList() << "Reconstruction" << "Prediction" << "Residual" << "Transform Coefficients"; }
  void setDecodeSignal(int signalID, bool &decoderResetNeeded) Q_DECL_OVERRIDE;

  // Decoding / pushing data
  bool decodeNextFrame() Q_DECL_OVERRIDE;
  QByteArray getRawFrameData() Q_DECL_OVERRIDE;
  bool pushData(QByteArray &data) Q_DECL_OVERRIDE;
  
  // Statistics
  void fillStatisticList(statisticHandler &statSource) const Q_DECL_OVERRIDE;

  // Check if the given library file is an existing libde265 decoder that we can use.
  static bool checkLibraryFile(QString libFilePath, QString &error);

  QString getDecoderName() const Q_DECL_OVERRIDE { return "libDe265"; }
  QString getCodecName()         Q_DECL_OVERRIDE { return "hevc"; }

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as a hevcDecoderLibde265.
  decoderLibde265() : decoderBaseSingleLib() {};
  
  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() Q_DECL_OVERRIDE;

  // Return the possible names of the libde265 library
  QStringList getLibraryNames() Q_DECL_OVERRIDE;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol, bool optional=false);

  void allocateNewDecoder();

  de265_decoder_context* decoder {nullptr};

  int nrSignals {1};
  bool flushing {false};

  // When pushing frames, the decoder will try to decode a frame to check if this is possible.
  // If this is true, a frame is waiting from that step and decodeNextFrame will not actually decode a new frame.
  bool decodedFrameWaiting {false};

  // Try to decode a frame. If successfull, the frame will be pointed to by curImage.
  bool decodeFrame();
  const de265_image* curImage {nullptr};

  // Convert from libde265 types to YUView types
  YUVSubsamplingType convertFromInternalSubsampling(de265_chroma fmt);
  
  // Statistics caching
  void cacheStatistics(const de265_image *img);
  
  // With the given partitioning mode, the size of the CU and the prediction block index, calculate the
  // sub-position and size of the prediction block
  void getPBSubPosition(int partMode, int CUSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH) const;
  void cacheStatistics_TUTree_recursive(uint8_t *const tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth, bool isIntra, uint8_t *const intraDirY, uint8_t *const intraDirC, int intraDir_infoUnit_size, int widthInIntraDirUnits);

  // We buffer the current image as a QByteArray so you can call getYUVFrameData as often as necessary
  // without invoking the copy operation from the libde265 buffer to the QByteArray again.
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyImgToByteArray(const de265_image *src, QByteArray &dst);   // Copy the raw data from the de265_image source *src to the byte array
#endif
};

#endif // DECODERLIBDE265_H
