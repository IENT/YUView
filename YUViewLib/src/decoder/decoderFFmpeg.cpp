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

#include <common/Functions.h>

#define DECODERFFMPEG_DEBUG_OUTPUT 0
#if DECODERFFMPEG_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG(f) qDebug() << f
#else
#define DEBUG_FFMPEG(f) ((void)0)
#endif

namespace decoder
{

decoderFFmpeg::decoderFFmpeg(FFmpeg::AVCodecIDWrapper   codecID,
                             Size                       size,
                             QByteArray                 extradata,
                             video::yuv::PixelFormatYUV pixelFormatYUV,
                             IntPair                    profileLevel,
                             Ratio                      sampleAspectRatio,
                             bool                       cachingDecoder)
    : decoderBase(cachingDecoder)
{
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without
  // loading the libraries which is slow and uses a lot of memory.
  this->ff.loadFFmpegLibraries();
  if (!this->ff.loadingSuccessfull())
    return;

  // Create the cofiguration parameters
  auto codecpar = this->ff.allocCodecParameters();
  codecpar.setAVMediaType(FFmpeg::AVMEDIA_TYPE_VIDEO);
  codecpar.setAVCodecID(this->ff.getCodecIDFromWrapper(codecID));
  codecpar.setSize(size);
  codecpar.setExtradata(extradata);

  auto avPixelFormat = this->ff.getAVPixelFormatFromPixelFormatYUV(pixelFormatYUV);
  if (avPixelFormat == FFmpeg::AV_PIX_FMT_NONE)
  {
    this->setError("Error determining the AVPixelFormat.");
    return;
  }
  codecpar.setAVPixelFormat(avPixelFormat);

  codecpar.setProfileLevel(profileLevel.first, profileLevel.second);
  codecpar.setSampleAspectRatio(sampleAspectRatio.num, sampleAspectRatio.den);

  if (!createDecoder(codecID, codecpar))
  {
    this->setError("Error creating the needed decoder.");
    return;
  }

  this->flushing            = false;
  this->statisticsSupported = true;
  // Fill the padding array
  for (int i = 0; i < AV_INPUT_BUFFER_PADDING_SIZE; i++)
    this->avPacketPaddingData.append((char)0);

  DEBUG_FFMPEG("decoderFFmpeg::decoderFFmpeg Created new FFmpeg decoder - codec "
               << this->getCodecName() << (cachingDecoder ? " - caching" : ""));
}

decoderFFmpeg::decoderFFmpeg(FFmpeg::AVCodecParametersWrapper codecpar, bool cachingDecoder)
    : decoderBase(cachingDecoder)
{
  // The libraries are only loaded on demand. This way a FFmpegLibraries instance can exist without
  // loading the libraries which is slow and uses a lot of memory.
  this->ff.loadFFmpegLibraries();
  if (!this->ff.loadingSuccessfull())
    return;

  auto codecID    = this->ff.getCodecIDWrapper(codecpar.getCodecID());
  this->codecName = codecID.getCodecName();
  if (!this->createDecoder(codecID, codecpar))
  {
    this->setError("Error creating the needed decoder.");
    return;
  }

  this->flushing            = false;
  this->statisticsSupported = true;

  DEBUG_FFMPEG("decoderFFmpeg::decoderFFmpeg Created new FFmpeg decoder - codec "
               << this->getCodecName() << (cachingDecoder ? " - caching" : ""));
}

decoderFFmpeg::~decoderFFmpeg()
{
  if (this->frame)
    this->ff.freeFrame(this->frame);
  if (this->raw_pkt)
    this->ff.freePacket(this->raw_pkt);
}

void decoderFFmpeg::resetDecoder()
{
  if (this->decoderState == DecoderState::Error)
    return;

  DEBUG_FFMPEG("decoderFFmpeg::resetDecoder");
  this->ff.flush_buffers(this->decCtx);
  this->flushing = false;
  decoderBase::resetDecoder();
}

bool decoderFFmpeg::decodeNextFrame()
{
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_FFMPEG("decoderFFmpeg::decodeNextFrame: Wrong decoder state.");
    return false;
  }

  DEBUG_FFMPEG("decoderFFmpeg::decodeNextFrame");
  return this->decodeFrame();
}

