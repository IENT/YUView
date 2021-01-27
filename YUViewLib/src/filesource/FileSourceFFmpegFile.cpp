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

FileSourceFFmpegFile::FileSourceFFmpegFile()
{
  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &FileSourceFFmpegFile::fileSystemWatcherFileChanged);
}

AVPacketWrapper FileSourceFFmpegFile::getNextPacket(bool getLastPackage, bool videoPacket)
{
  if (getLastPackage)
    return pkt;

  // Load the next packet
  if (!goToNextPacket(videoPacket))
  {
    posInFile = -1;
    return AVPacketWrapper();
  }

  return pkt;
}

QByteArray FileSourceFFmpegFile::getNextUnit(bool getLastDataAgain, uint64_t *pts)
{
  if (getLastDataAgain)
    return lastReturnArray;

  // Is a packet loaded?
  if (currentPacketData.isEmpty())
  {
    if (!goToNextPacket(true))
    {
      posInFile = -1;
      return QByteArray();
    }

    currentPacketData = QByteArray::fromRawData((const char*)(pkt.getData()), pkt.getDataSize());
    posInData = 0;
  }
  
  // AVPacket data can be in one of two formats:
  // 1: The raw annexB format with start codes (0x00000001 or 0x000001)
  // 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed by the payload
  if (packetDataFormat == packetFormatRawNAL)
  {
    QByteArray firstBytes = currentPacketData.mid(posInData, 4);
    int offset;
    if (firstBytes.at(0) == (char)0 && firstBytes.at(1) == (char)0 && firstBytes.at(2) == (char)0 && firstBytes.at(3) == (char)1)
      offset = 4;
    else if (firstBytes.at(0) == (char)0 && firstBytes.at(1) == (char)0 && firstBytes.at(2) == (char)1)
      offset = 3;
    else
    {
      // The start code could not be found ...
      currentPacketData.clear();
      return QByteArray();
    }
    
    // Look for the next start code (or the end of the file)
    int nextStartCodePos = currentPacketData.indexOf(startCode, posInData + 3);

    if (nextStartCodePos == -1)
    {
      // Return the remainder of the buffer and clear it so that the next packet is loaded on the next call
      lastReturnArray = currentPacketData.mid(posInData + offset);
      currentPacketData.clear();
    }
    else
    {
      int size = nextStartCodePos - posInData - offset;
      lastReturnArray = currentPacketData.mid(posInData + offset, size);
      posInData += 3 + size;
    }
  }
  else if (packetDataFormat == packetFormatMP4)
  {
    QByteArray sizePart = currentPacketData.mid(posInData, 4);
    int size = (unsigned char)sizePart.at(3);
    size += (unsigned char)sizePart.at(2) << 8;
    size += (unsigned char)sizePart.at(1) << 16;
    size += (unsigned char)sizePart.at(0) << 24;

    if (size < 0)
    {
      // The int did overflow. This means that the NAL unit is > 2GB in size. This is probably an error
      currentPacketData.clear();
      return QByteArray();
    }
    if (size > currentPacketData.length() - posInData)
    {
      // The indicated size is bigger than the buffer
      currentPacketData.clear();
      return QByteArray();
    }
    
    lastReturnArray = currentPacketData.mid(posInData + 4, size);
    posInData += 4 + size;
    if (posInData >= currentPacketData.size())
      currentPacketData.clear();
  }
  else if (packetDataFormat == packetFormatOBU)
  {
    SubByteReaderLogging reader(SubByteReaderLogging::convertToByteVector(currentPacketData), nullptr, "", posInData);

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
        lastReturnArray = currentPacketData.mid(posInData, completeSize);
        posInData += completeSize;
        if (posInData >= currentPacketData.size())
          currentPacketData.clear();
      }
      else
      {
        // The OBU is the remainder of the input
        lastReturnArray = currentPacketData.mid(posInData);
        posInData = currentPacketData.size();
        currentPacketData.clear();
      }
    }
    catch(...)
    {
      // The reader threw an exception
      currentPacketData.clear();
      return QByteArray();
    }
  }

  if (pts)
    *pts = pkt.getPTS();

  return lastReturnArray;
}

