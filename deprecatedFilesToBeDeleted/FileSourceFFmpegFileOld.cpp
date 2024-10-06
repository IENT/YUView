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

#include <QProgressDialog>
#include <QSettings>
#include <fstream>

#include <common/Formatting.h>
#include <ffmpeg/AVCodecContextWrapper.h>
#include <parser/AV1/obu_header.h>
#include <parser/common/SubByteReaderLogging.h>

#define FILESOURCEFFMPEGFILE_DEBUG_OUTPUT 0
#if FILESOURCEFFMPEGFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt, ...) ((void)0)
#endif

using SubByteReaderLogging = parser::reader::SubByteReaderLogging;
using namespace FFmpeg;

namespace
{

auto startCode = QByteArrayLiteral("\x00\x00\x01");

uint64_t getBoxSize(ByteVector::const_iterator iterator)
{
  uint64_t size = 0;
  size += static_cast<uint64_t>(*(iterator++)) << (8 * 3);
  size += static_cast<uint64_t>(*(iterator++)) << (8 * 2);
  size += static_cast<uint64_t>(*(iterator++)) << (8 * 1);
  size += static_cast<uint64_t>(*iterator);
  return size;
}

} // namespace

FileSourceFFmpegFile::FileSourceFFmpegFile()
{
  connect(&this->fileWatcher,
          &QFileSystemWatcher::fileChanged,
          this,
          &FileSourceFFmpegFile::fileSystemWatcherFileChanged);
}

bool FileSourceFFmpegFile::atEnd() const
{
  return this->endOfFile;
}

QStringList FileSourceFFmpegFile::getPathsOfLoadedLibraries() const
{
  return this->ff.getLibPaths();
}

double FileSourceFFmpegFile::getFramerate() const
{
  return this->frameRate;
}

Size FileSourceFFmpegFile::getSequenceSizeSamples() const
{
  return this->frameSize;
}

video::RawFormat FileSourceFFmpegFile::getRawFormat() const
{
  return this->rawFormat;
}

video::yuv::PixelFormatYUV FileSourceFFmpegFile::getPixelFormatYUV() const
{
  return this->pixelFormat_yuv;
}

video::rgb::PixelFormatRGB FileSourceFFmpegFile::getPixelFormatRGB() const
{
  return this->pixelFormat_rgb;
}

AVPacketWrapper FileSourceFFmpegFile::getNextPacket(bool getLastPackage, bool videoPacket)
{
  if (getLastPackage)
    return this->currentPacket;

  // Load the next packet
  if (!this->goToNextPacket(videoPacket))
  {
    this->posInFile = -1;
    return {};
  }

  return this->currentPacket;
}

