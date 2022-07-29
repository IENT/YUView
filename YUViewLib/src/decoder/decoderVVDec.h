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

#include "common/FileInfo.h"
#include "decoderBase.h"
#include "externalHeader/vvdec/vvdec.h"
#include "video/videoHandlerYUV.h"

namespace decoder
{

struct LibraryFunctionsVVDec
{
  // General functions
  const char *(*vvdec_get_version)(void){};

  vvdecAccessUnit *(*vvdec_accessUnit_alloc)(){};
  void (*vvdec_accessUnit_free)(vvdecAccessUnit *accessUnit){};
  void (*vvdec_accessUnit_alloc_payload)(vvdecAccessUnit *accessUnit, int payload_size){};
  void (*vvdec_accessUnit_free_payload)(vvdecAccessUnit *accessUnit){};
  void (*vvdec_accessUnit_default)(vvdecAccessUnit *accessUnit){};

  void (*vvdec_params_default)(vvdecParams *param){};
  vvdecParams *(*vvdec_params_alloc)(){};
  void (*vvdec_params_free)(vvdecParams *params){};

  vvdecDecoder *(*vvdec_decoder_open)(vvdecParams *){};
  int (*vvdec_decoder_close)(vvdecDecoder *){};

  int (*vvdec_set_logging_callback)(vvdecDecoder *, vvdecLoggingCallback callback){};
  int (*vvdec_decode)(vvdecDecoder *, vvdecAccessUnit *accessUnit, vvdecFrame **frame){};
  int (*vvdec_flush)(vvdecDecoder *, vvdecFrame **frame){};
  int (*vvdec_frame_unref)(vvdecDecoder *, vvdecFrame *frame){};

  int (*vvdec_get_hash_error_count)(vvdecDecoder *){};
  const char *(*vvdec_get_dec_information)(vvdecDecoder *){};
  const char *(*vvdec_get_last_error)(vvdecDecoder *){};
  const char *(*vvdec_get_last_additional_error)(vvdecDecoder *){};
  const char *(*vvdec_get_error_msg)(int nRet){};
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
  QString getCodecName() const override { return "hevc"; }

  int nrSignalsSupported() const override { return this->nrSignals; }

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as a this decoder.
  decoderVVDec(){};

  // Return the possible names of the HM library
  QStringList getLibraryNames() const override;

  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() override;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol, bool optional = false);

  void allocateNewDecoder();

  vvdecDecoder *decoder{nullptr};
  vvdecAccessUnit* accessUnit{nullptr};
  vvdecFrame *currentFrame{nullptr};

  // Try to get the next picture from the decoder and save it in currentHMPic
  bool getNextFrameFromDecoder();

  int  nrSignals{0};
  bool flushing{false};

  // We buffer the current image as a QByteArray so you can call getYUVFrameData as often as
  // necessary without invoking the copy operation from the hm image buffer to the QByteArray again.
  QByteArray currentOutputBuffer;
  void       copyImgToByteArray(QByteArray &dst);

  bool currentFrameReadyForRetrieval{};

  LibraryFunctionsVVDec lib{};
};

}
