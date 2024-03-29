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

#include <common/EventSubsampler.h>
#include <common/Typedef.h>

#include <QObject>
#include <QTimer>
#include <optional>

enum class Axis
{
  X,
  Y
};

class PlotModel : public QObject
{
  Q_OBJECT

signals:
  void dataChanged();
  void nrStreamsChanged();

public:
  PlotModel();

  enum class PlotType
  {
    Bar,
    Line,
    ConstValue
  };

  struct PlotParameter
  {
    PlotType type{};
    unsigned nrpoints{};
  };

  // A limit can be used to draw a horizontal/vertical line at a certain position (x/y)
  struct Limit
  {
    QString name{};
    int     value{};
    Axis    axis{};
  };

  struct StreamParameter
  {
    unsigned             getNrPlots() const { return unsigned(this->plotParameters.size()); }
    Range<double>        xRange;
    Range<double>        yRange;
    QList<PlotParameter> plotParameters;
    QList<Limit>         limits;
  };

  struct Point
  {
    double x, y, width;
    bool   intra;
  };

  virtual unsigned        getNrStreams() const                           = 0;
  virtual StreamParameter getStreamParameter(unsigned streamIndex) const = 0;
  virtual Point
  getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const = 0;
  virtual QString
                                  getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const = 0;
  virtual std::optional<unsigned> getReasonabelRangeToShowOnXAxisPer100Pixels() const = 0;
  virtual QString                 formatValue(Axis axis, double value) const          = 0;
  virtual Range<double>           getYRange() const                                   = 0;
  std::optional<unsigned>
  getPointIndex(unsigned streamIndex, unsigned plotIndex, QPointF point) const;

protected:
  EventSubsampler eventSubsampler;
};
