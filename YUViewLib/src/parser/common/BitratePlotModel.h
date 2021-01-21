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

#pragma once

#include <QMap>
#include <QMutex>
#include <QString>

#include "common/typedef.h"
#include "ui/views/plotModel.h"

class BitratePlotModel : public PlotModel
{
public:
  BitratePlotModel() = default;
  virtual ~BitratePlotModel() = default;

  unsigned getNrStreams() const override;
  PlotModel::StreamParameter getStreamParameter(unsigned streamIndex) const override;
  PlotModel::Point getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override;
  QString getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override;
  std::optional<unsigned> getReasonabelRangeToShowOnXAxisPer100Pixels() const override;
  QString formatValue(Axis axis, double value) const override;
  Range<double> getYRange() const override {return yMaxStreamRange;}
  QString getItemInfoText(int index);

  struct BitrateEntry
  {
    int dts {0};
    int pts {0};
    int duration {1};
    size_t bitrate {0};
    bool keyframe {false};
    QString frameType;
  };

  void addBitratePoint(int streamIndex, BitrateEntry &entry);
  void setBitrateSortingIndex(int index);

private:

  enum class SortMode
  {
    DECODE_ORDER,
    PRESENTATION_ORDER
  };
  SortMode sortMode { SortMode::DECODE_ORDER };

  QMap<unsigned int, QList<BitrateEntry>> dataPerStream;
  mutable QMutex dataMutex;

  unsigned int calculateAverageValue(unsigned streamIndex, unsigned pointIndex) const;

  Range<int> rangeDts;
  Range<int> rangePts;
  QMap<unsigned int, Range<int>> rangeBitratePerStream;
  Range<double> yMaxStreamRange;
};
