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

#include "fileSourceFFmpegFile.h"

#include <QSettings>
#include <QProgressDialog>

#include "parserCommon.h"

#define FILESOURCEFFMPEGFILE_DEBUG_OUTPUT 0
#if FILESOURCEFFMPEGFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

using namespace YUV_Internals;

fileSourceFFmpegFile::fileSourceFFmpegFile()
{
  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &fileSourceFFmpegFile::fileSystemWatcherFileChanged);
}

AVPacketWrapper fileSourceFFmpegFile::getNextPacket(bool getLastPackage, bool videoPacket)
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

QByteArray fileSourceFFmpegFile::getNextUnit(bool getLastDataAgain, uint64_t *pts)
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

    currentPacketData = QByteArray::fromRawData((const char*)(pkt.get_data()), pkt.get_data_size());
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
    parserCommon::sub_byte_reader reader(currentPacketData, posInData);

    try
    {
      QString bitsRead;
      bool obu_forbidden_bit = (reader.readBits(1, bitsRead) != 0);
      reader.readBits(4, bitsRead); // obu_type
      bool obu_extension_flag = (reader.readBits(1, bitsRead) != 0);
      bool obu_has_size_field = (reader.readBits(1, bitsRead) != 0);
      bool obu_reserved_1bit = (reader.readBits(1, bitsRead) != 0);

      if (obu_forbidden_bit || obu_reserved_1bit)
      {
        currentPacketData.clear();
        return QByteArray();
      }
      if (obu_extension_flag)
      {
        reader.readBits(3, bitsRead); // temporal_id
        reader.readBits(2, bitsRead); // spatial_id
        unsigned int extension_header_reserved_3bits = reader.readBits(3, bitsRead);
        if (extension_header_reserved_3bits != 0)
        {
          currentPacketData.clear();
          return QByteArray();
        }
      }
      if (obu_has_size_field)
      {
        int bitCount;
        unsigned int obu_size = reader.readLeb128(bitsRead, bitCount);
        unsigned int completeSize = obu_size + reader.nrBytesRead();
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
    *pts = pkt.get_pts();

  return lastReturnArray;
}

QByteArray fileSourceFFmpegFile::getExtradata()
{
  // Get the video stream
  if (!video_stream)
    return QByteArray();
  AVCodecContextWrapper codec = video_stream.getCodec();
  if (!codec)
    return QByteArray();
  return codec.get_extradata();
}

QStringPairList fileSourceFFmpegFile::getMetadata()
{
  if (!fmt_ctx)
    return QStringPairList();
  return ff.get_dictionary_entries(fmt_ctx.get_metadata(), "", 0);
}

QList<QByteArray> fileSourceFFmpegFile::getParameterSets()
{
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

fileSourceFFmpegFile::~fileSourceFFmpegFile()
{
  if (pkt)
    pkt.free_packet();
}

bool fileSourceFFmpegFile::openFile(const QString &filePath, QWidget *mainWindow, fileSourceFFmpegFile *other, bool parseFile)
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
    
    // Seek back to the beginning
    seekToDTS(0);
  }

  return true;
}

// Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
void fileSourceFFmpegFile::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

int fileSourceFFmpegFile::getClosestSeekableDTSBefore(int frameIdx, int &seekToFrameIdx) const
{
  // We are always be able to seek to the beginning of the file
  int bestSeekDTS = keyFrameList[0].dts;
  seekToFrameIdx = keyFrameList[0].frame;

  for (pictureIdx idx : keyFrameList)
  {
    if (idx.frame >= 0) 
    {
      if (idx.frame <= frameIdx)
      {
        // We could seek here
        bestSeekDTS = idx.dts;
        seekToFrameIdx = idx.frame;
      }
      else
        break;
    }
  }

  return bestSeekDTS;
}

bool fileSourceFFmpegFile::scanBitstream(QWidget *mainWindow)
{
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
    DEBUG_FFMPEG("fileSourceFFmpegFile::scanBitstream: frame %d pts %d dts %d%s", nrFrames, (int)pkt.get_pts(), (int)pkt.get_dts(), pkt.get_flag_keyframe() ? " - keyframe" : "");

    if (pkt.get_flag_keyframe())
      keyFrameList.append(pictureIdx(nrFrames, pkt.get_dts()));

    if (progress && progress->wasCanceled())
      return false;

    int newPercentValue = pkt.get_pts() * 100 / maxPTS;
    if (newPercentValue != curPercentValue)
    {
      if (progress)
        progress->setValue(newPercentValue);
      curPercentValue = newPercentValue;
    }

    nrFrames++;
  }

  DEBUG_FFMPEG("fileSourceFFmpegFile::scanBitstream: Scan done. Found %d frames and %d keyframes.", nrFrames, keyFrameList.length());
  return !progress->wasCanceled();
}

