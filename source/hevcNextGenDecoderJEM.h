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

#ifndef HEVCNEXTGENDECODERJEM_H
#define HEVCNEXTGENDECODERJEM_H

#include "decoderBase.h"
#include "fileInfoWidget.h"
#include "fileSourceJEMAnnexBFile.h"
#include "libJEMDecoder.h"
#include "statisticsExtensions.h"
#include "videoHandlerYUV.h"
#include <QLibrary>

using namespace YUV_Internals;

struct hevcNextGenDecoderJEM_Functions
{
  hevcNextGenDecoderJEM_Functions();

  // General functions
  const char            *(*libJEMDec_get_version)            (void);
  libJEMDec_context     *(*libJEMDec_new_decoder)            (void);
  libJEMDec_error        (*libJEMDec_free_decoder)           (libJEMDec_context*);
  void                   (*libJEMDec_set_SEI_Check)          (libJEMDec_context*, bool check_hash);
  void                   (*libJEMDec_set_max_temporal_layer) (libJEMDec_context*, int max_layer);
  libJEMDec_error        (*libJEMDec_push_nal_unit)          (libJEMDec_context *decCtx, const void* data8, int length, bool eof, bool &bNewPicture, bool &checkOutputPictures);
  libJEMDec_error        (*libJEMDec_get_nal_unit_info)      (libJEMDec_context *decCtx, const void* data8, int length, bool eof, int &poc, bool &isRAP, bool &isParameterSet, int &picWidthLumaSamples, int &picHeightLumaSamples, int &bitDepthLuma, int &bitDepthChroma, libJEMDec_ChromaFormat &chromaFormat);

  // Get a picture and retrive information on the picture
  libJEMDec_picture     *(*libJEMDec_get_picture)            (libJEMDec_context*);
  int                    (*libJEMDEC_get_POC)                (libJEMDec_picture *pic);
  int                    (*libJEMDEC_get_picture_width)      (libJEMDec_picture *pic, libJEMDec_ColorComponent c);
  int                    (*libJEMDEC_get_picture_height)     (libJEMDec_picture *pic, libJEMDec_ColorComponent c);
  int                    (*libJEMDEC_get_picture_stride)     (libJEMDec_picture *pic, libJEMDec_ColorComponent c);
  short                 *(*libJEMDEC_get_image_plane)        (libJEMDec_picture *pic, libJEMDec_ColorComponent c);
  libJEMDec_ChromaFormat (*libJEMDEC_get_chroma_format)      (libJEMDec_picture *pic);
  int                    (*libJEMDEC_get_internal_bit_depth) (libJEMDec_picture *pic, libJEMDec_ColorComponent c);

  // Internals
  unsigned int            (*libJEMDEC_get_internal_type_number)          ();
  char                   *(*libJEMDEC_get_internal_type_name)            (unsigned int idx);
  libJEMDec_InternalsType (*libJEMDEC_get_internal_type)                 (unsigned int idx);
  unsigned int            (*libJEMDEC_get_internal_type_max)             (unsigned int idx);
  unsigned int            (*libJEMDEC_get_internal_type_vector_scaling)  (unsigned int idx);
  char                   *(*libJEMDEC_get_internal_type_description)     (unsigned int idx);
  libJEMDec_BlockValue   *(*libJEMDEC_get_internal_info)                 (libJEMDec_context*, libJEMDec_picture *pic, unsigned int typeIdx, unsigned int &nrValues, bool &callAgain);
  libJEMDec_error         (*libJEMDEC_clear_internal_info)               (libJEMDec_context *decCtx);
};

// This class wraps the de265 library in a demand-load fashion.
// To easily access the functions, one can use protected inheritance:
// class de265User : ..., protected de265Wrapper
// This API is similar to the QOpenGLFunctions API family.
class hevcNextGenDecoderJEM : public QObject, public hevcNextGenDecoderJEM_Functions, public decoderBase
{
  Q_OBJECT

public:
  hevcNextGenDecoderJEM(int signalID, bool cachingDecoder=false);
  ~hevcNextGenDecoderJEM();

  // Open the given file. Parse the NAL units list and get the size and YUV pixel format from the file.
  // Return false if an error occured (opening the decoder or parsing the bitstream)
  // If another decoder is given, don't parse the annex B bitstream again.
  bool openFile(QString fileName, decoderBase *otherDecoder = nullptr) Q_DECL_OVERRIDE;

  // Load the raw YUV data for the given frame
  QByteArray loadYUVFrameData(int frameIdx) Q_DECL_OVERRIDE;

  // Get the statistics values for the given frame (decode if necessary)
  statisticsData getStatisticsData(int frameIdx, int typeIdx) Q_DECL_OVERRIDE;

  // Reload the input file
  bool reloadItemSource() Q_DECL_OVERRIDE;

  // Add the statistics supported by the HM decoder
  void fillStatisticList(statisticHandler &statSource) const Q_DECL_OVERRIDE;

  QString getDecoderName() const Q_DECL_OVERRIDE;
  
  // Check if the given library file is an existing HM decoder that we can use.
  static bool checkLibraryFile(QString libFilePath, QString &error);

private slots:
  // Ask the decoder library about the raw NAL unit and pass the info the the annexBFile
  void slotGetNALUnitInfo(QByteArray nalBytes);

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as a hevcNextGenDecoderJEM.
  hevcNextGenDecoderJEM();

  QStringList getLibraryNames() Q_DECL_OVERRIDE;

  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() Q_DECL_OVERRIDE;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol);
  template <typename T> T resolveInternals(T &ptr, const char *symbol);
  
  void allocateNewDecoder();
  void freeDecoder();
   
  libJEMDec_context* decoder;

  libJEMDec_error decError;
  
  // We keep a pointer to the last pictures that was output by the decoder. 
  // This is valid until we push more NAL units to the decoder.
  libJEMDec_picture *currentHMPic;

  // Statistics caching
  void cacheStatistics(libJEMDec_picture *pic);

  // Are we currently reading from the decoder or are we pushing more NAL units?
  bool stateReadingFrames;
  // The last pushed NAL unit. We hight have to push this again.
  QByteArray lastNALUnit;

  // The buffer and the index that was requested in the last call to getOneFrame
  int currentOutputBufferFrameIndex;
#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
  void copyImgToByteArray(libJEMDec_picture *src, byteArrayAligned &dst);
#else
  QByteArray currentOutputBuffer;
  void copyImgToByteArray(libJEMDec_picture *src, QByteArray &dst);   // Copy the raw data from the de265_image source *src to the byte array
#endif
};

#endif // HEVCNEXTGENDECODERJEM_H