QByteArray decoderFFmpeg::getRawFrameData()
{
  if (this->decoderState != DecoderState::RetrieveFrames)
  {
    DEBUG_FFMPEG("decoderFFmpeg::getYUVFrameData: Wrong decoder state.");
    return QByteArray();
  }

  DEBUG_FFMPEG("decoderFFmpeg::getYUVFrameData Copy frame");

  if (this->currentOutputBuffer.isEmpty())
  {
    DEBUG_FFMPEG("decoderFFmpeg::decodeNextFrame: Copy frame data to buffer");
    this->copyCurImageToBuffer();
  }

  return this->currentOutputBuffer;
}

stats::DataPerTypeMap decoderFFmpeg::getFrameStatisticsData()
{
  if (!this->statisticsEnabled)
    return {};

  DEBUG_FFMPEG("decoderFFmpeg::cacheCurStatistics");

  auto sideData = this->ff.getSideData(frame, FFmpeg::AV_FRAME_DATA_MOTION_VECTORS);
  if (!sideData)
    return {};

  stats::DataPerTypeMap data;

  const auto nrMVs = sideData.getNumberMotionVectors();
  for (size_t i = 0; i < nrMVs; i++)
  {
    auto mvs = sideData.getMotionVector(unsigned(i));

    auto          block = stats::Block(mvs.dst_x - mvs.w / 2, mvs.dst_y - mvs.h / 2, mvs.w, mvs.h);
    const int16_t mvX   = mvs.dst_x - mvs.src_x;
    const int16_t mvY   = mvs.dst_y - mvs.src_y;

    data[mvs.source < 0 ? 0 : 1].valueData.push_back(stats::BlockWithValue(block, int(mvs.source)));
    data[mvs.source < 0 ? 2 : 3].vectorData.push_back(
        stats::BlockWithVector(block, stats::Vector(mvX, mvY)));
  }

  return data;
}

void decoderFFmpeg::copyCurImageToBuffer()
{
  if (!frame)
    return;

  //// get metadata
  // AVDictionaryWrapper dict = this->ff.get_metadata(frame);
  // QStringPairList values = this->ff.getDictionary_entries(dict, "", 0);

  if (this->rawFormat == video::RawFormat::YUV)
  {
    // At first get how many bytes we are going to write
    const auto pixFmt           = this->getPixelFormatYUV();
    const auto nrBytesPerSample = pixFmt.getBitsPerSample() <= 8 ? 1 : 2;
    const auto nrBytesY         = this->frameSize.width * this->frameSize.height * nrBytesPerSample;
    const auto nrBytesC         = this->frameSize.width / pixFmt.getSubsamplingHor() *
                          this->frameSize.height / pixFmt.getSubsamplingVer() * nrBytesPerSample;
    const auto nrBytes = nrBytesY + 2 * nrBytesC;

    // Is the output big enough?
    if (auto c = functions::clipToUnsigned(this->currentOutputBuffer.capacity()); c < nrBytes)
      this->currentOutputBuffer.resize(nrBytes);

    // Copy line by line. The linesize of the source may be larger than the width of the frame.
    // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
    // resolution may be larger than the output frame size.
    for (unsigned plane = 0; plane < pixFmt.getNrPlanes(); plane++)
    {
      const auto component =
          (plane == 0) ? video::yuv::Component::Luma : video::yuv::Component::Chroma;
      auto       src         = frame.getData(plane);
      const auto srcLinesize = frame.getLineSize(plane);
      auto       dst         = this->currentOutputBuffer.data();
      if (plane > 0)
        dst += (nrBytesY + (plane - 1) * nrBytesC);
      const auto dstLinesize =
          this->frameSize.width / pixFmt.getSubsamplingHor(component) * nrBytesPerSample;
      const auto height = this->frameSize.height / pixFmt.getSubsamplingVer(component);
      for (unsigned y = 0; y < height; y++)
      {
        memcpy(dst, src, dstLinesize);
        dst += dstLinesize;
        src += srcLinesize;
      }
    }
  }
  else if (this->rawFormat == video::RawFormat::RGB)
  {
    const auto pixFmt           = this->getRGBPixelFormat();
    const auto nrBytesPerSample = pixFmt.getBitsPerSample() <= 8 ? 1 : 2;
    const auto nrBytesPerComponent =
        this->frameSize.width * this->frameSize.height * nrBytesPerSample;
    const auto nrBytes = nrBytesPerComponent * pixFmt.nrChannels();

    // Is the output big enough?
    if (auto c = functions::clipToUnsigned(this->currentOutputBuffer.capacity()); c < nrBytes)
      this->currentOutputBuffer.resize(nrBytes);

    auto       dst  = this->currentOutputBuffer.data();
    const auto hDst = this->frameSize.height;
    if (pixFmt.getDataLayout() == video::DataLayout::Planar)
    {
      // Copy line by line. The linesize of the source may be larger than the width of the frame.
      // This may be because the frame buffer is (8) byte aligned. Also the internal decoded
      // resolution may be larger than the output frame size.
      const auto wDst = this->frameSize.width * nrBytesPerSample;
      for (int i = 0; i < 3; i++)
      {
        auto       src         = frame.getData(i);
        const auto srcLinesize = frame.getLineSize(i);
        for (unsigned y = 0; y < hDst; y++)
        {
          memcpy(dst, src, wDst);
          // Goto the next line
          dst += wDst;
          src += srcLinesize;
        }
      }
    }
    else
    {
      // We only need to iterate over the image once and copy all values per line at once (RGB(A))
      const auto wDst        = this->frameSize.width * nrBytesPerSample * pixFmt.nrChannels();
      auto       src         = frame.getData(0);
      const auto srcLinesize = frame.getLineSize(0);
      for (unsigned y = 0; y < hDst; y++)
      {
        memcpy(dst, src, wDst);
        dst += wDst;
        src += srcLinesize;
      }
    }
  }
}

