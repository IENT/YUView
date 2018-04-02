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

#ifndef FILESOURCEFFMPEGFILE_H
#define FILESOURCEFFMPEGFILE_H

#include "fileSource.h"
#include "FFmpegLibraries.h"

typedef enum
{
  FFMPEG_CODEC_HEVC,
  FFMPEG_CODEC_AVC,
  FFMPEG_CODEC_OTHER
} ffmpeg_codec;

/* This class is a normal fileSource for opening of raw AnnexBFiles.
* Basically it understands that this is a binary file where each unit starts with a start code (0x0000001)
* TODO: The reading / parsing could be performed in a background thread in order to increase the performance
*/
class fileSourceFFMpegFile : public QObject
{
  Q_OBJECT

public:
  fileSourceFFMpegFile();
  fileSourceFFMpegFile(const QString &filePath) : fileSourceFFMpegFile() { openFile(filePath); }
  ~fileSourceFFMpegFile();

  // Load the ffmpeg libraries and try to open the file. The fileSource will install a watcher for the file.
  // Return false if anything goes wrong.
  bool openFile(const QString &filePath);

  // Is the file at the end?
  // TODO: How do we do this?
  bool atEnd() const { return ffmpegLib.atEnd(); }

  // Get the next NAL unit (everything excluding the start code)
  QByteArray getNextNALUnit(quint64 &pts);

  // File watching
  void updateFileWatchSetting();
  bool isFileChanged() { bool b = fileChanged; fileChanged = false; return b; }

  bool seekToPTS(qint64 pts) { if (!isFileOpened) return false; return ffmpegLib.seekToPTS(pts); }
  qint64 getMaxPTS() { if (!isFileOpened) return -1; return ffmpegLib.getMaxPTS(); };

  int getNumberFrames() const { return nrFrames; }
  ffmpeg_codec getCodec();
  
private slots:
  void fileSystemWatcherFileChanged(const QString &path) { Q_UNUSED(path); fileChanged = true; }

protected:

  FFmpegLibraries ffmpegLib;

  // Watch the opened file for modifications
  QFileSystemWatcher fileWatcher;
  bool fileChanged;

  QString   fullFilePath;
  QFileInfo fileInfo;
  bool      isFileOpened;

  // In order to translate from frames to PTS, we need to count the frames and keep a list of
  // the PTS values of keyframes that we can start decoding at.
  void scanBitstream();
  int nrFrames;

  // Private struct for navigation. We index frames by frame number and FFMpeg uses the pts.
  // This connects both values.
  struct pictureIdx
  {
    pictureIdx(qint64 frame, qint64 pts) : frame(frame), pts(pts) {}
    qint64 frame;
    qint64 pts;
  };

  // These are filled after opening a file (after scanBitstream was called)
  QList<pictureIdx> keyFrameList;  //< A list of pairs (frameNr, PTS) that we can seek to.
  //pictureIdx getClosestSeekableFrameNumberBefore(int frameIdx);

  // For parsing NAL units from the compressed data:
  QByteArray currentPacketData;
  int posInData;

};

#endif // FILESOURCEFFMPEGFILE_H
