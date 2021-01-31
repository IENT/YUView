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

#include "FileSourceFFmpegFile.h"

#include <QSettings>
#include <QProgressDialog>

#include "parser/common/SubByteReaderLogging.h"

#define FILESOURCEFFMPEGFILE_DEBUG_OUTPUT 0
#if FILESOURCEFFMPEGFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

using namespace YUView;
using namespace YUV_Internals;
using namespace parser::reader;

const ByteVector STARTCODE({0, 0, 1});

FileSourceFFmpegFile::FileSourceFFmpegFile()
{
  this->connect(&this->fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileSourceFFmpegFile::fileSystemWatcherFileChanged);
}

AVPacketWrapper FileSourceFFmpegFile::getNextPacket(bool getLastPackage, bool videoPacket)
{
  if (getLastPackage)
    return pkt;

  // Load the next packet
  if (!goToNextPacket(videoPacket))
  {
    return AVPacketWrapper();
  }

  return pkt;
}

ByteVector FileSourceFFmpegFile::getNextUnit(bool getLastDataAgain, uint64_t *pts)
{
  if (getLastDataAgain)
    return this->lastReturnArray;

  // Is a packet loaded?
  if (this->currentPacketData.empty())
  {
    if (!goToNextPacket(true))
      return {};

    this->currentPacketData = ByteVector(pkt.getData(), pkt.getData() + pkt.getDataSize());
    this->posInData = this->currentPacketData.begin();
  }
  
  // AVPacket data can be in one of two formats:
  // 1: The raw annexB format with start codes (0x00000001 or 0x000001)
  // 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed by the payload
  if (packetDataFormat == packetFormatRawNAL)
  {
    auto distance = std::distance(this->posInData, this->currentPacketData.end());
    if (distance < 5)
      return {};

    int offset {};
    {
      auto byte0 = *(this->posInData);
      auto byte1 = *(this->posInData + 1);
      auto byte2 = *(this->posInData + 2);
      auto byte3 = *(this->posInData + 3);
      
      if (byte0 == 0 && byte1 == 0 && byte2 == 0 && byte3 == 1)
        offset = 4;
      else if (byte0 == 0 && byte1 == 0 && byte2 == 1)
        offset = 3;
      else
      {
        this->currentPacketData.clear();
        return {};
      }
    }

    // Look for the next start code (or the end of the file)
    auto nextStartCodePos = std::search(this->posInData + 3, this->currentPacketData.end(), STARTCODE.begin(), STARTCODE.end());

    if (nextStartCodePos == this->currentPacketData.end())
    {
      // Return the remainder of the buffer and clear it so that the next packet is loaded on the next call
      this->lastReturnArray = ByteVector(this->posInData, this->currentPacketData.end());
      this->currentPacketData.clear();
      this->posInData = this->currentPacketData.begin();
    }
    else
    {
      this->lastReturnArray = ByteVector(this->posInData, nextStartCodePos);
      this->posInData = nextStartCodePos;
    }
  }
  else if (packetDataFormat == packetFormatMP4)
  {
    auto it = this->currentPacketData.begin();
    size_t size = 0;
    size += (*it) << 24;
    size += (*(it + 1)) << 16;
    size += (*(it + 2)) << 8;
    size += *(it + 3);

    auto remainingPacketSize = std::distance(this->posInData, this->currentPacketData.end());
    assert(remainingPacketSize >= 0);
    if (size + 4 > size_t(remainingPacketSize))
    {
      // The size indicates something bigger then the remaining data in the packet.
      this->currentPacketData.clear();
      this->posInData = this->currentPacketData.begin();
      return {};
    }
    
    this->lastReturnArray = ByteVector(this->posInData, this->posInData + size);
    this->posInData = this->posInData + 4 + size;
  }
  else if (packetDataFormat == packetFormatOBU)
  {
    auto posInDataUnsigned = size_t(std::distance(this->currentPacketData.begin(), this->posInData));
    SubByteReaderLogging reader(this->currentPacketData, nullptr, "", posInDataUnsigned);

    try
    {
      reader.readFlag("obu_forbidden_bit", Options().withCheckEqualTo(0));
      reader.readBits("obu_type", 4);
      auto obu_extension_flag = reader.readFlag("obu_extension_flag");
      auto obu_has_size_field = reader.readFlag("obu_has_size_field");
      reader.readFlag("obu_reserved_1bit", Options().withCheckEqualTo(1));

      if (obu_extension_flag)
      {
        reader.readBits("temporal_id", 3);
        reader.readBits("spatial_id", 2);
        reader.readBits("extension_header_reserved_3bits", 3, Options().withCheckEqualTo(0));
      }
      if (obu_has_size_field)
      {
        auto obu_size = reader.readLEB128("obu_size");
        auto completeSize = obu_size + reader.nrBytesRead();
        
        auto remainingPacketSize = std::distance(this->posInData, this->currentPacketData.end());
        if (completeSize >= remainingPacketSize)
        {
          this->lastReturnArray = ByteVector(this->posInData, this->currentPacketData.end());
          this->currentPacketData.clear();
          this->posInData = this->currentPacketData.begin();
        }
        else
        {
          this->lastReturnArray = ByteVector(this->posInData, this->posInData + completeSize);
          this->posInData += completeSize;
        }
      }
      else
      {
        // The OBU is the remainder of the input
        this->lastReturnArray = ByteVector(this->posInData, this->currentPacketData.end());
        this->currentPacketData.clear();
        this->posInData = this->currentPacketData.begin();
      }
    }
    catch(...)
    {
      // The reader threw an exception
      currentPacketData.clear();
      return {};
    }
  }

  if (pts)
    *pts = pkt.getPTS();

  return lastReturnArray;
}