QByteArray FileSourceFFmpegFile::getNextUnit(bool getLastDataAgain)
{
  if (getLastDataAgain)
    return this->lastReturnArray;

  // Is a packet loaded?
  if (this->currentPacketData.isEmpty())
  {
    if (!this->goToNextPacket(true))
    {
      this->posInFile = -1;
      return {};
    }

    this->currentPacketData = QByteArray::fromRawData((const char *)(this->currentPacket.getData()),
                                                      this->currentPacket.getDataSize());
    this->posInData         = 0;
  }

  // AVPacket data can be in one of two formats:
  // 1: The raw annexB format with start codes (0x00000001 or 0x000001)
  // 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed
  // by the payload
  if (this->packetDataFormat == PacketDataFormat::RawNAL)
  {
    const auto firstBytes = this->currentPacketData.mid(posInData, 4);
    int        offset;
    if (firstBytes.at(0) == (char)0 && firstBytes.at(1) == (char)0 && firstBytes.at(2) == (char)0 &&
        firstBytes.at(3) == (char)1)
      offset = 4;
    else if (firstBytes.at(0) == (char)0 && firstBytes.at(1) == (char)0 &&
             firstBytes.at(2) == (char)1)
      offset = 3;
    else
    {
      // The start code could not be found ...
      this->currentPacketData.clear();
      return {};
    }

    // Look for the next start code (or the end of the file)
    auto nextStartCodePos = this->currentPacketData.indexOf(startCode, posInData + 3);

    if (nextStartCodePos == -1)
    {
      // Return the remainder of the buffer and clear it so that the next packet is loaded on the
      // next call
      this->lastReturnArray = currentPacketData.mid(this->posInData + offset);
      this->currentPacketData.clear();
    }
    else
    {
      auto size       = nextStartCodePos - this->posInData - offset;
      lastReturnArray = this->currentPacketData.mid(this->posInData + offset, size);
      this->posInData += 3 + size;
    }
  }
  else if (this->packetDataFormat == PacketDataFormat::MP4)
  {
    auto sizePart = this->currentPacketData.mid(posInData, 4);
    int  size     = (unsigned char)sizePart.at(3);
    size += (unsigned char)sizePart.at(2) << 8;
    size += (unsigned char)sizePart.at(1) << 16;
    size += (unsigned char)sizePart.at(0) << 24;

    if (size < 0)
    {
      // The int did overflow. This means that the NAL unit is > 2GB in size. This is probably an
      // error
      this->currentPacketData.clear();
      return {};
    }
    if (size > this->currentPacketData.length() - this->posInData)
    {
      // The indicated size is bigger than the buffer
      this->currentPacketData.clear();
      return {};
    }

    this->lastReturnArray = this->currentPacketData.mid(posInData + 4, size);
    this->posInData += 4 + size;
    if (this->posInData >= this->currentPacketData.size())
      this->currentPacketData.clear();
  }
  else if (this->packetDataFormat == PacketDataFormat::OBU)
  {
    SubByteReaderLogging reader(
        SubByteReaderLogging::convertToByteVector(currentPacketData), nullptr, "", posInData);

    try
    {
      parser::av1::obu_header header;
      header.parse(reader);

      if (header.obu_has_size_field)
      {
        auto completeSize     = header.obu_size + reader.nrBytesRead();
        this->lastReturnArray = currentPacketData.mid(posInData, completeSize);
        this->posInData += completeSize;
        if (this->posInData >= currentPacketData.size())
          this->currentPacketData.clear();
      }
      else
      {
        // The OBU is the remainder of the input
        this->lastReturnArray = currentPacketData.mid(posInData);
        this->posInData       = currentPacketData.size();
        this->currentPacketData.clear();
      }
    }
    catch (...)
    {
      // The reader threw an exception
      this->currentPacketData.clear();
      return {};
    }
  }

  return this->lastReturnArray;
}

ByteVector FileSourceFFmpegFile::getExtradata()
{
  // Get the video stream
  if (!this->video_stream)
    return {};
  return this->video_stream.getExtradata();
}

StringPairVec FileSourceFFmpegFile::getMetadata()
{
  if (!this->formatCtx)
    return {};
  return ff.getDictionaryEntries(this->formatCtx.getMetadata(), "", 0);
}

ByteVector FileSourceFFmpegFile::getLhvCData()
{
  const auto inputFormat = this->formatCtx.getInputFormat();
  const auto isMp4       = inputFormat.getName().contains("mp4");
  if (!this->getVideoStreamCodecID().isHEVC() || !isMp4)
    return {};

  // This is a bit of a hack. The problem is that FFmpeg can currently not extract this for us.
  // Maybe this will be added in the future. So the only option we have here is to manually extract
  // the lhvC data from the mp4 file. In mp4, the boxes can be at the beginning or at the end of the
  // file.
  enum class SearchPosition
  {
    Beginning,
    End
  };

  std::ifstream inputFile(this->fileName.toStdString(), std::ios::binary);
  for (const auto searchPosition : {SearchPosition::Beginning, SearchPosition::End})
  {
    constexpr auto NR_SEARCH_BYTES = 5120;

    if (searchPosition == SearchPosition::End)
      inputFile.seekg(-NR_SEARCH_BYTES, std::ios_base::end);

    const auto rawFileData = functions::readData(inputFile, NR_SEARCH_BYTES);
    if (rawFileData.empty())
      continue;

    const std::string searchString = "lhvC";
    auto              lhvcPos      = std::search(
        rawFileData.begin(), rawFileData.end(), searchString.begin(), searchString.end());
    if (lhvcPos == rawFileData.end())
      continue;

    if (std::distance(rawFileData.begin(), lhvcPos) < 4)
      continue;

    const auto boxSize = getBoxSize(lhvcPos - 4);
    if (boxSize == 0 || boxSize > std::distance(lhvcPos, rawFileData.end()))
      continue;

    // We just return the payload without the box size or the "lhvC" tag
    return ByteVector(lhvcPos + 4, lhvcPos + boxSize - 4);
  }

  return {};
}

