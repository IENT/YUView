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

#include "decoderFFmpeg.h"

#define DECODERFFMPEG_DEBUG_OUTPUT 0
#if DECODERFFMPEG_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

using namespace YUV_Internals;
using namespace RGB_Internals;

decoderFFmpeg::decoderFFmpeg(AVCodecIDWrapper codecID, QSize size, QByteArray extradata, yuvPixelFormat fmt, QPair<int,int> profileLevel, QPair<int,int> sampleAspectRatio, bool cachingDecoder) : 
  decoderBase(cachingDecoder)
{
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without loading 
  // the libraries which is slow and uses a lot of memory.
  if (!ff.loadFFmpegLibraries())
    return;

  // Create the cofiguration parameters
  AVCodecParametersWrapper codecpar = ff.alloc_code_parameters();
  codecpar.setAVMediaType(AVMEDIA_TYPE_VIDEO);
  codecpar.setAVCodecID(ff.getCodecIDFromWrapper(codecID));
  codecpar.setSize(size.width(), size.height());
  codecpar.setExtradata(extradata);
  
  AVPixelFormat f = ff.getAVPixelFormatFromYUVPixelFormat(fmt);
  if (f == AV_PIX_FMT_NONE)
  {
    setError("Error determining the AVPixelFormat.");
    return;
  }
  codecpar.setAVPixelFormat(f);
  
  codecpar.setProfileLevel(profileLevel.first, profileLevel.second);
  codecpar.setSampleAspectRatio(sampleAspectRatio.first, sampleAspectRatio.second);

  if (!createDecoder(codecID, codecpar))
  {
    setError("Error creating the needed decoder.");
    return;
  }

  flushing = false;
  internalsSupported = true;
  // Fill the padding array
  for (int i=0; i<AV_INPUT_BUFFER_PADDING_SIZE; i++)
    avPacketPaddingData.append((char)0);

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::decoderFFmpeg(AVCodecParametersWrapper codecpar, bool cachingDecoder) :
  decoderBase(cachingDecoder)
{
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without loading 
  // the libraries which is slow and uses a lot of memory.
  if (!ff.loadFFmpegLibraries())
    return;

  AVCodecIDWrapper codecID = ff.getCodecIDWrapper(codecpar.getCodecID());
  if (!createDecoder(codecID, codecpar))
  {
    setError("Error creating the needed decoder.");
    return ;
  }

  flushing = false;
  internalsSupported = true;

  DEBUG_FFMPEG("Created new FFMpeg decoder - codec %s%s", ff.getCodecName(codec), cachingDecoder ? " - caching" : "");
}

decoderFFmpeg::~decoderFFmpeg()
{
  if (frame)
    frame.free_frame(ff);
  if (raw_pkt)
    raw_pkt.free_packet();
}

void decoderFFmpeg::resetDecoder()
{
  if (decoderState == decoderError)
    return;
    
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
  
  if (retrieveStatistics)
    // Get the statistics from the image and put them into the statistics cache
    cacheCurStatistics();

  return true;
}

QByteArray decoderFFmpeg::getRawFrameData()
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

  //// get metadata
  //AVDictionaryWrapper dict = ff.get_metadata(frame);
  //QStringPairList values = ff.get_dictionary_entries(dict, "", 0);

  if (rawFormat == raw_YUV)
  {
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
  else if (rawFormat == raw_RGB)
  {
    const rgbPixelFormat pixFmt = getRGBPixelFormat();
    const int nrBytesPerSample = pixFmt.bitsPerValue <= 8 ? 1 : 2;
    const int nrBytesPerComponent = frameSize.width() * frameSize.height() * nrBytesPerSample;
    const int nrBytes = 3 * nrBytesPerComponent;

    // Is the output big enough?
    if (currentOutputBuffer.capacity() < nrBytes)
      currentOutputBuffer.resize(nrBytes);

    char* dst = currentOutputBuffer.data();
    int hDst = frameSize.height();
    if (pixFmt.planar)
    {
      // Copy line by line. The linesize of the source may be larger than the width of the frame.
      // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
      // resolution may be larger than the output frame size.
      const int wDst = frameSize.width() * nrBytesPerSample;
      for (int i = 0; i < 3; i++)
      {
        uint8_t *src = frame.get_data(i);
        int linesize = frame.get_line_size(i);
        for (int y = 0; y < hDst; y++)
        {
          memcpy(dst, src, wDst);
          // Goto the next line
          dst += wDst;
          src += linesize;
        }
      }
    }
    else
    {
      // We only need to iterate over the image once and copy all values per line at once (RGB(A))
      const int wDst = frameSize.width() * nrBytesPerSample * pixFmt.nrChannels();
      for (int y = 0; y < hDst; y++)
      {
        uint8_t *src = frame.get_data(0);
        int linesize = frame.get_line_size(0);
        memcpy(dst, src, wDst);
        // Goto the next line
        dst += wDst;
        src += linesize;
      }
    }
  }
}

void decoderFFmpeg::cacheCurStatistics()
{
  // Copy the statistics of the current frame to the buffer
  DEBUG_FFMPEG("decoderFFmpeg::cacheCurStatistics");

  // Clear the local statistics cache
  curPOCStats.clear();

  // Try to get the motion information
  AVFrameSideDataWrapper sd = ff.get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
  if (sd)
  { 
    const int nrMVs = sd.get_number_motion_vectors();
    for (int i = 0; i < nrMVs; i++)
    {
      AVMotionVectorWrapper mvs = sd.get_motion_vector(i);

      // dst marks the center of the current block so the block position is:
      const int blockX = mvs.dst_x - mvs.w/2;
      const int blockY = mvs.dst_y - mvs.h/2;
      const int16_t mvX = mvs.dst_x - mvs.src_x;
      const int16_t mvY = mvs.dst_y - mvs.src_y;

      curPOCStats[mvs.source < 0 ? 0 : 1].addBlockValue(blockX, blockY, mvs.w, mvs.h, (int)mvs.source);
      curPOCStats[mvs.source < 0 ? 2 : 3].addBlockVector(blockX, blockY, mvs.w, mvs.h, mvX, mvY);
    }
  }
}

bool decoderFFmpeg::pushData(QByteArray &data)
{
  if (!raw_pkt)
    raw_pkt.allocate_paket(ff);
  if (data.length() == 0)
  {
    // Push an empty packet to indicate that the file has ended
    DEBUG_FFMPEG("decoderFFmpeg::pushData: Pushing an empty packet");
    AVPacketWrapper emptyPacket;
    return pushAVPacket(emptyPacket);
  }
  else
    DEBUG_FFMPEG("decoderFFmpeg::pushData: Pushing data length %d", data.length());

  // Add some padding
  data.append(avPacketPaddingData);

  raw_pkt.set_data(data);
  raw_pkt.set_dts(AV_NOPTS_VALUE);
  raw_pkt.set_pts(AV_NOPTS_VALUE);

  return pushAVPacket(raw_pkt);
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

  // Push the packet to the decoder
  int retPush = ff.pushPacketToDecoder(decCtx, pkt);

  if (retPush < 0 && retPush != AVERROR(EAGAIN))
  {
#if DECODERFFMPEG_DEBUG_OUTPUT
    {
      QString meaning = QString("decoderFFmpeg::pushAVPacket: Error sending packet - err %1").arg(retPush);
      if (retPush == 1094995529)
        meaning += " INDA";
      // Log the first bytes
      meaning += " B(";
      int nrBytes = std::min(pkt.get_data_size(), 5);
      uint8_t *data = pkt.get_data();
      for (int i = 0; i < nrBytes; i++)
      {
        uint8_t b = data[i];
        meaning += QString(" %1").arg(b, 2, 16);
      }
      meaning += ")";
      qDebug() << meaning;
    }
#endif
    setError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
    return false;
  }
  else
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Send packet PTS %ld duration %ld flags %d", pkt.get_pts(), pkt.get_duration(), pkt.get_flags());
  
  if (retPush == AVERROR(EAGAIN))
  {
    // Enough data pushed. Decode and retrieve frames now.
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Enough data pushed. Decode and retrieve frames now.");
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

void decoderFFmpeg::fillStatisticList(statisticHandler &statSource) const
{
  StatisticsType refIdx0(0, "Source -", "col3_bblg", -2, 2);
  statSource.addStatType(refIdx0);

  StatisticsType refIdx1(1, "Source +", "col3_bblg", -2, 2);
  statSource.addStatType(refIdx1);

  StatisticsType motionVec0(2, "Motion Vector -", 4);
  statSource.addStatType(motionVec0);

  StatisticsType motionVec1(3, "Motion Vector +", 4);
  statSource.addStatType(motionVec1);
}

bool decoderFFmpeg::createDecoder(AVCodecIDWrapper codecID, AVCodecParametersWrapper codecpar)
{
  // Allocate the decoder context
  if (videoCodec)
    return setErrorB(QStringLiteral("Video codec already allocated."));
  videoCodec = ff.find_decoder(codecID);
  if(!videoCodec)
    return setErrorB(QStringLiteral("Could not find a video decoder for the given codec ") + codecID.getCodecName());

  if (decCtx)
    return setErrorB(QStringLiteral("Decoder context already allocated."));
  decCtx = ff.alloc_decoder(videoCodec);
  if(!decCtx)
    return setErrorB(QStringLiteral("Could not allocate video deocder (avcodec_alloc_context3)"));

  if (codecpar)
  {
    if (!ff.configureDecoder(decCtx, codecpar))
      return setErrorB(QStringLiteral("Unable to configure decoder from codecpar"));
  }

  // Get some parameters from the decoder context
  frameSize = QSize(decCtx.get_width(), decCtx.get_height());

  AVPixFmtDescriptorWrapper ffmpegPixFormat = ff.getAvPixFmtDescriptionFromAvPixelFormat(decCtx.get_pixel_format());
  rawFormat = ffmpegPixFormat.getRawFormat();
  if (rawFormat == raw_YUV)
    formatYUV = ffmpegPixFormat.getYUVPixelFormat();
  else if (rawFormat == raw_RGB)
    formatRGB = ffmpegPixFormat.getRGBPixelFormat();

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

QString decoderFFmpeg::getCodecName()
{
  if (!decCtx)
    return "";

  return ff.getCodecIDWrapper(decCtx.getCodecID()).getCodecName();
}