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

#include "playlistItemFFMPEGFile.h"

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QPainter>

#define FFMPEG_DEBUG_OUTPUT 0
#if FFMPEG_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_FFMPEG qDebug
#else
#define DEBUG_FFMPEG(fmt,...) ((void)0)
#endif

playlistItemFFMPEGFile::playlistItemFFMPEGFile(const QString &ffmpegFilePath)
  : playlistItemWithVideo(ffmpegFilePath, playlistItem_Indexed)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_videoHEVC.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Set the video pointer correctly
  video.reset(new videoHandlerYUV());

  // Connect the basic signals from the video
  playlistItemWithVideo::connectVideo();

  // Open the file
  decoderReady = decoder.openFile(ffmpegFilePath);

  // Set the frame number limits
  startEndFrame = getStartEndFrameLimits();

  // Load YUV data fro frame 0
  loadYUVData(0, false);

  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
  connect(yuvVideo, &videoHandlerYUV::signalRequestRawData, this, &playlistItemFFMPEGFile::loadYUVData, Qt::DirectConnection);
  connect(yuvVideo, &videoHandlerYUV::signalUpdateFrameLimits, this, &playlistItemFFMPEGFile::slotUpdateFrameLimits);
}

void playlistItemFFMPEGFile::getSupportedFileExtensions(QStringList &allExtensions, QStringList &filters)
{
  allExtensions.append("mkv");
  filters.append("FFMpeg File (*.mkv)");
}

void playlistItemFFMPEGFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the HEVC file. We save both in the playlist.
  QUrl fileURL(plItemNameOrFileName);
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(plItemNameOrFileName);

  QDomElementYUView d = root.ownerDocument().createElement("playlistItemHEVCFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the HEVC file (the path to the file. Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);

  root.appendChild(d);
}

infoData playlistItemFFMPEGFile::getInfo() const
{
  infoData info("FFMpeg File Info");

  // At first append the file information part (path, date created, file size...)
  info.items.append(decoder.getFileInfoList());

  if (decoder.errorInDecoder())
    info.items.append(infoItem("Error", decoder.decoderErrorString()));
  else
  {
    QSize videoSize = video->getFrameSize();
    info.items.append(infoItem("Resolution", QString("%1x%2").arg(videoSize.width()).arg(videoSize.height()), "The video resolution in pixel (width x height)"));
    info.items.append(infoItem("Num Frames", QString::number(decoder.getNumberPOCs()), "The number of pictures in the stream."));
  }

  return info;
}

void playlistItemFFMPEGFile::loadYUVData(int frameIdx, bool caching)
{
  if (caching && !cachingEnabled)
    return;

  if (decoder.errorInDecoder())
    // We can not decode images
    return;

  DEBUG_FFMPEG("playlistItemFFMPEGFile::loadYUVData %d %s", frameIdx, caching ? "caching" : "");

  videoHandlerYUV *yuvVideo = dynamic_cast<videoHandlerYUV*>(video.data());
  yuvVideo->setFrameSize(decoder.getFrameSize());
  yuvVideo->setYUVPixelFormat(decoder.getYUVPixelFormat());
  
  if (frameIdx > startEndFrame.second || frameIdx < 0)
  {
    DEBUG_FFMPEG("playlistItemHEVCFile::loadYUVData Invalid frame index");
    return;
  }

  // Just get the frame from the correct decoder
  QByteArray decByteArray = decoder.loadYUVFrameData(frameIdx);
  
  if (!decByteArray.isEmpty())
  {
    yuvVideo->rawYUVData = decByteArray;
    yuvVideo->rawYUVData_frameIdx = frameIdx;
  }
}