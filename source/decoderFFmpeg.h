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

#ifndef DECODERFFMPEG_H
#define DECODERFFMPEG_H

#include "decoderBase.h"
#include "FFMpegLibrariesHandling.h"

class decoderFFmpeg : public decoderBase
{
public:
  decoderFFmpeg(AVCodecIDWrapper codec, QSize frameSize, QByteArray extradata, yuvPixelFormat fmt, QPair<int,int> profileLevel, QPair<int,int> sampleAspectRatio, bool cachingDecoder=false);
  decoderFFmpeg(AVCodecParametersWrapper codecpar, bool cachingDecoder=false);
  ~decoderFFmpeg();

  void resetDecoder() Q_DECL_OVERRIDE;

  // Decoding / pushing data
  bool decodeNextFrame() Q_DECL_OVERRIDE;
  QByteArray getRawFrameData() Q_DECL_OVERRIDE;
  
  // Push an AVPacket or raw data. When this returns false, pushing the given packet failed. Probably the 
  // decoder switched to decoderRetrieveFrames. Don't forget to push the given packet again later.
  bool pushAVPacket(AVPacketWrapper &pkt);
  bool pushData(QByteArray &data) Q_DECL_OVERRIDE;

  // What statistics do we support?
  void fillStatisticList(statisticHandler &statSource) const Q_DECL_OVERRIDE;

  QStringList getLibraryPaths() const Q_DECL_OVERRIDE { return ff.getLibPaths(); }
  QString     getDecoderName()  const Q_DECL_OVERRIDE { return "FFmpeg"; }
  QString     getCodecName()          Q_DECL_OVERRIDE;
  
  static QStringList getLogMessages() { return FFmpegVersionHandler::getFFmpegLog(); }

protected:

  FFmpegVersionHandler ff;

  bool createDecoder(AVCodecIDWrapper codecID, AVCodecParametersWrapper codecpar=AVCodecParametersWrapper());

  AVCodecWrapper videoCodec;        //< The video decoder codec
  AVCodecContextWrapper decCtx;     //< The decoder context
  AVFrameWrapper frame;             //< The frame that we use for decoding

  // Try to decode a frame. If successfull, the frame will be in "frame" and return true.
  bool decodeFrame();

  // Statistics caching
  void cacheCurStatistics();

  QByteArray currentOutputBuffer;
  void copyCurImageToBuffer();   // Copy the raw data from the de265_image source *src to the byte array

  // At the end of the file, when no more data is available, we will swith to flushing. After all
  // remaining frames were decoding, we will not request more data but switch to decoderEndOfBitstream.
  bool flushing;

  // When pushing raw data to the decoder, we need to package it into AVPackets
  AVPacketWrapper raw_pkt;

  // An array of AV_INPUT_BUFFER_PADDING_SIZE zeros to be added as padding in pushData
  QByteArray avPacketPaddingData;
};

#endif // DECODERFFMPEG_H
