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

#define DECODERFFMPEG_DEBUG_OUTPUT 0
#if DECODERFFMPEG_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

decoderFFmpeg::decoderFFmpeg(AVCodecID codec, QSize size, yuvPixelFormat fmt, bool cachingDecoder) : 
  decoderBase(cachingDecoder)
{
  if (!createDecoder(codec))
  {
    setError("Error creating the needed decoder.");
    return ;
  }

  format = fmt;
  frameSize = size;
  flushing = false;

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::decoderFFmpeg(AVCodecParametersWrapper codecpar, yuvPixelFormat fmt, bool cachingDecoder) :
  decoderBase(cachingDecoder)
{
  AVCodecID codec = codecpar.getCodecID();
  if (!createDecoder(codec, codecpar))
  {
    setError("Error creating the needed decoder.");
    return ;
  }

  format = fmt;
  flushing = false;

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::~decoderFFmpeg()
{
  if (frame)
    frame.free_frame(ff);
}

void decoderFFmpeg::resetDecoder()
{
  DEBUG_FFMPEG("decoderFFmpeg::resetDecoder");
  ff.flush_buffers(decCtx);
  decoderState = decoderNeedsMoreData;
  flushing = false;
}

bool decoderFFmpeg::decodeNextFrame()
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_FFMPEG("decoderFFmpeg::decodeNextFrame: Wrong decoder state.");
    return false;
  }

  DEBUG_FFMPEG("decoderFFmpeg::decodeNextFrame");

  if (!decodeFrame())
    return false;

  copyCurImageToBuffer();
  return true;
}

QByteArray decoderFFmpeg::getYUVFrameData()
{
  if (decoderState != decoderRetrieveFrames)
  {
    DEBUG_FFMPEG("decoderFFmpeg::getYUVFrameData: Wrong decoder state.");
    return QByteArray();
  }

  DEBUG_FFMPEG("decoderFFmpeg::getYUVFrameData Copy frame");

  if (currentOutputBuffer.isEmpty())
    DEBUG_FFMPEG("decoderFFmpeg::loadYUVFrameData empty buffer");

  return currentOutputBuffer;
}

void decoderFFmpeg::copyCurImageToBuffer()
{
  if (!frame)
    return;

  // At first get how many bytes we are going to write
  const yuvPixelFormat pixFmt = getYUVPixelFormat();
  const int nrBytesPerSample = pixFmt.bitsPerSample <= 8 ? 1 : 2;
  const int nrBytesY = frameSize.width() * frameSize.height() * nrBytesPerSample;
  const int nrBytesC = frameSize.width() / pixFmt.getSubsamplingHor() * frameSize.height() / pixFmt.getSubsamplingVer() * nrBytesPerSample;
  const int nrBytes = nrBytesY + 2 * nrBytesC;

  // Is the output big enough?
  if (currentOutputBuffer.capacity() < nrBytes)
    currentOutputBuffer.resize(nrBytes);

  // Copy line by line. The linesize of the source may be larger than the width of the frame.
  // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
  // resolution may be larger than the output frame size.
  uint8_t *src = frame.get_data(0);
  int linesize = frame.get_line_size(0);
  char* dst = currentOutputBuffer.data();
  int wDst = frameSize.width() * nrBytesPerSample;
  int hDst = frameSize.height();
  for (int y = 0; y < hDst; y++)
  {
    // Copy one line
    memcpy(dst, src, wDst);
    // Goto the next line in input and output (these offsets/strides may differ)
    dst += wDst;
    src += linesize;
  }

  // Chroma
  wDst = frameSize.width() / pixFmt.getSubsamplingHor() * nrBytesPerSample;
  hDst = frameSize.height() / pixFmt.getSubsamplingVer();
  for (int c = 0; c < 2; c++)
  {
    uint8_t *src = frame.get_data(c+1);
    linesize = frame.get_line_size(c+1);
    dst = currentOutputBuffer.data();
    dst += (nrBytesY + ((c == 0) ? 0 : nrBytesC));
    for (int y = 0; y < hDst; y++)
    {
      memcpy(dst, src, wDst);
      // Goto the next line
      dst += wDst;
      src += linesize;
    }
  }
}

void decoderFFmpeg::cacheCurStatistics()
{
}

void decoderFFmpeg::pushData(QByteArray &data)
{
  assert(false);
}

bool decoderFFmpeg::pushAVPacket(AVPacketWrapper &pkt)
{
  if (decoderState != decoderNeedsMoreData)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Wrong decoder state.");
    return false;
  }
  if (!pkt)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Recieved empty packet. Swithing to flushing.");
    flushing = true;
    decoderState = decoderRetrieveFrames;
    return false;
  }
  if (flushing)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Error no new packets should be pushed in flushing mode.");
    return false;
  }

  // We feed data to the decoder until it returns AVERROR(EAGAIN)
  int retPush = ff.pushPacketToDecoder(decCtx, pkt);

  if (retPush < 0 && retPush != AVERROR(EAGAIN))
  {
    setError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
    return false;
  }
  else
    DEBUG_FFMPEG("Send packet PTS %ld duration %ld flags %d", pkt.get_pts(), pkt.get_duration(), pkt.get_flags());
  
  if (retPush == AVERROR(EAGAIN))
  {
    // Enough data pushed. Decode and retrieve frames now.
    decoderState = decoderRetrieveFrames;
    return false;
  }

  return true;
}

bool decoderFFmpeg::decodeFrame()
{
  // Try to retrive a next frame from the decoder (don't copy it yet).
  int retRecieve = ff.getFrameFromDecoder(decCtx, frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    DEBUG_FFMPEG("Recieved frame: Size(%dx%d) PTS %ld type %d %s", frame.get_width(), frame.get_height(), frame.get_pts(), frame.get_pict_type(), frame.get_key_frame() ? "key frame" : "");
    // Checkt the size of the retrieved image
    if (frameSize != frame.get_size())
      return setErrorB("Recieved a frame of different size");
    return true;
  }
  else if (retRecieve < 0 && retRecieve != AVERROR(EAGAIN) && retRecieve != -35)
  {
    // An error occured
    DEBUG_FFMPEG("decoderFFmpeg::decodeFrame Error reading frame.");
    return setErrorB(QStringLiteral("Error recieving frame (avcodec_receive_frame)"));
  }
  else if (retRecieve == AVERROR(EAGAIN))
  {
    if (flushing)
    {
      // There is no more data
      decoderState = decoderEndOfBitstream;
      DEBUG_FFMPEG("decoderFFmpeg::decodeFrame End of bitstream.");
    }
    else
    {
      decoderState = decoderNeedsMoreData;
      DEBUG_FFMPEG("decoderFFmpeg::decodeFrame Need more data.");
    }    
  }
  return false;
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
  {
    if (!ff.configureDecoder(decCtx, codecpar))
      return setErrorB(QStringLiteral("Unable to configure decoder from codecpar"));

    // Get some parameters from the codec parameters
    //format = codecpar.get
    frameSize = QSize(codecpar.get_width(), codecpar.get_height());
  }

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
