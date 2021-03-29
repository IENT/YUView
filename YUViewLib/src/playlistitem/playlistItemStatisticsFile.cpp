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

#include <QDebug>
#include <QTime>
#include <QUrl>
#include <QtConcurrent>
#include <cassert>
#include <iostream>

#include "common/YUViewDomElement.h"
#include "common/functions.h"
#include "statistics/StatisticsFileCSV.h"
#include "statistics/StatisticsFileVTMBMS.h"

// The internal buffer for parsing the starting positions. The buffer must not be larger than 2GB
// so that we can address all the positions in it with int (using such a large buffer is not a good
// idea anyways)
#define STAT_PARSING_BUFFER_SIZE 1048576

playlistItemStatisticsFile::playlistItemStatisticsFile(const QString &itemNameOrFileName,
                                                       OpenMode       openMode)
    : playlistItem(itemNameOrFileName, Type::Indexed), openMode(openMode)
{
  // Set default variables
  currentDrawnFrameIdx = -1;
  isStatisticsLoading  = false;

  this->prop.isFileSource          = true;
  this->prop.propertiesWidgetTitle = "Statistics File Properties";
  this->prop.providesStatistics    = true;

  // Set statistics icon
  setIcon(0, functions::convertIcon(":img_stats.png"));

  this->openStatisticsFile();
  this->statisticsUIHandler.setStatisticsData(&this->statisticsData);

  connect(&this->statisticsUIHandler, &stats::StatisticUIHandler::updateItem, [this](bool redraw) {
    emit signalItemChanged(redraw, RECACHE_NONE);
  });
}

playlistItemStatisticsFile::~playlistItemStatisticsFile()
{
  if (this->backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    this->breakBackgroundAtomic.store(true);
    this->backgroundParserFuture.waitForFinished();
  }
}

infoData playlistItemStatisticsFile::getInfo() const
{
  if (this->file)
    return this->file->getInfo();
  
  infoData info("Statistics File info");
  info.items.append(infoItem("File", "No file loaded"));
  return info;
}

playlistItemStatisticsFile *playlistItemStatisticsFile::newplaylistItemStatisticsFile(
    const YUViewDomElement &root, const QString &playlistFilePath, OpenMode openMode)
{
  // Parse the DOM element. It should have all values of a playlistItemStatisticsFile
  auto absolutePath = root.findChildValue("absolutePath");
  auto relativePath = root.findChildValue("relativePath");

  // check if file with absolute path exists, otherwise check relative path
  auto filePath = FileSource::getAbsPathFromAbsAndRel(playlistFilePath, absolutePath, relativePath);
  if (filePath.isEmpty())
    return nullptr;

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  auto newStat = new playlistItemStatisticsFile(filePath, openMode);

  // Load the propertied of the playlistItem
  playlistItem::loadPropertiesFromPlaylist(root, newStat);

  // Load the status of the statistics (which are shown, transparency ...)
  newStat->statisticsData.loadPlaylist(root);

  return newStat;
}

void playlistItemStatisticsFile::reloadItemSource()
{
  this->currentDrawnFrameIdx = -1;

  this->statisticsData.clear();
  this->statisticsUIHandler.updateStatisticsHandlerControls();

  this->openStatisticsFile();
}

itemLoadingState playlistItemStatisticsFile::needsLoading(int frameIdx, bool loadRawdata)
{
  Q_UNUSED(loadRawdata);
  if (!this->file)
    return itemLoadingState::LoadingNotNeeded;

  return this->statisticsData.needsLoading(frameIdx);
}

void playlistItemStatisticsFile::drawItem(QPainter *painter,
                                          int       frameIdx,
                                          double    zoomFactor,
                                          bool      drawRawData)
{
  Q_UNUSED(drawRawData);
  this->statisticsData.paintStatistics(painter, frameIdx, zoomFactor);
  this->currentDrawnFrameIdx = frameIdx;
}

void playlistItemStatisticsFile::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  // Determine the relative path to the YUV file-> We save both in the playlist.
  auto absolutePath = QFileInfo(this->prop.name).absoluteFilePath();
  QUrl fileURL(absolutePath);
  fileURL.setScheme("file");
  auto relativePath = playlistDir.relativeFilePath(absolutePath);

  YUViewDomElement d = root.ownerDocument().createElement("playlistItemStatisticsFile");

  // Append the properties of the playlistItem
  playlistItem::appendPropertiesToPlaylist(d);

  // Append all the properties of the YUV file (the path to the file-> Relative and absolute)
  d.appendProperiteChild("absolutePath", fileURL.toString());
  d.appendProperiteChild("relativePath", relativePath);

  // Save the status of the statistics (which are shown, transparency ...)
  this->statisticsData.savePlaylist(d);

  root.appendChild(d);
}