ByteVector FileSourceFFmpegFile::getExtradata()
{
  // Get the video stream
  if (!video_stream)
    return {};
  AVCodecContextWrapper codec = video_stream.getCodec();
  if (!codec)
    return {};
  return codec.getExtradata();
}

QStringPairList FileSourceFFmpegFile::getMetadata()
{
  if (!fmt_ctx)
    return QStringPairList();
  return ff.getDictionaryEntries(fmt_ctx.getMetadata(), "", 0);
}

QList<ByteVector> FileSourceFFmpegFile::getParameterSets()
{
  if (!isFileOpened)
    return {};

  /* The SPS/PPS are somewhere else in containers:
   * In mp4-container (mkv also) PPS/SPS are stored separate from frame data in global headers. 
   * To access them from libav* APIs you need to look for extradata field in AVCodecContext of AVStream 
   * which relate to needed video stream. Also extradata can have different format from standard H.264 
   * NALs so look in MP4-container specs for format description. */
  auto extradata = this->getExtradata();
  auto it = extradata.begin();
  QList<ByteVector> retArray;

  // Since the FFmpeg developers don't want to make it too easy, the extradata is organized differently depending on the codec.
  AVCodecIDWrapper codecID = ff.getCodecIDWrapper(video_stream.getCodecID());
  if (codecID.isHEVC())
  {
    if (*it == 1)
    {
      // Internally, ffmpeg uses a custom format for the parameter sets (hvcC).
      // The hvcC parameters come first, and afterwards, the "normal" parameter sets are sent.

      // The first 22 bytes are fixed hvcC parameter set (see hvcc_write in libavformat hevc.c)
      it += 22;
      auto numOfArrays = unsigned(*it++);

      for (unsigned i = 0; i < numOfArrays; i++)
      {
        // The first byte contains the NAL unit type (which we don't use here).
        it++;
        //int byte = (unsigned char)(extradata.at(pos++));
        //bool array_completeness = byte & (1 << 7);
        //int nalUnitType = byte & 0x3f;

        // Two bytes numNalus
        unsigned numNalus = (unsigned(*it++)) << 7;
        numNalus += unsigned(*it++);

        for (unsigned j = 0; j < numNalus; j++)
        {
          // Two bytes nalUnitLength
          unsigned nalUnitLength = (unsigned(*it++)) << 7;
          nalUnitLength += unsigned(*it++);

          // nalUnitLength bytes payload of the NAL unit
          // This payload includes the NAL unit header
          if (nalUnitLength > std::distance(it, extradata.end()))
            // Not enough data in the buffer
            return {};

          auto newExtradata = ByteVector(it, it + nalUnitLength);
          retArray.append(newExtradata);
          it += nalUnitLength;
        }
      }
    }
  }
  else if (codecID.isAVC())
  {
    // Note: Actually we would only need this if we would feed the AVC bitstream to a different decoder then ffmpeg.
    //       So this function is so far not called (and not tested).

    // First byte is 1, length must be at least 7 bytes
    if (extradata.at(0) == 1 && extradata.size() >= 7)
    {
      it += 5;
      unsigned nrSPS = unsigned(*it++) & 0x1f;
      for (unsigned i = 0; i < nrSPS; i++)
      {
        unsigned nalUnitLength = (unsigned(*it++)) << 7;
        nalUnitLength += unsigned(*it++);

        if (nalUnitLength > std::distance(it, extradata.end()))
          // Not enough data in the buffer
          return {};

        auto rawNAL = ByteVector(it, it + nalUnitLength);
        retArray.append(rawNAL);
        it += nalUnitLength;
      }

      int nrPPS = unsigned(*it++) & 0x1f;
      for (int i = 0; i < nrPPS; i++)
      {
        unsigned nalUnitLength = (unsigned(*it++)) << 7;
        nalUnitLength += unsigned(*it++);

        if (nalUnitLength > std::distance(it, extradata.end()))
          // Not enough data in the buffer
          return {};

        auto rawNAL = ByteVector(it, it + nalUnitLength);
        retArray.append(rawNAL);
        it += nalUnitLength;
      }
    }
  }

  return retArray;
}