QList<QByteArray> FileSourceFFmpegFile::getParameterSets()
{
  if (!this->isFileOpened)
    return {};

  /* The SPS/PPS are somewhere else in containers:
   * In mp4-container (mkv also) PPS/SPS are stored separate from frame data in global headers.
   * To access them from libav* APIs you need to look for extradata field in AVCodecContext of
   * AVStream which relate to needed video stream. Also extradata can have different format from
   * standard H.264 NALs so look in MP4-container specs for format description. */
  const auto extradata = this->getExtradata();
  if (extradata.isEmpty())
  {
    DEBUG_FFMPEG("Error no extradata could be found.");
    return {};
  }

  QList<QByteArray> retArray;

  // Since the FFmpeg developers don't want to make it too easy, the extradata is organized
  // differently depending on the codec.
  auto codecID = this->ff.getCodecIDWrapper(video_stream.getCodecID());
  if (codecID.isHEVC())
  {
    if (extradata.at(0) == 1)
    {
      // Internally, ffmpeg uses a custom format for the parameter sets (hvcC).
      // The hvcC parameters come first, and afterwards, the "normal" parameter sets are sent.

      // The first 22 bytes are fixed hvcC parameter set (see hvcc_write in libavformat hevc.c)
      auto numOfArrays = int(extradata.at(22));

      int pos = 23;
      for (int i = 0; i < numOfArrays; i++)
      {
        // The first byte contains the NAL unit type (which we don't use here).
        pos++;
        // int byte = (unsigned char)(extradata.at(pos++));
        // bool array_completeness = byte & (1 << 7);
        // int nalUnitType = byte & 0x3f;

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
          auto rawNAL = extradata.mid(pos, nalUnitLength);
          retArray.append(rawNAL);
          pos += nalUnitLength;
        }
      }
    }
  }
  else if (codecID.isAVC())
  {
    // Note: Actually we would only need this if we would feed the AVC bitstream to a different
    // decoder then ffmpeg.
    //       So this function is so far not called (and not tested).

    // First byte is 1, length must be at least 7 bytes
    if (extradata.at(0) == 1 && extradata.length() >= 7)
    {
      int nrSPS = extradata.at(5) & 0x1f;
      int pos   = 6;
      for (int i = 0; i < nrSPS; i++)
      {
        int nalUnitLength = (unsigned char)(extradata.at(pos++)) << 7;
        nalUnitLength += (unsigned char)(extradata.at(pos++));

        auto rawNAL = extradata.mid(pos, nalUnitLength);
        retArray.append(rawNAL);
        pos += nalUnitLength;
      }

      int nrPPS = extradata.at(pos++);
      for (int i = 0; i < nrPPS; i++)
      {
        int nalUnitLength = (unsigned char)(extradata.at(pos++)) << 7;
        nalUnitLength += (unsigned char)(extradata.at(pos++));

        auto rawNAL = extradata.mid(pos, nalUnitLength);
        retArray.append(rawNAL);
        pos += nalUnitLength;
      }
    }
  }
  else if (codecID.isAV1())
  {
    // This should be a normal OBU for the seuqence header starting with the OBU header
    SubByteReaderLogging    reader(SubByteReaderLogging::convertToByteVector(extradata), nullptr);
    parser::av1::obu_header header;
    try
    {
      header.parse(reader);
    }
    catch (const std::exception &e)
    {
      (void)e;
      DEBUG_FFMPEG("Error parsing OBU header " + e.what());
      return retArray;
    }

    if (header.obu_type == parser::av1::ObuType::OBU_SEQUENCE_HEADER)
      retArray.append(extradata);
  }

  return retArray;
}

FileSourceFFmpegFile::~FileSourceFFmpegFile()
{
  if (this->currentPacket)
    this->ff.freePacket(this->currentPacket);
}