void playlistItemStatisticsFile::loadFrame(int  frameIdx,
                                           bool playback,
                                           bool loadRawdata,
                                           bool emitSignals)
{
  Q_UNUSED(playback);
  Q_UNUSED(loadRawdata);

  if (this->statisticsData.needsLoading(frameIdx) == LoadingNeeded)
  {
    this->isStatisticsLoading = true;
    {
      auto typesToLoad = this->statisticsData.getTypesThatNeedLoading();
      for (auto typeID : typesToLoad)
        this->file->loadStatisticData(this->statisticsData, frameIdx, typeID);
    }
    this->isStatisticsLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }
}

ValuePairListSets playlistItemStatisticsFile::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  (void)frameIdx;
  return ValuePairListSets("Stats", this->statisticsData.getValuesAt(pixelPos));
}

bool playlistItemStatisticsFile::isSourceChanged()
{
  return this->file && this->file->isFileChanged();
}

void playlistItemStatisticsFile::updateSettings()
{
  this->statisticsUIHandler.updateSettings();
  if (this->file)
    this->file->updateSettings();
}

void playlistItemStatisticsFile::getSupportedFileExtensions(QStringList &allExtensions,
                                                            QStringList &filters)
{
  allExtensions.append("vtmbmsstats");
  allExtensions.append("csv");
  filters.append("Statistics File (*.vtmbmsstats)");
  filters.append("Statistics File (*.csv)");
}

void playlistItemStatisticsFile::onPOCParser(int poc)
{
  if (poc == this->currentDrawnFrameIdx)
    emit signalItemChanged(true, RECACHE_NONE); 
}

void playlistItemStatisticsFile::createPropertiesWidget()
{
  Q_ASSERT_X(!propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  this->preparePropertiesWidget(QStringLiteral("playlistItemStatisticsFile"));

  // On the top level everything is layout vertically
  auto vAllLaout = new QVBoxLayout(propertiesWidget.data());

  auto line = new QFrame;
  line->setObjectName(QStringLiteral("lineOne"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  vAllLaout->addLayout(createPlaylistItemControls());
  vAllLaout->addWidget(line);
  vAllLaout->addLayout(this->statisticsUIHandler.createStatisticsHandlerControls());

  // Do not add any stretchers at the bottom because the statistics handler controls will
  // expand to take up as much space as there is available
}

void playlistItemStatisticsFile::openStatisticsFile()
{
  // Is the background parser still running? If yes, abort it.
  if (this->backgroundParserFuture.isRunning())
  {
    // signal to background thread that we want to cancel the processing
    this->breakBackgroundAtomic.store(true);
    this->backgroundParserFuture.waitForFinished();
  }

  auto suffix = QFileInfo(this->prop.name).suffix();
  if (this->openMode == OpenMode::CSVFile ||
      (this->openMode == OpenMode::Extension && suffix == "csv"))
    this->file.reset(new stats::StatisticsFileCSV(this->prop.name, this->statisticsData));
  else if (this->openMode == OpenMode::VTMBMSFile ||
           (this->openMode == OpenMode::Extension && suffix == "vtmbmsstats"))
    this->file.reset(new stats::StatisticsFileVTMBMS(this->prop.name, this->statisticsData));
  else
    assert(false);

  connect(this->file.get(), &stats::StatisticsFileBase::readPOC, this, &playlistItemStatisticsFile::onPOCParser);

  // Run the parsing of the file in the background
  this->timer.start(1000, this);
  this->backgroundParserFuture = QtConcurrent::run(
      [=](stats::StatisticsFileBase *file) {
        file->readFrameAndTypePositionsFromFile(std::ref(this->breakBackgroundAtomic));
      },
      this->file.get());
}

// This timer event is called regularly when the background loading process is running.
void playlistItemStatisticsFile::timerEvent(QTimerEvent *event)
{
  if (event->timerId() != timer.timerId())
    return playlistItem::timerEvent(event);

  if (!backgroundParserFuture.isRunning())
    timer.stop();

  if (this->file)
    this->prop.startEndRange = indexRange(0, this->file->getMaxPoc());
  emit signalItemChanged(false, RECACHE_NONE);
}