FileSourceFFmpegFile::~FileSourceFFmpegFile()
{
  if (pkt)
    pkt.freePacket(ff);
}

bool FileSourceFFmpegFile::openFile(const QString &filePath, QWidget *mainWindow, FileSourceFFmpegFile *other, bool parseFile)
{
  // Check if the file exists
  fileInfo.setFile(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (isFileOpened)
  {
    // Close the file?
    // TODO
  }

  openFileAndFindVideoStream(filePath);
  if (!isFileOpened)
    return false;

  // Save the full file path
  fullFilePath = filePath;

  // Install a watcher for the file (if file watching is active)
  updateFileWatchSetting();
  fileChanged = false;

  // If another (already opened) bitstream is given, copy bitstream info from there; Otherwise scan the bitstream.
  if (other && other->isFileOpened)
  {
    nrFrames = other->nrFrames;
    keyFrameList = other->keyFrameList;
  }
  else if (parseFile)
  {
    if (!scanBitstream(mainWindow))
      return false;
    
    seekFileToBeginning();
  }

  return true;
}

// Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
void FileSourceFFmpegFile::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

std::pair<int64_t, size_t> FileSourceFFmpegFile::getClosestSeekableFrameBefore(int frameIdx) const
{
  // We are always be able to seek to the beginning of the file
  auto bestSeekDTS = this->keyFrameList[0].dts;
  auto seekToFrameIdx = this->keyFrameList[0].frame;

  for (const auto &pic : this->keyFrameList)
  {
    if (pic.frame >= 0) 
    {
      if (pic.frame <= frameIdx)
      {
        // We could seek here
        bestSeekDTS = pic.dts;
        seekToFrameIdx = pic.frame;
      }
      else
        break;
    }
  }

  return {bestSeekDTS, seekToFrameIdx};
}

bool FileSourceFFmpegFile::scanBitstream(QWidget *mainWindow)
{
  if (!isFileOpened)
    return false;

  // Create the dialog (if the given pointer is not null)
  int64_t maxPTS = getMaxTS();
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QScopedPointer<QProgressDialog> progress;
  if (mainWindow != nullptr)
  {
    progress.reset(new QProgressDialog("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow));
    progress->setMinimumDuration(1000);  // Show after 1s
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setWindowModality(Qt::WindowModal);
  }

  nrFrames = 0;
  while (goToNextPacket(true))
  {
    DEBUG_FFMPEG("FileSourceFFmpegFile::scanBitstream: frame %d pts %d dts %d%s", nrFrames, (int)pkt.getPTS(), (int)pkt.getDTS(), pkt.getFlagKeyframe() ? " - keyframe" : "");

    if (pkt.getFlagKeyframe())
      keyFrameList.append(pictureIdx(nrFrames, pkt.getDTS()));

    if (progress && progress->wasCanceled())
      return false;

    int newPercentValue = 0;
    if (maxPTS != 0)
      newPercentValue = clip(int(pkt.getPTS() * 100 / maxPTS), 0, 100);
    if (newPercentValue != curPercentValue)
    {
      if (progress)
        progress->setValue(newPercentValue);
      curPercentValue = newPercentValue;
    }

    nrFrames++;
  }

  DEBUG_FFMPEG("FileSourceFFmpegFile::scanBitstream: Scan done. Found %d frames and %d keyframes.", nrFrames, keyFrameList.length());
  return !progress->wasCanceled();
}

void FileSourceFFmpegFile::openFileAndFindVideoStream(QString fileName)
{
  isFileOpened = false;

  if (!ff.loadFFmpegLibraries())
    return;

  // Open the input file
  if (!ff.openInput(fmt_ctx, fileName))
    return;
  
  // What is the input format?
  AVInputFormatWrapper inp_format = fmt_ctx.getInputFormat();

  // Iterate through all streams
  for (unsigned int idx=0; idx < fmt_ctx.getNbStreams(); idx++)
  {
    AVStreamWrapper stream = fmt_ctx.getStream(idx);
    AVMediaType streamType =  stream.getCodecType();
    AVCodecIDWrapper codeID = ff.getCodecIDWrapper(stream.getCodecID());
    if (streamType == AVMEDIA_TYPE_VIDEO)
    {
      video_stream = stream;
      streamIndices.video = idx;
    }
    else if (streamType == AVMEDIA_TYPE_AUDIO)
      streamIndices.audio.append(idx);
    else if (streamType == AVMEDIA_TYPE_SUBTITLE)
    {
      if (codeID.getCodecName() == "dvb_subtitle")
        streamIndices.subtitle.dvb.append(idx);
      else if (codeID.getCodecName() == "eia_608")
        streamIndices.subtitle.eia608.append(idx);
      else
        streamIndices.subtitle.other.append(idx);
    }
  }
  if(!video_stream)
    return;

  // Initialize an empty packet
  pkt.allocatePaket(ff);

  // Get the frame rate, picture size and color conversion mode
  AVRational avgFrameRate = video_stream.getAvgFrameRate();
  if (avgFrameRate.den == 0)
    frameRate = -1;
  else
    frameRate = avgFrameRate.num / double(avgFrameRate.den);

  AVPixFmtDescriptorWrapper ffmpegPixFormat = ff.getAvPixFmtDescriptionFromAvPixelFormat(video_stream.getCodec().getPixelFormat());
  rawFormat = ffmpegPixFormat.getRawFormat();
  if (rawFormat == raw_YUV)
    pixelFormat_yuv = ffmpegPixFormat.getYUVPixelFormat();
  else if (rawFormat == raw_RGB)
    pixelFormat_rgb = ffmpegPixFormat.getRGBPixelFormat();
  
  duration = fmt_ctx.getDuration();
  timeBase = video_stream.getTimeBase();

  AVColorSpace colSpace = video_stream.getColorspace();
  int w = video_stream.getFrameWidth();
  int h = video_stream.getFrameHeight();
  frameSize.setWidth(w);
  frameSize.setHeight(h);

  if (colSpace == AVCOL_SPC_BT2020_NCL || colSpace == AVCOL_SPC_BT2020_CL)
    colorConversionType = ColorConversion::BT2020_LimitedRange;
  else if (colSpace == AVCOL_SPC_BT470BG || colSpace == AVCOL_SPC_SMPTE170M)
    colorConversionType = ColorConversion::BT601_LimitedRange;
  else
    colorConversionType = ColorConversion::BT709_LimitedRange;

  isFileOpened = true;
}

bool FileSourceFFmpegFile::goToNextPacket(bool videoPacketsOnly)
{
  //Load the next video stream packet into the packet buffer
  int ret = 0;
  do
  {
    if (pkt)
      // Unref the packet
      pkt.unrefPacket(ff);
  
    ret = fmt_ctx.readFrame(ff, pkt);

    if (pkt.getStreamIndex() == streamIndices.video)
      pkt.setPacketType(PacketType::VIDEO);
    else if (streamIndices.audio.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::AUDIO);
    else if (streamIndices.subtitle.dvb.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::SUBTITLE_DVB);
    else if (streamIndices.subtitle.eia608.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::SUBTITLE_608);
    else
      pkt.setPacketType(PacketType::OTHER);
  }
  while (ret == 0 && videoPacketsOnly && pkt.getPacketType() != PacketType::VIDEO);
  
  if (ret < 0)
  {
    endOfFile = true;
    return false;
  }

  DEBUG_FFMPEG("FileSourceFFmpegFile::goToNextPacket: Return: stream %d pts %d dts %d%s", (int)pkt.getStreamIndex(), (int)pkt.getPTS(), (int)pkt.getDTS(), pkt.getFlagKeyframe() ? " - keyframe" : "");

  if (packetDataFormat == packetFormatUnknown)
    // This is the first video package that we find and we don't know what the format of the packet data is.
    // Guess the format from the data in the first package
    // This format should not change from packet to packet
    packetDataFormat = pkt.guessDataFormatFromData();

  return true;
}

bool FileSourceFFmpegFile::seekToDTS(int64_t dts)
{
  if (!isFileOpened)
    return false;

  int ret = ff.seekFrame(fmt_ctx, video_stream.getIndex(), dts);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegLibraries::seekToDTS Error DTS %ld. Return Code %d", dts, ret);
    return false;
  }

  // We seeked somewhere, so we are not at the end of the file anymore.
  endOfFile = false;

  DEBUG_FFMPEG("FFmpegLibraries::seekToDTS Successfully seeked to DTS %d", (int)dts);
  return true;
}

bool FileSourceFFmpegFile::seekFileToBeginning()
{
  if (!isFileOpened)
    return false;

  int ret = ff.seekBeginning(fmt_ctx);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegLibraries::seekToBeginning Error. Return Code %d", ret);
    return false;
  }

  // We seeked somewhere, so we are not at the end of the file anymore.
  endOfFile = false;

  DEBUG_FFMPEG("FFmpegLibraries::seekToBeginning Successfull.");
  return true;
}

int64_t FileSourceFFmpegFile::getMaxTS() 
{ 
  if (!isFileOpened)
    return -1; 
  
  // duration / AV_TIME_BASE is the duration in seconds
  // pts * timeBase is also in seconds
  return duration / AV_TIME_BASE * timeBase.den / timeBase.num;
}

indexRange FileSourceFFmpegFile::getDecodableFrameLimits() const
{
  if (this->keyFrameList.isEmpty() || nrFrames == 0)
    return {};

  indexRange range;
  range.first = int(this->keyFrameList.at(0).frame);
  range.second = int(nrFrames);
  return range;
}

QList<QStringPairList> FileSourceFFmpegFile::getFileInfoForAllStreams()
{
  QList<QStringPairList> info;

  info += fmt_ctx.getInfoText();
  for(unsigned int i=0; i<fmt_ctx.getNbStreams(); i++)
  {
    AVStreamWrapper s = fmt_ctx.getStream(i);
    AVCodecIDWrapper codecIdWrapper = ff.getCodecIDWrapper(s.getCodecID());
    info += s.getInfoText(codecIdWrapper);
  }

  return info;
}

QList<AVRational> FileSourceFFmpegFile::getTimeBaseAllStreams()
{
  QList<AVRational> timeBaseList;

  for (unsigned int i = 0; i < fmt_ctx.getNbStreams(); i++)
  {
    AVStreamWrapper s = fmt_ctx.getStream(i);
    timeBaseList.append(s.getTimeBase());
  }

  return timeBaseList;
}

QList<QString> FileSourceFFmpegFile::getShortStreamDescriptionAllStreams()
{
  QList<QString> descriptions;

  for (unsigned int i = 0; i < fmt_ctx.getNbStreams(); i++)
  {
    QString description;
    AVStreamWrapper s = fmt_ctx.getStream(i);
    description = s.getCodecTypeName();
    
    AVCodecIDWrapper codecID = ff.getCodecIDWrapper(s.getCodecID());
    description += " " + codecID.getCodecName();

    description += QString(" (%1x%2)").arg(s.getFrameWidth()).arg(s.getFrameHeight());

    descriptions.append(description);
  }

  return descriptions;
}