bool FileSourceFFmpegFile::openFile(const QString &       filePath,
                                    QWidget *             mainWindow,
                                    FileSourceFFmpegFile *other,
                                    bool                  parseFile)
{
  // Check if the file exists
  this->fileInfo.setFile(filePath);
  if (!this->fileInfo.exists() || !this->fileInfo.isFile())
    return false;

  if (this->isFileOpened)
  {
    // Close the file?
    // TODO
  }

  this->openFileAndFindVideoStream(filePath);
  if (!this->isFileOpened)
    return false;

  // Save the full file path
  this->fullFilePath = filePath;

  // Install a watcher for the file (if file watching is active)
  this->updateFileWatchSetting();
  this->fileChanged = false;

  // If another (already opened) bitstream is given, copy bitstream info from there; Otherwise scan
  // the bitstream.
  if (other && other->isFileOpened)
  {
    this->nrFrames     = other->nrFrames;
    this->keyFrameList = other->keyFrameList;
  }
  else if (parseFile)
  {
    if (!this->scanBitstream(mainWindow))
      return false;

    this->seekFileToBeginning();
  }

  return true;
}

// Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes,
// install one.
void FileSourceFFmpegFile::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles", true).toBool())
    this->fileWatcher.addPath(this->fullFilePath);
  else
    this->fileWatcher.removePath(this->fullFilePath);
}

bool FileSourceFFmpegFile::isFileChanged() const
{
  bool b            = this->fileChanged;
  this->fileChanged = false;
  return b;
}

std::pair<int64_t, size_t> FileSourceFFmpegFile::getClosestSeekableFrameBefore(int frameIdx) const
{
  // We are always be able to seek to the beginning of the file
  auto bestSeekDTS    = this->keyFrameList[0].dts;
  auto seekToFrameIdx = this->keyFrameList[0].frame;

  for (const auto &pic : this->keyFrameList)
  {
    if (frameIdx > 0 && pic.frame <= unsigned(frameIdx))
    {
      // We could seek here
      bestSeekDTS    = pic.dts;
      seekToFrameIdx = pic.frame;
    }
    else
      break;
  }

  return {bestSeekDTS, seekToFrameIdx};
}

bool FileSourceFFmpegFile::scanBitstream(QWidget *mainWindow)
{
  if (!this->isFileOpened)
    return false;

  // Create the dialog (if the given pointer is not null)
  auto maxPTS = this->getMaxTS();
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int                              curPercentValue = 0;
  std::unique_ptr<QProgressDialog> progress;
  if (mainWindow != nullptr)
  {
    progress.reset(
        new QProgressDialog("Parsing (indexing) bitstream...", "Cancel", 0, 100, mainWindow));
    progress->setMinimumDuration(1000); // Show after 1s
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setWindowModality(Qt::WindowModal);
  }

  this->nrFrames = 0;
  while (this->goToNextPacket(true))
  {
    DEBUG_FFMPEG("FileSourceFFmpegFile::scanBitstream: frame %d pts %d dts %d%s",
                 this->nrFrames,
                 (int)this->currentPacket.getPTS(),
                 (int)this->currentPacket.getDTS(),
                 this->currentPacket.getFlagKeyframe() ? " - keyframe" : "");

    if (this->currentPacket.getFlagKeyframe())
      this->keyFrameList.append(pictureIdx(this->nrFrames, this->currentPacket.getDTS()));

    if (progress && progress->wasCanceled())
      return false;

    int newPercentValue = 0;
    if (maxPTS != 0)
      newPercentValue = functions::clip(int(this->currentPacket.getPTS() * 100 / maxPTS), 0, 100);
    if (newPercentValue != curPercentValue)
    {
      if (progress)
        progress->setValue(newPercentValue);
      curPercentValue = newPercentValue;
    }

    this->nrFrames++;
  }

  DEBUG_FFMPEG("FileSourceFFmpegFile::scanBitstream: Scan done. Found %d frames and %d keyframes.",
               this->nrFrames,
               this->keyFrameList.length());
  return !progress->wasCanceled();
}

