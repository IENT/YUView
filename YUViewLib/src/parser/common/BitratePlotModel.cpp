/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen
 * University, GERMANY
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

#define BITRATE_PLOT_MODE_DEBUG 0
#if BITRATE_PLOT_MODE_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_PLOT(msg) qDebug() << msg
#else
#define DEBUG_PLOT(msg) ((void)0)
#endif

#include "BitratePlotModel.h"

#include <common/functions.h>

unsigned BitratePlotModel::getNrStreams() const
{
  return this->dataPerStream.size();
}

PlotModel::StreamParameter BitratePlotModel::getStreamParameter(unsigned streamIndex) const
{
  QMutexLocker locker(&this->dataMutex);

  if (this->dataPerStream.contains(streamIndex))
  {
    PlotModel::StreamParameter streamParameter;
    streamParameter.xRange.min = (sortMode == SortMode::DECODE_ORDER) ? double(this->rangeDts.min) : double(this->rangePts.min);
    streamParameter.xRange.max = (sortMode == SortMode::DECODE_ORDER) ? double(this->rangeDts.max) : double(this->rangePts.max);
    streamParameter.yRange.min = double(this->rangeBitratePerStream[streamIndex].min);
    streamParameter.yRange.max = double(this->rangeBitratePerStream[streamIndex].max);

    const auto nrPoints = unsigned(this->dataPerStream[streamIndex].size());
    streamParameter.plotParameters.append({PlotType::Bar, nrPoints});
    streamParameter.plotParameters.append({PlotType::Line, nrPoints});

    return streamParameter;
  }
  return {};
}

PlotModel::Point BitratePlotModel::getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const
{
  QMutexLocker locker(&this->dataMutex);

  if (!this->dataPerStream.contains(streamIndex))
    return {};

  if (pointIndex < unsigned(this->dataPerStream[streamIndex].size()))
  {
    PlotModel::Point point;
    if (this->sortMode == SortMode::DECODE_ORDER)
      point.x = this->dataPerStream[streamIndex][pointIndex].dts;
    else
      point.x = this->dataPerStream[streamIndex][pointIndex].pts;
    point.intra = this->dataPerStream[streamIndex][pointIndex].keyframe;
    
    const auto isAveragePlot = (plotIndex == 1);
    if (isAveragePlot)
      point.y = this->calculateAverageValue(streamIndex, pointIndex);
    else
      point.y = this->dataPerStream[streamIndex][pointIndex].bitrate;
    point.width = this->dataPerStream[streamIndex][pointIndex].duration;

    return point;
  }

  return {};
}

QString BitratePlotModel::getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const
{
  QMutexLocker locker(&this->dataMutex);
  auto &entry = this->dataPerStream[streamIndex][pointIndex];
  const auto isAveragePlot = (plotIndex == 1);

  if (isAveragePlot)
    return QString("<h4>Stream Average %1</h4>"
                  "<table width=\"100%\">"
                  "<tr><td>PTS:</td><td align=\"right\">%2</td></tr>"
                  "<tr><td>DTS:</td><td align=\"right\">%3</td></tr>"
                  "<tr><td>Average:</td><td align=\"right\">%4</td></tr>"
                  "<tr><td>Type:</td><td align=\"right\">%5</td></tr>"
                  "</table>")
      .arg(streamIndex)
      .arg(entry.pts)
      .arg(entry.dts)
      .arg(this->calculateAverageValue(streamIndex, pointIndex))
      .arg(entry.frameType);
  else
    return QString("<h4>Stream %1</h4>"
                    "<table width=\"100%\">"
                    "<tr><td>PTS:</td><td align=\"right\">%2</td></tr>"
                    "<tr><td>DTS:</td><td align=\"right\">%3</td></tr>"
                    "<tr><td>Duration:</td><td align=\"right\">%4</td></tr>"
                    "<tr><td>Bitrate:</td><td align=\"right\">%5</td></tr>"
                    "</table>")
        .arg(streamIndex)
        .arg(entry.pts)
        .arg(entry.dts)
        .arg(entry.duration)
        .arg(entry.bitrate);
}