bool decoderFFmpeg::pushData(QByteArray &data)
{
  if (!this->raw_pkt)
    this->raw_pkt = this->ff.allocatePaket();
  if (data.length() == 0)
  {
    // Push an empty packet to indicate that the file has ended
    DEBUG_FFMPEG("decoderFFmpeg::pushData: Pushing an empty packet");
    FFmpeg::AVPacketWrapper emptyPacket;
    return this->pushAVPacket(emptyPacket);
  }
  else
    DEBUG_FFMPEG("decoderFFmpeg::pushData: Pushing data length " << data.length());

  // Add some padding
  data.append(this->avPacketPaddingData);

  this->raw_pkt.setData(data);
  this->raw_pkt.setDTS(AV_NOPTS_VALUE);
  this->raw_pkt.setPTS(AV_NOPTS_VALUE);

  return this->pushAVPacket(this->raw_pkt);
}

stats::StatisticsTypes decoderFFmpeg::getStatisticsTypes() const
{
  auto sourceColorMapper =
      stats::color::ColorMapper({-2, 2}, stats::color::PredefinedType::Col3_bblg);

  return {stats::StatisticsType(0, "Source -", sourceColorMapper),
          stats::StatisticsType(1, "Source +", sourceColorMapper),
          stats::StatisticsType(2, "Motion Vector -", 4),
          stats::StatisticsType(3, "Motion Vector +", 4)};
}

bool decoderFFmpeg::pushAVPacket(FFmpeg::AVPacketWrapper &pkt)
{
  if (this->decoderState != DecoderState::NeedsMoreData)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Wrong decoder state.");
    return false;
  }
  if (!pkt)
  {
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Received empty packet. Swithing to flushing.");
    this->flushing     = true;
    this->decoderState = DecoderState::RetrieveFrames;
    return false;
  }
  if (this->flushing)
  {
    DEBUG_FFMPEG(
        "decoderFFmpeg::pushAVPacket: Error no new packets should be pushed in flushing mode.");
    return false;
  }

  // Push the packet to the decoder
  int retPush = this->ff.pushPacketToDecoder(decCtx, pkt);

  if (retPush < 0 && retPush != AVERROR(EAGAIN))
  {
#if DECODERFFMPEG_DEBUG_OUTPUT
    {
      QString meaning =
          QString("decoderFFmpeg::pushAVPacket: Error sending packet - err %1").arg(retPush);
      if (retPush == -1094995529)
        meaning += " INDA";
      // Log the first bytes
      meaning += " B(";
      int      nrBytes = std::min(pkt.getDataSize(), 5);
      uint8_t *data    = pkt.getData();
      for (int i = 0; i < nrBytes; i++)
      {
        uint8_t b = data[i];
        meaning += QString(" %1").arg(b, 2, 16);
      }
      meaning += ")";
      qDebug() << meaning;
    }
#endif
    this->setError(QStringLiteral("Error sending packet (avcodec_send_packet)"));
    return false;
  }
  else
    DEBUG_FFMPEG("decoderFFmpeg::pushAVPacket: Send packet PTS " << pkt.getPTS() << " duration "
                                                                 << pkt.getDuration() << " flags "
                                                                 << pkt.getFlags());

  if (retPush == AVERROR(EAGAIN))
  {
    // Enough data pushed. Decode and retrieve frames now.
    DEBUG_FFMPEG(
        "decoderFFmpeg::pushAVPacket: Enough data pushed. Decode and retrieve frames now.");
    this->decoderState = DecoderState::RetrieveFrames;
    return false;
  }

  return true;
}

