/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include <common/Typedef.h>
#include <decoder/decoderBase.h>
#include <video/videoHandlerYUV.h>

#include "externalHeader/dav1d/blockData.h"
#include "externalHeader/dav1d/dav1d.h"

namespace decoder
{

struct LibraryFunctionsDav1d
{
  const char *(*dav1d_version)(){};
  void (*dav1d_default_settings)(Dav1dSettings *){};
  int (*dav1d_open)(Dav1dContext **, const Dav1dSettings *){};
  int (*dav1d_parse_sequence_header)(Dav1dSequenceHeader *, const uint8_t *, const size_t){};
  int (*dav1d_send_data)(Dav1dContext *, Dav1dData *){};
  int (*dav1d_get_picture)(Dav1dContext *, Dav1dPicture *){};
  void (*dav1d_close)(Dav1dContext **){};
  void (*dav1d_flush)(Dav1dContext *){};

  uint8_t *(*dav1d_data_create)(Dav1dData *data, size_t sz){};

  // The interface for the analizer. These might not be available in the library.
  void (*dav1d_default_analyzer_settings)(Dav1dAnalyzerFlags *s){};
  int (*dav1d_set_analyzer_settings)(Dav1dContext *c, const Dav1dAnalyzerFlags *s){};
};

class Dav1dPictureWrapper
{
public:
  Dav1dPictureWrapper() {}

  void setInternalsSupported() { internalsSupported = true; }

  void                    clear() { memset(&curPicture, 0, sizeof(Dav1dPicture)); }
  Size                    getFrameSize() const { return Size({curPicture.p.w, curPicture.p.h}); }
  Dav1dPicture *          getPicture() const { return (Dav1dPicture *)(&curPicture); }
  video::yuv::Subsampling getSubsampling() const;
  int                     getBitDepth() const { return curPicture.p.bpc; }
  uint8_t * getData(int component) const { return (uint8_t *)curPicture.data[component]; }
  ptrdiff_t getStride(int component) const { return curPicture.stride[component]; }
  uint8_t * getDataPrediction(int component) const
  {
    return internalsSupported ? (uint8_t *)curPicture.pred[component] : nullptr;
  }
  uint8_t *getDataReconstructionPreFiltering(int component) const
  {
    return internalsSupported ? (uint8_t *)curPicture.pre_lpf[component] : nullptr;
  }
  Av1Block *getBlockData() const
  {
    return internalsSupported ? reinterpret_cast<Av1Block *>(curPicture.blk_data) : nullptr;
  }

  Dav1dSequenceHeader *getSequenceHeader() const { return curPicture.seq_hdr; }
  Dav1dFrameHeader *   getFrameHeader() const { return curPicture.frame_hdr; }

private:
  Dav1dPicture curPicture;
  bool         internalsSupported{false};
};

struct dav1dFrameInfo
{
  dav1dFrameInfo(Size s, Dav1dFrameType frameType);
  Size           frameSize{};
  Size           frameSizeAligned{};
  Size           sizeInBlocks{};
  Size           sizeInBlocksAligned{};
  int            b4_stride{};
  Dav1dFrameType frameType{};
};

// This class wraps the libde265 library in a demand-load fashion.
class decoderDav1d : public decoderBaseSingleLib
{
public:
  decoderDav1d(int signalID, bool cachingDecoder = false);
  ~decoderDav1d();

  void resetDecoder() override;

  int         nrSignalsSupported() const override { return nrSignals; }
  bool        isSignalDifference(int signalID) const override;
  QStringList getSignalNames() const override;

  DecoderResetNeeded setDecodeSignal(int signalID) override;

  // Decoding / pushing data
  bool       decodeNextFrame() override;
  QByteArray getRawFrameData() override;
  bool       pushData(QByteArray &data) override;

  // Check if the given library file is an existing libde265 decoder that we can use.
  static bool checkLibraryFile(QString libFilePath, QString &error);

  QString getDecoderName() const override;
  QString getCodecName() const override { return "AV1"; }

private:
  // A private constructor that creates an uninitialized decoder library.
  // Used by checkLibraryFile to check if a file can be used as a hevcDecoderLibde265.
  decoderDav1d() : decoderBaseSingleLib(){};

  // Try to resolve all the required function pointers from the library
  void resolveLibraryFunctionPointers() override;

  // Return the possible names of the dav1d library
  QStringList getLibraryNames() const override;

  // The function template for resolving the functions.
  // This can not go into the base class because then the template
  // generation does not work.
  template <typename T> T resolve(T &ptr, const char *symbol, bool optional = false);

  void allocateNewDecoder();

  Dav1dContext *     decoder{};
  Dav1dSettings      settings;
  Dav1dAnalyzerFlags analyzerSettings;

  int  nrSignals{1};
  bool flushing{};
  bool sequenceHeaderPushed{};

  // When pushing frames, the decoder will try to decode a frame to check if this is possible.
  // If this is true, a frame is waiting from that step and decodeNextFrame will not actually decode
  // a new frame.
  bool decodedFrameWaiting{};

  // Try to decode a frame. If successful, the frame will be in curPicture.
  bool decodeFrame();

#if SSE_CONVERSION
  byteArrayAligned currentOutputBuffer;
#else
  QByteArray currentOutputBuffer;
#endif

  Dav1dPictureWrapper curPicture;

  stats::StatisticsTypes getStatisticsTypes() const override;

  unsigned int subBlockSize{};

  LibraryFunctionsDav1d lib{};
};

} // namespace decoder
