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

#include "playlistItemStatisticsFile.h"

#include <cassert>
#include <iostream>
#include <QDebug>
#include <QTime>
#include <QUrl>
#include "statisticsExtensions.h"

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
#define STAT_PARSING_BUFFER_SIZE 1048576

playlistItemStatisticsFile::playlistItemStatisticsFile(const QString &itemNameOrFileName)
  : playlistItem(itemNameOrFileName, playlistItem_Indexed)
{
  // Set default variables
  fileSortedByPOC = false;
  blockOutsideOfFrame_idx = -1;
  backgroundParserProgress = 0.0;
  currentDrawnFrameIdx = -1;
  maxPOC = 0;
  isStatisticsLoading = false;

  // Set statistics icon
  setIcon(0, convertIcon(":img_stats.png"));

  file.openFile(itemNameOrFileName);
  if (!file.isOk())
    return;
}

playlistItemStatisticsFile::~playlistItemStatisticsFile()
{
  // The playlistItemStatisticsFile object is being deleted.
  // Check if the background thread is still running.
  if (backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    cancelBackgroundParser = true;
    backgroundParserFuture.waitForFinished();
  }
}

infoData playlistItemStatisticsFile::getInfo() const
{
  infoData info("Statistics File info");

  // Append the file information (path, date created, file size...)
  info.items.append(file.getFileInfoList());

  // Is the file sorted by POC?
  info.items.append(infoItem("Sorted by POC", fileSortedByPOC ? "Yes" : "No"));

  // Show the progress of the background parsing (if running)
  if (backgroundParserFuture.isRunning())
    info.items.append(infoItem("Parsing:", QString("%1%...").arg(backgroundParserProgress, 0, 'f', 2)));

  // Print a warning if one of the blocks in the statistics file is outside of the defined "frame size"
  if (blockOutsideOfFrame_idx != -1)
    info.items.append(infoItem("Warning", QString("A block in frame %1 is outside of the given size of the statistics.").arg(blockOutsideOfFrame_idx)));

  // Show any errors that occurred during parsing
  if (!parsingError.isEmpty())
    info.items.append(infoItem("Parsing Error:", parsingError));

  return info;
}

void playlistItemStatisticsFile::drawItem(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData)
{
  // drawRawData only controls the drawing of raw pixel values
  Q_UNUSED(drawRawData);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  // Tell the statSource to draw the statistics
  statSource.paintStatistics(painter, frameIdxInternal, zoomFactor);

  // Currently this frame is drawn.
  currentDrawnFrameIdx = frameIdxInternal;
}

// This timer event is called regularly when the background loading process is running.
// It will update
void playlistItemStatisticsFile::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != timer.timerId())
    return playlistItem::timerEvent(event);

  // Check if the background process is still running. If it is not, no signal are required anymore.
  // The final update signal was emitted by the background process.
  if (!backgroundParserFuture.isRunning())
    timer.stop();
  else
  {
    setStartEndFrame(indexRange(0, maxPOC), false);
    emit signalItemChanged(false, RECACHE_NONE);
  }
}

void playlistItemStatisticsFile::createPropertiesWidget()
{
  // Absolutely always only call this once//
  assert(!propertiesWidget);

  // Create a new widget and populate it with controls
  preparePropertiesWidget(getPlaylistTag());

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget.data());

  QFrame *line = new QFrame;
  line->setObjectName(QStringLiteral("lineOne"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(statSource.createStatisticsHandlerControls());

  // Do not add any stretchers at the bottom because the statistics handler controls will
  // expand to take up as much space as there is available
}

void playlistItemStatisticsFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the YUV file-> We save both in the playlist.
  QUrl fileURL(file.getAbsoluteFilePath());
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath(file.getAbsoluteFilePath());

  QDomElementYUView d = root.ownerDocument().createElement(getPlaylistTag());

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the YUV file (the path to the file-> Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);

  // Save the status of the statistics (which are shown, transparency ...)
  statSource.savePlaylist(d);

  root.appendChild(d);
}

void playlistItemStatisticsFile::loadFrame(int frameIdx, bool playback, bool loadRawdata, bool emitSignals)
{
  Q_UNUSED(playback);
  Q_UNUSED(loadRawdata);
  const int frameIdxInternal = getFrameIdxInternal(frameIdx);

  if (statSource.needsLoading(frameIdxInternal) == LoadingNeeded)
  {
    isStatisticsLoading = true;
    statSource.loadStatistics(frameIdxInternal);
    isStatisticsLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }
}
