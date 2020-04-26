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

#define BITRATE_PLOT_MODE_DEBUG 1
#if BITRATE_PLOT_MODE_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_PLOT(msg) qDebug() << msg
#else
#define DEBUG_PLOT(msg) ((void)0)
#endif

#include "bitratePlotModel.h"

unsigned int BitratePlotModel::getNrPlots() const { return 1; }

PlotModel::PlotParameter BitratePlotModel::getPlotParameter(unsigned plotIndex) const 
{
  QMutexLocker locker(&this->dataMutex);
  if (this->dataPerStream.contains(plotIndex))
  {
    PlotModel::PlotParameter parameter;
    parameter.nrpoints = this->dataPerStream[plotIndex].size();
    parameter.xRange = (sortMode == SortMode::DECODE_ORDER) ? this->rangeDts : this->rangePts;
    parameter.yRange = this->rangeBitratePerStream[plotIndex];
    parameter.type = PlotType::Bar;
    return parameter;
  }
  return {};
}

PlotModel::Point BitratePlotModel::getPlotPoint(unsigned plotIndex,
                                                unsigned pointIndex) const 
{
  if (!this->dataPerStream.contains(plotIndex))
    return {};

  if (pointIndex < this->dataPerStream[plotIndex].size()) {
    PlotModel::Point point;
    point.x = pointIndex;
    point.y = this->dataPerStream[plotIndex][pointIndex].bitrate;
    return point;
  }

  return {};
}

QString BitratePlotModel::getPointInfo(unsigned plotIndex,
                                       unsigned pointIndex) const 
{
  if (plotIndex == 0) {
    auto &entry = this->dataPerStream[0][pointIndex];
    return QString("<h4>Frame</h4>"
                   "<table width=\"100%\">"
                   "<tr><td>PTS:</td><td align=\"right\">%1</td></tr>"
                   "<tr><td>DTS:</td><td align=\"right\">%2</td></tr>"
                   "<tr><td>Bitrate:</td><td align=\"right\">%3</td></tr>"
                   "<tr><td>Type:</td><td align=\"right\">%4</td></tr>"
                   "</table>")
        .arg(entry.pts)
        .arg(entry.dts)
        .arg(entry.bitrate)
        .arg(entry.frameType);
  }
  return {};
}

// QVariant BitrateItemModel::data(const QModelIndex &index, int role) const
// {
//   DEBUG_MODEL("BitrateItemModel::data role %d row %d column %d", role,
//   index.row(), index.column()); if (role == Qt::DisplayRole)
//   {
//     if (index.column() == 0)
//       return index.row();
//     QMutexLocker locker(&this->bitratePerStreamDataMutex);
//     if (index.column() == 1)
//     {
//       const int averageRange = 10;
//       unsigned averageBitrate = 0;
//       unsigned start = qMax(0, index.row() - averageRange);
//       unsigned end = qMin(index.row() + averageRange,
//       bitratePerStreamData[0].size()); for (unsigned i = start; i < end; i++)
//       {
//         averageBitrate += this->bitratePerStreamData[0][i].bitrate;
//       }
//       return averageBitrate / (end - start);
//     }
//     const bool key = this->bitratePerStreamData[0][index.row()].keyframe;
//     if (!key && index.column() == 2)
//       return this->bitratePerStreamData[0][index.row()].bitrate;
//     if (key && index.column() == 3)
//       return this->bitratePerStreamData[0][index.row()].bitrate;
//     // if (index.column() == 3)
//     //   return this->bitratePerStreamData[0][index.row()].dts;
//     // if (index.column() == 4)
//     //   return this->bitratePerStreamData[0][index.row()].pts;
//   }

//   if (role == Qt::BackgroundRole)
//   {
//     QMutexLocker locker(&this->bitratePerStreamDataMutex);
//     if (this->bitratePerStreamData[0][index.row()].keyframe)
//       return QBrush(QColor(255, 0, 0));
//   }

//   return {};
// }

void BitratePlotModel::addBitratePoint(int streamIndex, bitrateEntry &entry) 
{
  QMutexLocker locker(&this->dataMutex);

  auto &graphData = this->dataPerStream[streamIndex];

  rangeDts.min = qMin(rangeDts.min, entry.dts);
  rangeDts.max = qMax(rangeDts.max, entry.dts);
  rangePts.min = qMin(rangePts.min, entry.pts);
  rangePts.max = qMax(rangePts.max, entry.pts);
  rangeBitratePerStream[streamIndex].min = qMin(rangeBitratePerStream[streamIndex].min, int(entry.bitrate));
  rangeBitratePerStream[streamIndex].max = qMax(rangeBitratePerStream[streamIndex].max, int(entry.bitrate));

  DEBUG_PLOT("BitrateItemModel::addBitratePoint streamIndex " << streamIndex << " pts " << entry.pts << " dts " << entry.dts << " rate " << entry.bitrate << " keyframe " << entry.keyframe);

  // Keep the list sorted

  const auto currentSortMode = this->sortMode;
  auto compareFunctionLessThen = [currentSortMode](const bitrateEntry &a, const bitrateEntry &b) 
  {
    if (currentSortMode == SortMode::DECODE_ORDER)
      return a.dts < b.dts;
    else
      return b.pts < a.pts;
  };

  auto insertIterator = std::upper_bound(this->dataPerStream[streamIndex].begin(), this->dataPerStream[streamIndex].end(), entry, compareFunctionLessThen);
  this->dataPerStream[streamIndex].insert(insertIterator, entry);
}

void BitratePlotModel::setBitrateSortingIndex(int index) 
{
  if (index == 1) {
    if (this->sortMode == SortMode::PRESENTATION_ORDER)
      return;
    this->sortMode = SortMode::PRESENTATION_ORDER;
  } else {
    if (this->sortMode == SortMode::DECODE_ORDER)
      return;
    this->sortMode = SortMode::DECODE_ORDER;
  }

  const auto currentSortMode = this->sortMode;
  auto compareFunctionLessThen = [currentSortMode](const bitrateEntry &a, const bitrateEntry &b) 
  {
    if (currentSortMode == SortMode::DECODE_ORDER)
      return a.dts < b.dts;
    else
      return a.pts < b.pts;
  };

  for (auto &list : this->dataPerStream)
    std::sort(list.begin(), list.end(), compareFunctionLessThen);

  // auto topLeft = this->index(0, 0);
  // auto bottomRight = this->index(this->rowCount(), this->columnCount());
  // emit QAbstractItemModel::dataChanged(topLeft, bottomRight);
}