std::optional<unsigned> BitratePlotModel::getReasonabelRangeToShowOnXAxisPer100Pixels() const
{
  std::optional<unsigned> range;
  for (auto &list : this->dataPerStream)
  {
    if (list.size() >= 2)
    {
      const auto minDistance = unsigned(std::abs(list.at(1).dts - list.at(0).dts));
      // Try to show 10 of these distance steps per 100 px
      const auto minDistancePer100Pix = minDistance * 10;
      if (minDistance == 0)
        continue;
      if (range)
        range = std::max(minDistancePer100Pix, *range);
      else
        range = minDistancePer100Pix;
    }
  }
  return range;
}

QString BitratePlotModel::formatValue(Axis axis, double value) const
{
  if (axis == Axis::X)
  {
    // The value is a timestamp. We could convert this to a time but for now a total value will do
    return QString("%1").arg(value);
  }
  else
    // The value is bytes
    return functions::formatDataSize(value, false);
}

void BitratePlotModel::addBitratePoint(int streamIndex, BitrateEntry &entry)
{
  QMutexLocker locker(&this->dataMutex);

  const auto newStream = !this->dataPerStream.contains(streamIndex);

  if (this->dataPerStream[streamIndex].empty())
  {
    rangeDts.min = entry.dts;
    rangeDts.max = entry.dts;
    rangePts.min = entry.pts;
    rangePts.max = entry.pts;
  }
  else
  {
    rangeDts.min = std::min(rangeDts.min, entry.dts);
    rangeDts.max = std::max(rangeDts.max, entry.dts);
    rangePts.min = std::min(rangePts.min, entry.pts);
    rangePts.max = std::max(rangePts.max, entry.pts);
  }
  
  rangeBitratePerStream[streamIndex].min = std::min(rangeBitratePerStream[streamIndex].min, int(entry.bitrate));
  rangeBitratePerStream[streamIndex].max = std::max(rangeBitratePerStream[streamIndex].max, int(entry.bitrate));

  // Store absolute minimum and maximum over all streams
  yMaxStreamRange.min = std::min(yMaxStreamRange.min, double(rangeBitratePerStream[streamIndex].min));
  yMaxStreamRange.max = std::max(yMaxStreamRange.max, double(rangeBitratePerStream[streamIndex].max));

  DEBUG_PLOT("BitrateItemModel::addBitratePoint streamIndex " << streamIndex << " pts " << entry.pts << " dts " << entry.dts << " rate " << entry.bitrate << " keyframe " << entry.keyframe);

  // Keep the list sorted
  const auto currentSortMode = this->sortMode;
  auto compareFunctionLessThen = [currentSortMode](const BitrateEntry &a, const BitrateEntry &b) 
  {
    if (currentSortMode == SortMode::DECODE_ORDER)
      return a.dts < b.dts;
    else
      return b.pts < a.pts;
  };

  auto insertIterator = std::upper_bound(this->dataPerStream[streamIndex].begin(), this->dataPerStream[streamIndex].end(), entry, compareFunctionLessThen);
  this->dataPerStream[streamIndex].insert(insertIterator, entry);
  this->eventSubsampler.postEvent();
  if (newStream)
    emit nrStreamsChanged();
}

void BitratePlotModel::setBitrateSortingIndex(int index)
{
  auto newSortMode = (index == 1) ? SortMode::PRESENTATION_ORDER : SortMode::DECODE_ORDER;
  if (this->sortMode == newSortMode)
    return;

  this->sortMode = newSortMode;

  const auto currentSortMode = this->sortMode;
  auto compareFunctionLessThen = [currentSortMode](const BitrateEntry &a, const BitrateEntry &b) 
  {
    if (currentSortMode == SortMode::DECODE_ORDER)
      return a.dts < b.dts;
    else
      return a.pts < b.pts;
  };

  QMutexLocker locker(&this->dataMutex);
  for (auto &list : this->dataPerStream)
    std::sort(list.begin(), list.end(), compareFunctionLessThen);
}

unsigned int BitratePlotModel::calculateAverageValue(unsigned streamIndex, unsigned pointIndex) const
{
  const unsigned int averageRange = 10;
  unsigned averageBitrate = 0;
  const unsigned start = std::max(unsigned(0), pointIndex - averageRange);
  const unsigned end = std::min(pointIndex + averageRange, unsigned(this->dataPerStream[streamIndex].size()));
  for (unsigned i = start; i < end; i++)
    averageBitrate += unsigned(this->dataPerStream[streamIndex][i].bitrate);
  return averageBitrate / (end - start);
}
