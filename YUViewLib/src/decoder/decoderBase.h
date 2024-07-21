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

#include <common/NewEnumMapper.h>
#include <filesource/FileSourceAnnexBFile.h>
#include <statistics/StatisticsData.h>
#include <video/rgb/videoHandlerRGB.h>
#include <video/yuv/videoHandlerYUV.h>

#include <QLibrary>

namespace decoder
{

// Each decoder is in one of these states
enum class DecoderState
{
  NeedsMoreData,  ///< The decoder needs more data (pushData). When there is no more data,
                  ///< push an empty array to flush out frames.
  RetrieveFrames, ///< Retrieve frames from the decoder (decodeNextFrame)
  EndOfBitstream, ///< Decoding has ended.
  Error
};

enum class DecoderEngine
{
  Invalid,
  Libde265, // The libde265 decoder
  HM,       // The HM reference software decoder
  VTM,      // The VTM reference software decoder
  VVDec,    // The VVDec VVC decoder
  Dav1d,    // The dav1d AV1 decoder
  FFMpeg    // The FFMpeg decoder
};

constexpr NewEnumMapper<DecoderEngine, 7>
    DecoderEngineMapper(std::make_pair(DecoderEngine::Invalid, "Invalid"sv),
                        std::make_pair(DecoderEngine::Libde265, "Libde265"sv),
                        std::make_pair(DecoderEngine::HM, "HM"sv),
                        std::make_pair(DecoderEngine::VTM, "VTM"sv),
                        std::make_pair(DecoderEngine::VVDec, "VVDec"sv),
                        std::make_pair(DecoderEngine::Dav1d, "Dav1d"sv),
                        std::make_pair(DecoderEngine::FFMpeg, "FFMpeg"sv));

const auto DecodersHEVC =
    std::vector<DecoderEngine>({DecoderEngine::Libde265, DecoderEngine::HM, DecoderEngine::FFMpeg});
const auto DecodersVVC = std::vector<DecoderEngine>({DecoderEngine::VVDec, DecoderEngine::VTM});
const auto DecodersAV1 = std::vector<DecoderEngine>({DecoderEngine::FFMpeg, DecoderEngine::Dav1d});

/* This class is the abstract base class for all decoders. All decoders work like this:
 * 1. Create an instance and configure it (if required)
 * 2. Push data to the decoder until it returns that it can not take any more data.
 *    When you pushed all of the bitstream into the decoder, push an empty QByteArray to indicate
 * the EOF.
 * 3. Read frames until no new frames are coming out. Go back to 2.
 */
class decoderBase
{
public:
  // Create a new decoder. cachingDecoder: Is this a decoder used for caching or interactive
  // decoding?
  decoderBase(bool cachingDecoder = false);
  virtual ~decoderBase() {};

  // Reset the decoder. Afterwards, the decoder should behave as if you just created a new one
  // (without the overhead of reloading the libraries). This must be used in case of errors or when
  // seeking.
  virtual void resetDecoder();

  // Does the loaded library support the extraction of prediction/residual data?
  // These are the default implementations. Overload if a decoder can support more signals.
  virtual int         nrSignalsSupported() const { return 1; }
  virtual QStringList getSignalNames() const { return QStringList() << "Reconstruction"; }
  virtual bool        isSignalDifference(int signalID) const;
  virtual void        setDecodeSignal(int signalID, bool &decoderResetNeeded);
  int                 getDecodeSignal() const { return this->decodeSignal; }

  // -- The decoding interface
  // If the current frame is valid, the current frame can be retrieved using getRawFrameData.
  // Call decodeNextFrame to advance to the next frame. When the function returns false, more data
  // is probably needed.
  virtual bool               decodeNextFrame() = 0;
  virtual QByteArray         getRawFrameData() = 0;
  video::RawFormat           getRawFormat() const { return this->rawFormat; }
  video::yuv::PixelFormatYUV getPixelFormatYUV() const { return this->formatYUV; }
  video::rgb::PixelFormatRGB getRGBPixelFormat() const { return this->formatRGB; }
  Size                       getFrameSize() const { return this->frameSize; }
  // Push data to the decoder (until no more data is needed)
  // In order to make the interface generic, the pushData function accepts data only without start
  // codes
  virtual bool pushData(QByteArray &data) = 0;

  DecoderState state() const { return this->decoderState; }

  // Get the statistics values for the current frame. In order to enable statistics retrievel,
  // activate it, reset the decoder and decode to the current frame again.
  bool statisticsSupported() const { return internalsSupported; }
  bool statisticsEnabled() const { return statisticsData != nullptr; }
  void enableStatisticsRetrieval(stats::StatisticsData *s) { this->statisticsData = s; }
  stats::FrameTypeData getCurrentFrameStatsForType(int typeIdx) const;
  virtual void         fillStatisticList(stats::StatisticsData &) const {};

  // Error handling
  bool    errorInDecoder() const { return decoderState == DecoderState::Error; }
  QString decoderErrorString() const { return errorString; }

  // Get the name, filename and full path to the decoder library(s) that is/are being used.
  // The length of the list must be a multiple of 3 (name, libName, fullPath)
  virtual QStringList getLibraryPaths() const = 0;

  // Get the decoder name (everyting that is needed to identify the decoder library) and the codec
  // that is being decoded. If needed, also version information (like HM 16.4)
  virtual QString getDecoderName() const = 0;
  virtual QString getCodecName() const   = 0;

protected:
  DecoderState decoderState{DecoderState::NeedsMoreData};

  int  decodeSignal{0};  ///< Which signal should be decoded?
  bool isCachingDecoder; ///< Is this the caching or the interactive decoder?

  bool internalsSupported{false}; ///< Enable in the constructor if you support statistics
  Size frameSize{};

  // Some decoders are able to handel both YUV and RGB output
  video::RawFormat           rawFormat{};
  video::yuv::PixelFormatYUV formatYUV{};
  video::rgb::PixelFormatRGB formatRGB{};

  // Error handling
  void setError(const QString &reason)
  {
    decoderState = DecoderState::Error;
    errorString  = reason;
  }
  bool setErrorB(const QString &reason)
  {
    setError(reason);
    return false;
  }
  QString errorString{};

  // If set, fill it (if possible). The playlistItem has ownership of this.
  stats::StatisticsData *statisticsData{};
};

// This abstract base class extends the decoderBase class by the ability to load one single library
// using QLibrary. For decoding, the decoderBase interface is not changed.
class decoderBaseSingleLib : public decoderBase
{
public:
  decoderBaseSingleLib(bool cachingDecoder = false) : decoderBase(cachingDecoder) {};
  virtual ~decoderBaseSingleLib() {};

  QStringList getLibraryPaths() const override
  {
    return QStringList() << getDecoderName() << library.fileName() << library.fileName();
  }

protected:
  virtual void resolveLibraryFunctionPointers() = 0;
  void         loadDecoderLibrary(QString specificLibrary);

  // Get all possible names of the library that we will load
  virtual QStringList getLibraryNames() const = 0;

  QLibrary library;
  QString  libraryPath{};
};

} // namespace decoder