void FileSourceFFmpegFile::openFileAndFindVideoStream(QString fileName)
{
  this->isFileOpened = false;

  this->ff.loadFFmpegLibraries();
  if (!this->ff.loadingSuccessfull())
    return;

  // Open the input file
  if (!this->ff.openInput(this->formatCtx, fileName))
    return;

  this->fileName = fileName;
  this->formatCtx.getInputFormat();

  for (unsigned idx = 0; idx < this->formatCtx.getNbStreams(); idx++)
  {
    auto stream     = this->formatCtx.getStream(idx);
    auto streamType = stream.getCodecType();
    auto codeID     = this->ff.getCodecIDWrapper(stream.getCodecID());
    if (streamType == AVMEDIA_TYPE_VIDEO)
    {
      this->video_stream        = stream;
      this->streamIndices.video = idx;
    }
    else if (streamType == AVMEDIA_TYPE_AUDIO)
      this->streamIndices.audio.append(idx);
    else if (streamType == AVMEDIA_TYPE_SUBTITLE)
    {
      if (codeID.getCodecName() == "dvb_subtitle")
        this->streamIndices.subtitle.dvb.append(idx);
      else if (codeID.getCodecName() == "eia_608")
        this->streamIndices.subtitle.eia608.append(idx);
      else
        this->streamIndices.subtitle.other.append(idx);
    }
  }
  if (!this->video_stream)
    return;

  this->currentPacket = this->ff.allocatePacket();

  // Get the frame rate, picture size and color conversion mode
  auto avgFrameRate = this->video_stream.getAvgFrameRate();
  if (avgFrameRate.den == 0)
    this->frameRate = -1.0;
  else
    this->frameRate = avgFrameRate.num / double(avgFrameRate.den);

  const auto ffmpegPixFormat =
      this->ff.getAvPixFmtDescriptionFromAvPixelFormat(this->video_stream.getPixelFormat());
  this->rawFormat = ffmpegPixFormat.getRawFormat();
  if (this->rawFormat == video::RawFormat::YUV)
    this->pixelFormat_yuv = ffmpegPixFormat.getPixelFormatYUV();
  else if (this->rawFormat == video::RawFormat::RGB)
    this->pixelFormat_rgb = ffmpegPixFormat.getRGBPixelFormat();

  this->duration = this->formatCtx.getDuration();
  this->timeBase = this->video_stream.getTimeBase();

  auto colSpace   = this->video_stream.getColorspace();
  this->frameSize = this->video_stream.getFrameSize();

  if (colSpace == AVCOL_SPC_BT2020_NCL || colSpace == AVCOL_SPC_BT2020_CL)
    this->colorConversionType = video::yuv::ColorConversion::BT2020_LimitedRange;
  else if (colSpace == AVCOL_SPC_BT470BG || colSpace == AVCOL_SPC_SMPTE170M)
    this->colorConversionType = video::yuv::ColorConversion::BT601_LimitedRange;
  else
    this->colorConversionType = video::yuv::ColorConversion::BT709_LimitedRange;

  this->isFileOpened = true;
}

bool FileSourceFFmpegFile::goToNextPacket(bool videoPacketsOnly)
{
  // Load the next video stream packet into the packet buffer
  int ret = 0;
  do
  {
    auto &pkt = this->currentPacket;

    if (this->currentPacket)
      this->ff.unrefPacket(this->currentPacket);

    {
      auto ctx = this->formatCtx.getFormatCtx();
      if (!ctx || !pkt)
        ret = -1;
      else
        ret = ff.lib.avformat.av_read_frame(ctx, pkt.getPacket());
    }

    if (pkt.getStreamIndex() == this->streamIndices.video)
      pkt.setPacketType(PacketType::VIDEO);
    else if (this->streamIndices.audio.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::AUDIO);
    else if (this->streamIndices.subtitle.dvb.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::SUBTITLE_DVB);
    else if (this->streamIndices.subtitle.eia608.contains(pkt.getStreamIndex()))
      pkt.setPacketType(PacketType::SUBTITLE_608);
    else
      pkt.setPacketType(PacketType::OTHER);
  } while (ret == 0 && videoPacketsOnly &&
           this->currentPacket.getPacketType() != PacketType::VIDEO);

  if (ret < 0)
  {
    this->endOfFile = true;
    return false;
  }

  DEBUG_FFMPEG("FileSourceFFmpegFile::goToNextPacket: Return: stream %d pts %d dts %d%s",
               (int)this->currentPacket.getStreamIndex(),
               (int)this->currentPacket.getPTS(),
               (int)this->currentPacket.getDTS(),
               this->currentPacket.getFlagKeyframe() ? " - keyframe" : "");

  if (this->currentPacket.getPacketType() == PacketType::VIDEO &&
      this->packetDataFormat == PacketDataFormat::Unknown)
    // This is the first video package that we find and we don't know what the format of the packet
    // data is. Guess the format from the data in the first package This format should not change
    // from packet to packet
    this->packetDataFormat = this->currentPacket.guessDataFormatFromData();

  return true;
}