bool decoderFFmpeg::decodeFrame()
{
  // Try to retrive a next frame from the decoder (don't copy it yet).
  auto retRecieve = this->ff.getFrameFromDecoder(decCtx, this->frame);
  if (retRecieve == 0)
  {
    // We recieved a frame.
    DEBUG_FFMPEG("decoderFFmpeg::decodeFrame: Received frame Size("
                 << this->frame.getWidth() << "x" << this->frame.getHeight() << ") PTS"
                 << this->frame.getPTS() << "type" << this->frame.getPictType()
                 << (this->frame.getKeyFrame() ? "key frame" : ""));
    // Checkt the size of the retrieved image
    if (this->frameSize != this->frame.getSize())
      return this->setErrorB("Received a frame of different size");
    this->currentOutputBuffer.clear();
    return true;
  }
  else if (retRecieve < 0 && retRecieve != AVERROR(EAGAIN) && retRecieve != -35)
  {
    // An error occurred
    DEBUG_FFMPEG("decoderFFmpeg::decodeFrame Error reading frame.");
    return this->setErrorB(QStringLiteral("Error recieving frame (avcodec_receive_frame)"));
  }
  else if (retRecieve == AVERROR(EAGAIN))
  {
    if (this->flushing)
    {
      // There is no more data
      this->decoderState = DecoderState::EndOfBitstream;
      DEBUG_FFMPEG("decoderFFmpeg::decodeFrame End of bitstream.");
    }
    else
    {
      this->decoderState = DecoderState::NeedsMoreData;
      DEBUG_FFMPEG("decoderFFmpeg::decodeFrame Need more data.");
    }
  }
  return false;
}

bool decoderFFmpeg::createDecoder(FFmpeg::AVCodecIDWrapper         codecID,
                                  FFmpeg::AVCodecParametersWrapper codecpar)
{
  // Allocate the decoder context
  if (this->videoCodec)
    return this->setErrorB(QStringLiteral("Video codec already allocated."));
  this->videoCodec = this->ff.findDecoder(codecID);
  if (!this->videoCodec)
    return this->setErrorB(QStringLiteral("Could not find a video decoder for the given codec ") +
                           codecID.getCodecName());

  if (this->decCtx)
    return this->setErrorB(QStringLiteral("Decoder context already allocated."));
  this->decCtx = this->ff.allocDecoder(this->videoCodec);
  if (!this->decCtx)
    return this->setErrorB(
        QStringLiteral("Could not allocate video decoder (avcodec_alloc_context3)"));

  if (codecpar && !this->ff.configureDecoder(decCtx, codecpar))
    return this->setErrorB(QStringLiteral("Unable to configure decoder from codecpar"));

  // Get some parameters from the decoder context
  this->frameSize = Size(decCtx.getWidth(), decCtx.getHeight());

  auto ffmpegPixFormat = this->ff.getAvPixFmtDescriptionFromAvPixelFormat(decCtx.getPixelFormat());
  this->rawFormat      = ffmpegPixFormat.getRawFormat();
  if (this->rawFormat == video::RawFormat::YUV)
    this->formatYUV = ffmpegPixFormat.getPixelFormatYUV();
  else if (this->rawFormat == video::RawFormat::RGB)
    this->formatRGB = ffmpegPixFormat.getRGBPixelFormat();

  // Ask the decoder to provide motion vectors (if possible)
  FFmpeg::AVDictionaryWrapper opts;
  int                         ret = this->ff.dictSet(opts, "flags2", "+export_mvs", 0);
  if (ret < 0)
    return this->setErrorB(
        QStringLiteral("Could not request motion vector retrieval. Return code %1").arg(ret));

  // Open codec
  ret = this->ff.avcodecOpen2(decCtx, videoCodec, opts);
  if (ret < 0)
    return this->setErrorB(
        QStringLiteral("Could not open the video codec (avcodec_open2). Return code %1.").arg(ret));

  this->frame = ff.allocateFrame();
  if (!this->frame)
    return this->setErrorB(QStringLiteral("Could not allocate frame (av_frame_alloc)."));

  return true;
}

} // namespace decoder
