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

#include "decoderFFmpeg.h"

#define DECODERFFMPEG_DEBUG_OUTPUT 1
#if DECODERFFMPEG_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

decoderFFmpeg::decoderFFmpeg(AVCodecID codec, bool cachingDecoder) : 
  decoderBase(cachingDecoder)
{
  if (!createDecoder(codec))
  {
    setError("Error creating the needed decoder.");
    return ;
  }

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::decoderFFmpeg(AVCodecParametersWrapper codecpar, bool cachingDecoder) :
  decoderBase(cachingDecoder)
{
  AVCodecID codec = codecpar.getCodecID();
  if (!createDecoder(codec, codecpar))
  {
    setError("Error creating the needed decoder.");
    return ;
  }

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::~decoderFFmpeg()
{
}

void decoderFFmpeg::resetDecoder()
{
}

void decoderFFmpeg::decodeNextFrame()
{
}

QByteArray decoderFFmpeg::getYUVFrameData()
{
  return QByteArray();
}

void decoderFFmpeg::pushData(QByteArray &data)
{
}

void decoderFFmpeg::pushAVPacket(AVPacketWrapper &pkt)
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Wrong decoder state.");
    return;
  }


}

statisticsData decoderFFmpeg::getStatisticsData(int typeIdx)
{
  return statisticsData();
}

void decoderFFmpeg::fillStatisticList(statisticHandler &statSource) const
{
}

bool decoderFFmpeg::createDecoder(AVCodecID streamCodecID, AVCodecParametersWrapper codecpar)
{
  // Try to load the decoder library (.dll on Windows, .so on Linux, .dylib on Mac)
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without loading the libraries.
  if (!ff.loadFFmpegLibraries())
    return false;

  // Allocate the decoder context
  if (videoCodec)
    return setErrorB(QStringLiteral("Video codec already allocated."));
  videoCodec = ff.find_decoder(streamCodecID);
  if(!videoCodec)
    return setErrorB(QStringLiteral("Could not find a video decoder (avcodec_find_decoder)"));

  if (decCtx)
    return setErrorB(QStringLiteral("Decoder context already allocated."));
  decCtx = ff.alloc_decoder(videoCodec);
  if(!decCtx)
    return setErrorB(QStringLiteral("Could not allocate video deocder (avcodec_alloc_context3)"));

  if (codecpar)
    if (!ff.configureDecoder(decCtx, codecpar))
      return setErrorB(QStringLiteral("Unable to configure decoder from codecpar"));

  // Ask the decoder to provide motion vectors (if possible)
  AVDictionaryWrapper opts;
  int ret = ff.av_dict_set(opts, "flags2", "+export_mvs", 0);
  if (ret < 0)
    return setErrorB(QStringLiteral("Could not request motion vector retrieval. Return code %1").arg(ret));

  // Open codec
  ret = ff.avcodec_open2(decCtx, videoCodec, opts);
  if (ret < 0)
    return setErrorB(QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));

  frame.allocate_frame(ff);
  if (!frame)
    return setErrorB(QStringLiteral("Could not allocate frame (av_frame_alloc)."));

  return true;
}