bool FileSourceFFmpegFile::seekToDTS(int64_t dts)
{
  if (!this->isFileOpened)
    return false;

  int ret = this->ff.seekFrame(this->formatCtx, this->video_stream.getIndex(), dts);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegLibraries::seekToDTS Error DTS %ld. Return Code %d", dts, ret);
    return false;
  }

  // We seeked somewhere, so we are not at the end of the file anymore.
  this->endOfFile = false;

  DEBUG_FFMPEG("FFmpegLibraries::seekToDTS Successfully seeked to DTS %d", (int)dts);
  return true;
}

bool FileSourceFFmpegFile::seekFileToBeginning()
{
  if (!this->isFileOpened)
    return false;

  int ret = this->ff.seekBeginning(this->formatCtx);
  if (ret != 0)
  {
    DEBUG_FFMPEG("FFmpegLibraries::seekToBeginning Error. Return Code %d", ret);
    return false;
  }

  // We seeked somewhere, so we are not at the end of the file anymore.
  this->endOfFile = false;

  DEBUG_FFMPEG("FFmpegLibraries::seekToBeginning Successfull.");
  return true;
}

int64_t FileSourceFFmpegFile::getMaxTS() const
{
  if (!this->isFileOpened)
    return -1;

  // duration / AV_TIME_BASE is the duration in seconds
  // pts * timeBase is also in seconds
  return this->duration / AV_TIME_BASE * this->timeBase.den / this->timeBase.num;
}

indexRange FileSourceFFmpegFile::getDecodableFrameLimits() const
{
  if (this->keyFrameList.isEmpty() || this->nrFrames == 0)
    return {};

  indexRange range;
  range.first  = int(this->keyFrameList.at(0).frame);
  range.second = int(this->nrFrames);
  return range;
}

FFmpeg::AVCodecIDWrapper FileSourceFFmpegFile::getVideoStreamCodecID()
{
  return this->ff.getCodecIDWrapper(video_stream.getCodecID());
}

QList<QStringPairList> FileSourceFFmpegFile::getFileInfoForAllStreams()
{
  QList<QStringPairList> info;

  info += this->formatCtx.getInfoText();
  for (unsigned i = 0; i < this->formatCtx.getNbStreams(); i++)
  {
    auto stream         = this->formatCtx.getStream(i);
    auto codecIdWrapper = this->ff.getCodecIDWrapper(stream.getCodecID());
    info += stream.getInfoText(codecIdWrapper);
  }

  return info;
}

QList<AVRational> FileSourceFFmpegFile::getTimeBaseAllStreams()
{
  QList<AVRational> timeBaseList;

  for (unsigned i = 0; i < this->formatCtx.getNbStreams(); i++)
  {
    auto stream = this->formatCtx.getStream(i);
    timeBaseList.append(stream.getTimeBase());
  }

  return timeBaseList;
}

StringVec FileSourceFFmpegFile::getShortStreamDescriptionAllStreams()
{
  StringVec descriptions;

  for (unsigned i = 0; i < this->formatCtx.getNbStreams(); i++)
  {
    std::ostringstream description;
    auto               stream = this->formatCtx.getStream(i);
    description << stream.getCodecTypeName().toStdString();

    auto codecID = this->ff.getCodecIDWrapper(stream.getCodecID());
    description << " " << codecID.getCodecName().toStdString() << " ";

    description << std::pair{stream.getFrameSize().width, stream.getFrameSize().height};

    descriptions.push_back(description.str());
  }

  return descriptions;
}
