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

#pragma once

#include <QLibrary>

#include "common/fileInfo.h"
#include "decoderBase.h"
#include "externalHeader/libvvdec.h"
#include "statistics/statisticsExtensions.h"
#include "video/videoHandlerYUV.h"

struct LibraryFunctions
{
  // General functions
  const char *(*libvvdec_get_version)(void){};
  libvvdec_context *(*libvvdec_new_decoder)(void){};
  libvvdec_error (*libvvdec_set_logging_callback)(libvvdec_context *,
                                                    libvvdec_logging_callback,
                                                    void *             userData,
                                                    libvvdec_loglevel loglevel){};
  libvvdec_error (*libvvdec_free_decoder)(libvvdec_context *){};
  libvvdec_error (*libvvdec_push_nal_unit)(libvvdec_context *,
                                             const unsigned char *,
                                             int,
                                             bool &){};

  // Picture retrieval
  uint64_t (*libvvdec_get_picture_POC)(libvvdec_context *){};
  uint32_t (*libvvdec_get_picture_width)(libvvdec_context *, libvvdec_ColorComponent){};
  uint32_t (*libvvdec_get_picture_height)(libvvdec_context *, libvvdec_ColorComponent){};
  int32_t (*libvvdec_get_picture_stride)(libvvdec_context *, libvvdec_ColorComponent){};
  unsigned char *(*libvvdec_get_picture_plane)(libvvdec_context *, libvvdec_ColorComponent){};
  libvvdec_ChromaFormat (*libvvdec_get_picture_chroma_format)(libvvdec_context *){};
  uint32_t (*libvvdec_get_picture_bit_depth)(libvvdec_context *, libvvdec_ColorComponent){};
};

// This class wraps the decoder library in a demand-load fashion.
class decoderVVDec : public decoderBaseSingleLib
{
public:
  decoderVVDec(int signalID, bool cachingDecoder = false);
  ~decoderVVDec();

  void resetDecoder() override;

  // Decoding / pushing data
  bool       decodeNextFrame() override;
  QByteArray getRawFrameData() override;
  bool       pushData(QByteArray &data) override;

  // Check if the given library file is an existing libde265 decoder that we can use.
  static bool checkLibraryFile(QString libFilePath, QString &error);

  QString getDecoderName() const override;
  QString getCodecName() override { return "hevc"; }

  int nrSignalsSupported() const override { return nrSignals; }

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as a this decoder.
  decoderVVDec(){};

  // Return the possible names of the HM library
  QStringList getLibraryNames() override;

  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() override;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol, bool optional = false);

  void allocateNewDecoder();

  libvvdec_context *decoder{nullptr};

  // Try to get the next picture from the decoder and save it in currentHMPic
  bool getNextFrameFromDecoder();

  int  nrSignals{0};
  bool flushing{false};

  YUV_Internals::Subsampling convertFromInternalSubsampling(libvvdec_ChromaFormat fmt);

  // We buffer the current image as a QByteArray so you can call getYUVFrameData as often as
  // necessary without invoking the copy operation from the hm image buffer to the QByteArray again.
  QByteArray currentOutputBuffer;
  void       copyImgToByteArray(QByteArray &dst);

  bool currentFrameReadyForRetrieval{};

  LibraryFunctions lib{};
};
