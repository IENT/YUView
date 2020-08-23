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

#define HRD_PLOT_MODE_DEBUG 0
#if HRD_PLOT_MODE_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_PLOT(msg) qDebug() << msg
#else
#define DEBUG_PLOT(msg) ((void)0)
#endif

#include "HRDPlotModel.h"

int timestampToInt(double timestamp)
{
  // Doubles are not really supported for plotting. Lets just scale everything by 90000
  return int(timestamp * 90000);
}

PlotModel::StreamParameter HRDPlotModel::getStreamParameter(unsigned streamIndex) const
{
  if (streamIndex > 0)
    return {};

  QMutexLocker locker(&this->dataMutex);

  PlotModel::StreamParameter streamParameter;
  streamParameter.xRange = {0, timestampToInt(this->time_offset_max)};
  streamParameter.yRange = {0, this->cpb_buffer_size * 8};

  const auto nrPoints = this->data.empty() ? 0 : unsigned(this->data.size() + 1);
  streamParameter.plotParameters.append({PlotType::Line, nrPoints});

  return streamParameter;
}

PlotModel::Point HRDPlotModel::getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const
{
  if (streamIndex > 0)
    return {};

  if (pointIndex == 0)
    return {0, 0, 0, false};

  QMutexLocker locker(&this->dataMutex);

  if (pointIndex > unsigned(this->data.size()))
    return {};

  PlotModel::Point point;
  point.x = timestampToInt(this->data[pointIndex - 1].time_offset_end);
  point.y = this->data[pointIndex - 1].cbp_fullness_end;
  
  return point;
}

QString HRDPlotModel::getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const
{
  if (streamIndex > 0)
    return {};

  if (pointIndex > unsigned(this->data.size()))
    return {};

  const auto &entry = this->data[pointIndex - 1];

  return QString("<h4>HRD</h4>"
                  "<table width=\"100%\">"
                  "<tr><td>cpb_fullness:</td><td align=\"right\">%1-%2</td></tr>"
                  "<tr><td>Time offset:</td><td align=\"right\">%3-%4</td></tr>"
                  "<tr><td>POC:</td><td align=\"right\">%5</td></tr>"
                  "</table>")
      .arg(entry.cbp_fullness_start)
      .arg(entry.cbp_fullness_end)
      .arg(entry.time_offset_start)
      .arg(entry.time_offset_end)
      .arg(entry.poc);
}

void HRDPlotModel::addHRDEntry(HRDPlotModel::HRDEntry &entry)
{
  QMutexLocker locker(&this->dataMutex);

  this->data.append(entry);

  if (entry.time_offset_end > this->time_offset_max)
    this->time_offset_max = entry.time_offset_end;

  DEBUG_PLOT("HRDPlotModel::addHRDEntry time_offset_end " << entry.time_offset_end << " cbp_fullness_end " << entry.cbp_fullness_end);

  this->eventSubsampler.postEvent();
  if (this->data.size() == 1)
    // Technically the number of streams did not change but with this we can inform the view to start drawing.
    emit nrStreamsChanged();
}

void HRDPlotModel::setCPBBufferSize(unsigned size)
{
  if (size > this->cpb_buffer_size)
  {
    QMutexLocker locker(&this->dataMutex);
    this->cpb_buffer_size = size;
    this->eventSubsampler.postEvent();
  }
}
