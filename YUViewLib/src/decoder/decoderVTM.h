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

#ifndef DECODERVTM_H
#define DECODERVTM_H

#include <QLibrary>

#include "decoderBase.h"
#include "common/fileInfo.h"
#include "externalHeader/libVTMDecoder.h"
#include "statistics/statisticsExtensions.h"
#include "video/videoHandlerYUV.h"

struct decoderVTM_Functions
{
  decoderVTM_Functions();

  // General functions
  const char            *(*libVTMDec_get_version)            (void);
  libVTMDec_context     *(*libVTMDec_new_decoder)            (void);
  libVTMDec_error        (*libVTMDec_free_decoder)           (libVTMDec_context*);
  void                   (*libVTMDec_set_SEI_Check)          (libVTMDec_context*, bool check_hash);
  void                   (*libVTMDec_set_max_temporal_layer) (libVTMDec_context*, int max_layer);
  libVTMDec_error        (*libVTMDec_push_nal_unit)          (libVTMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);

  // Get a picture and retrive information on the picture
  libVTMDec_picture     *(*libVTMDec_get_picture)            (libVTMDec_context*);
  int                    (*libVTMDec_get_POC)                (libVTMDec_picture *pic);
  int                    (*libVTMDec_get_picture_width)      (libVTMDec_picture *pic, libVTMDec_ColorComponent c);
  int                    (*libVTMDec_get_picture_height)     (libVTMDec_picture *pic, libVTMDec_ColorComponent c);
  int                    (*libVTMDec_get_picture_stride)     (libVTMDec_picture *pic, libVTMDec_ColorComponent c);
  short                 *(*libVTMDec_get_image_plane)        (libVTMDec_picture *pic, libVTMDec_ColorComponent c);
  libVTMDec_ChromaFormat (*libVTMDec_get_chroma_format)      (libVTMDec_picture *pic);
  int                    (*libVTMDec_get_internal_bit_depth) (libVTMDec_picture *pic, libVTMDec_ColorComponent c);

  // Internals
  /*unsigned int            (*libVTMDec_get_internal_type_number)          ();
  char                   *(*libVTMDec_get_internal_type_name)            (unsigned int idx);*/
};

// This class wraps the HM decoder library in a demand-load fashion.
class decoderVTM : public decoderBaseSingleLib, decoderVTM_Functions
{
public:
  decoderVTM(int signalID, bool cachingDecoder=false);
  ~decoderVTM();

  void resetDecoder() Q_DECL_OVERRIDE;

  // Decoding / pushing data
  bool decodeNextFrame() Q_DECL_OVERRIDE;
  QByteArray getRawFrameData() Q_DECL_OVERRIDE;
  bool pushData(QByteArray &data) Q_DECL_OVERRIDE;

  // Check if the given library file is an existing libde265 decoder that we can use.
  static bool checkLibraryFile(QString libFilePath, QString &error);

  QString getDecoderName() const Q_DECL_OVERRIDE;
  QString getCodecName()         Q_DECL_OVERRIDE { return "hevc"; }

  int nrSignalsSupported() const Q_DECL_OVERRIDE { return nrSignals; }

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as this type of decoder.
  decoderVTM() {};

  // Return the possible names of the HM library
  QStringList getLibraryNames() Q_DECL_OVERRIDE;

  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() Q_DECL_OVERRIDE;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol, bool optional=false);

  void allocateNewDecoder();

  libVTMDec_context* decoder {nullptr};

  // Try to get the next picture from the decoder and save it in currentHMPic
  bool getNextFrameFromDecoder();
  libVTMDec_picture *currentVTMPic {nullptr};

  // When pushing data, we will try to retrive a frame to check if this is possible.
  // If this is true, a frame is waiting from that step and decodeNextFrame will not actually retrive a new frame.
  bool decodedFrameWaiting {false};

  // Statistics caching
  void cacheStatistics(libVTMDec_picture *pic);

  bool internalsSupported {false};
  int nrSignals { 0 };

  // Convert from libde265 types to YUView types
  YUVSubsamplingType convertFromInternalSubsampling(libVTMDec_ChromaFormat fmt);

  // Add the statistics supported by the HM decoder
  void fillStatisticList(statisticHandler &statSource) const Q_DECL_OVERRIDE;

  // We buffer the current image as a QByteArray so you can call getYUVFrameData as often as necessary
  // without invoking the copy operation from the hm image buffer to the QByteArray again.
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(libVTMDec_picture *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyImgToByteArray(libVTMDec_picture *src, QByteArray &dst);   // Copy the raw data from the de265_image source *src to the byte array
#endif  
};

#endif // DECODERVTM_H