QByteArray FileSourceFFmpegFile::getExtradata()
{
  // Get the video stream
  if (!video_stream)
    return QByteArray();
  AVCodecContextWrapper codec = video_stream.getCodec();
  if (!codec)
    return QByteArray();
  return codec.getExtradata();
}

QStringPairList FileSourceFFmpegFile::getMetadata()
{
  if (!fmt_ctx)
    return QStringPairList();
  return ff.getDictionaryEntries(fmt_ctx.getMetadata(), "", 0);
}

QList<QByteArray> FileSourceFFmpegFile::getParameterSets()
{
  if (!isFileOpened)
    return {};

  /* The SPS/PPS are somewhere else in containers:
   * In mp4-container (mkv also) PPS/SPS are stored separate from frame data in global headers. 
   * To access them from libav* APIs you need to look for extradata field in AVCodecContext of AVStream 
   * which relate to needed video stream. Also extradata can have different format from standard H.264 
   * NALs so look in MP4-container specs for format description. */
  QByteArray extradata = getExtradata();
  QList<QByteArray> retArray;

  // Since the FFmpeg developers don't want to make it too easy, the extradata is organized differently depending on the codec.
  AVCodecIDWrapper codecID = ff.getCodecIDWrapper(video_stream.getCodecID());
  if (codecID.isHEVC())
  {
    if (extradata.at(0) == 1)
    {
      // Internally, ffmpeg uses a custom format for the parameter sets (hvcC).
      // The hvcC parameters come first, and afterwards, the "normal" parameter sets are sent.

      // The first 22 bytes are fixed hvcC parameter set (see hvcc_write in libavformat hevc.c)
      int numOfArrays = extradata.at(22);

      int pos = 23;
      for (int i = 0; i < numOfArrays; i++)
      {
        // The first byte contains the NAL unit type (which we don't use here).
        pos++;
        //int byte = (unsigned char)(extradata.at(pos++));
        //bool array_completeness = byte & (1 << 7);
        //int nalUnitType = byte & 0x3f;

        // Two bytes numNalus
        int numNalus = (unsigned char)(extradata.at(pos++)) << 7;
        numNalus += (unsigned char)(extradata.at(pos++));

        for (int j = 0; j < numNalus; j++)
        {
          // Two bytes nalUnitLength
          int nalUnitLength = (unsigned char)(extradata.at(pos++)) << 7;
          nalUnitLength += (unsigned char)(extradata.at(pos++));

          // nalUnitLength bytes payload of the NAL unit
          // This payload includes the NAL unit header
          QByteArray rawNAL = extradata.mid(pos, nalUnitLength);
          retArray.append(rawNAL);
          pos += nalUnitLength;
        }
      }
    }
  }
  else if (codecID.isAVC())
  {
    // Note: Actually we would only need this if we would feed the AVC bitstream to a different decoder then ffmpeg.
    //       So this function is so far not called (and not tested).

    // First byte is 1, length must be at least 7 bytes
    if (extradata.at(0) == 1 && extradata.length() >= 7)
    {
      int nrSPS = extradata.at(5) & 0x1f;
      int pos = 6;
      for (int i = 0; i < nrSPS; i++)
      {
        int nalUnitLength = (unsigned char)(extradata.at(pos++)) << 7;
        nalUnitLength += (unsigned char)(extradata.at(pos++));

        QByteArray rawNAL = extradata.mid(pos, nalUnitLength);
        retArray.append(rawNAL);
        pos += nalUnitLength;
      }

      int nrPPS = extradata.at(pos++);
      for (int i = 0; i < nrPPS; i++)
      {
        int nalUnitLength = (unsigned char)(extradata.at(pos++)) << 7;
        nalUnitLength += (unsigned char)(extradata.at(pos++));

        QByteArray rawNAL = extradata.mid(pos, nalUnitLength);
        retArray.append(rawNAL);
        pos += nalUnitLength;
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