void fileSourceFFmpegFile::openFileAndFindVideoStream(QString fileName)
{
  isFileOpened = false;

  if (!ff.loadFFmpegLibraries())
    return;

  // Open the input file
  if (!ff.open_input(fmt_ctx, fileName))
    return;
  
  // What is the input format?
  AVInputFormatWrapper inp_format = fmt_ctx.get_input_format();

  // Get the first video stream
  for(unsigned int idx=0; idx < fmt_ctx.get_nb_streams(); idx++)
  {
    AVStreamWrapper stream = fmt_ctx.get_stream(idx);
    AVMediaType streamType =  stream.getCodecType();
    if(streamType == AVMEDIA_TYPE_VIDEO)
    {
      video_stream = stream;
      break;
    }
  }
  if(!video_stream)
    return;

  // Initialize an empty packet
  pkt.allocate_paket(ff);

  // Get the frame rate, picture size and color conversion mode
  AVRational avgFrameRate = video_stream.get_avg_frame_rate();
  if (avgFrameRate.den == 0)
    frameRate = -1;
  else
    frameRate = avgFrameRate.num / double(avgFrameRate.den);

  AVPixFmtDescriptorWrapper ffmpegPixFormat = ff.getAvPixFmtDescriptionFromAvPixelFormat(video_stream.getCodec().get_pixel_format());
  rawFormat = ffmpegPixFormat.getRawFormat();
  if (rawFormat == raw_YUV)
    pixelFormat_yuv = ffmpegPixFormat.getYUVPixelFormat();
  else if (rawFormat == raw_RGB)
    pixelFormat_rgb = ffmpegPixFormat.getRGBPixelFormat();
  
  duration = fmt_ctx.get_duration();
  timeBase = video_stream.get_time_base();

  AVColorSpace colSpace = video_stream.get_colorspace();
  int w = video_stream.get_frame_width();
  int h = video_stream.get_frame_height();
  frameSize.setWidth(w);
  frameSize.setHeight(h);

  if (colSpace == AVCOL_SPC_BT2020_NCL || colSpace == AVCOL_SPC_BT2020_CL)
    colorConversionType = BT2020_LimitedRange;
  else if (colSpace == AVCOL_SPC_BT470BG || colSpace == AVCOL_SPC_SMPTE170M)
    colorConversionType = BT601_LimitedRange;
  else
    colorConversionType = BT709_LimitedRange;

  isFileOpened = true;
}

bool fileSourceFFmpegFile::goToNextPacket(bool videoPacketsOnly)
{
  //Load the next video stream packet into the packet buffer
  int ret = 0;
  do
  {
    if (pkt)
      // Unref the packet
      pkt.unref_packet(ff);
  
    ret = fmt_ctx.read_frame(ff, pkt);
    pkt.is_video_packet = (pkt.get_stream_index() == video_stream.get_index());
  }
  while (ret == 0 && videoPacketsOnly && !pkt.is_video_packet);
  
  if (ret < 0)
  {
    endOfFile = true;
    return false;
  }

  DEBUG_FFMPEG("fileSourceFFmpegFile::goToNextPacket: Return: stream %d pts %d dts %d%s", (int)pkt.get_stream_index(), (int)pkt.get_pts(), (int)pkt.get_dts(), pkt.get_flag_keyframe() ? " - keyframe" : "");

  if (packetDataFormat == packetFormatUnknown)
    // This is the first video package that we find and we don't know what the format of the packet data is.
    // Guess the format from the data in the first package
    // This format should not change from packet to packet
    packetDataFormat = pkt.guessDataFormatFromData();

  return true;
}

bool fileSourceFFmpegFile::seekToDTS(int64_t dts)
{
  if (!isFileOpened)
    return false;

  int ret = ff.seek_frame(fmt_ctx, video_stream.get_index(), dts);
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

int64_t fileSourceFFmpegFile::getMaxTS() 
{ 
  if (!isFileOpened)
    return -1; 
  
  // duration / AV_TIME_BASE is the duration in seconds
  // pts * timeBase is also in seconds
  return duration / AV_TIME_BASE * timeBase.den / timeBase.num;
}

QList<QStringPairList> fileSourceFFmpegFile::getFileInfoForAllStreams()
{
  QList<QStringPairList> info;

  info += fmt_ctx.getInfoText();
  for(unsigned int i=0; i<fmt_ctx.get_nb_streams(); i++)
  {
    AVStreamWrapper s = fmt_ctx.get_stream(i);
    info += s.getInfoText();
  }

  return info;
}