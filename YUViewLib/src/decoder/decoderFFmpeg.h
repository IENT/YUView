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

#include "decoderBase.h"
#include "ffmpeg/FFMpegLibrariesHandling.h"

class decoderFFmpeg : public decoderBase
{
public:
  decoderFFmpeg(AVCodecIDWrapper codec, QSize frameSize, QByteArray extradata, YUV_Internals::yuvPixelFormat fmt, IntPair profileLevel, Ratio sampleAspectRatio, bool cachingDecoder=false);
  decoderFFmpeg(AVCodecParametersWrapper codecpar, bool cachingDecoder=false);
  ~decoderFFmpeg();

  void resetDecoder() override;

  // Decoding / pushing data
  bool decodeNextFrame() override;
  QByteArray getRawFrameData() override;
  
  // Push an AVPacket or raw data. When this returns false, pushing the given packet failed. Probably the 
  // decoder switched to DecoderState::RetrieveFrames. Don't forget to push the given packet again later.
  bool pushAVPacket(AVPacketWrapper &pkt);
  bool pushData(QByteArray &data) override;

  // What statistics do we support?
  void fillStatisticList(stats::StatisticsData &statisticsData) const override;

  QStringList getLibraryPaths() const override { return ff.getLibPaths(); }
  QString     getDecoderName()  const override { return "FFmpeg"; }
  QString     getCodecName()          override;
  
  static QStringList getLogMessages() { return FFmpegVersionHandler::getFFmpegLog(); }

protected:

  FFmpegVersionHandler ff;

  bool createDecoder(AVCodecIDWrapper codecID, AVCodecParametersWrapper codecpar=AVCodecParametersWrapper());

  AVCodecWrapper videoCodec;        //< The video decoder codec
  AVCodecContextWrapper decCtx;     //< The decoder context
  AVFrameWrapper frame;             //< The frame that we use for decoding

  // Try to decode a frame. If successful, the frame will be in "frame" and return true.
  bool decodeFrame();

  // Statistics caching
  void cacheCurStatistics();

  QByteArray currentOutputBuffer;
  void copyCurImageToBuffer();   // Copy the raw data from the de265_image source *src to the byte array

  // At the end of the file, when no more data is available, we will swith to flushing. After all
  // remaining frames were decoding, we will not request more data but switch to DecoderState::EndOfBitstream.
  bool flushing;

  // When pushing raw data to the decoder, we need to package it into AVPackets
  AVPacketWrapper raw_pkt;

  // An array of AV_INPUT_BUFFER_PADDING_SIZE zeros to be added as padding in pushData
  QByteArray avPacketPaddingData;
};
