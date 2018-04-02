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

#include "fileSourceFFMpegFile.h"

#include <QSettings>

#define FILESOURCEFFMPEGFILE_DEBUG_OUTPUT 1
#if FILESOURCEFFMPEGFILE_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

fileSourceFFMpegFile::fileSourceFFMpegFile()
{
  fileChanged = false;
  isFileOpened = false;
  nrFrames = 0;

  connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &fileSourceFFMpegFile::fileSystemWatcherFileChanged);
}

QByteArray fileSourceFFMpegFile::getNextNALUnit(quint64 & posInFile)
{
  // Is a packet loaded?
  if (currentPacketData.isEmpty())
  {
    if (!ffmpegLib.goToNextVideoPacket())
    {
      posInFile = -1;
      return QByteArray();
    }

    currentPacketData = QByteArray::fromRawData((const char*)(ffmpegLib.getPacketData()), ffmpegLib.getPacketDataSize());
    posInData = 0;
  }
  
  // FFMpeg packet use the following encoding:
  // The first 4 bytes determine the size of the NAL unit followed by the payload
  QByteArray sizePart = currentPacketData.mid(posInData, 4);
  unsigned int size = (unsigned char)sizePart.at(3);
  size += (unsigned char)sizePart.at(2) << 8;
  size += (unsigned char)sizePart.at(1) << 16;
  size += (unsigned char)sizePart.at(0) << 24;
  
  QByteArray retArray = currentPacketData.mid(posInData + 4, size);
  posInData += 4 + size;
  if (posInData >= currentPacketData.size())
    currentPacketData.clear();
  return retArray;
}

fileSourceFFMpegFile::~fileSourceFFMpegFile()
{
}

bool fileSourceFFMpegFile::openFile(const QString &filePath)
{
  // Check if the file exists
  fileInfo.setFile(filePath);
  if (!fileInfo.exists() || !fileInfo.isFile())
    return false;

  if (isFileOpened)
  {
    ffmpegLib.closeFile();
  }

  isFileOpened = ffmpegLib.openFile(filePath);
  if (!isFileOpened)
    return false;

  // Save the full file path
  fullFilePath = filePath;

  // Install a watcher for the file (if file watching is active)
  updateFileWatchSetting();
  fileChanged = false;

  scanBitstream();

  return true;
}

// Check if we are supposed to watch the file for changes. If no, remove the file watcher. If yes, install one.
void fileSourceFFMpegFile::updateFileWatchSetting()
{
  // Install a file watcher if file watching is active in the settings.
  // The addPath/removePath functions will do nothing if called twice for the same file.
  QSettings settings;
  if (settings.value("WatchFiles",true).toBool())
    fileWatcher.addPath(fullFilePath);
  else
    fileWatcher.removePath(fullFilePath);
}

void fileSourceFFMpegFile::scanBitstream()
{
  nrFrames = 0;
  while (ffmpegLib.goToNextVideoPacket())
  {
    qint64 pts = ffmpegLib.getPacketPTS();
    qint64 dts = ffmpegLib.getPacketDTS();
    bool isKeyframe = ffmpegLib.getPacketIsKeyframe();

    DEBUG_FFMPEG("frame %d pts %d dts %d%s", nrFrames, pts, dts, isKeyframe ? " - keyframe" : "");

    if (isKeyframe)
      keyFrameList.append(pictureIdx(nrFrames, pts));

    nrFrames++;
  }
}

ffmpeg_codec fileSourceFFMpegFile::getCodec()
{
  AVCodecID codec = ffmpegLib.getCodecID();
  if (codec == AV_CODEC_ID_H264)
    return FFMPEG_CODEC_AVC;
  if (codec == AV_CODEC_ID_HEVC)
    return FFMPEG_CODEC_HEVC;
  return FFMPEG_CODEC_OTHER;
}