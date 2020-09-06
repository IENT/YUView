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

#include <QList>
#include <QMutex>
#include <QString>

#include "common/typedef.h"
#include "ui/views/plotModel.h"

class HRDPlotModel : public PlotModel
{
public:
  HRDPlotModel() = default;
  virtual ~HRDPlotModel() = default;

  unsigned getNrStreams() const override { return 1; }
  PlotModel::StreamParameter getStreamParameter(unsigned streamIndex) const override;
  PlotModel::Point getPlotPoint(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override;
  QString getPointInfo(unsigned streamIndex, unsigned plotIndex, unsigned pointIndex) const override;
  std::optional<unsigned> getReasonabelRangeToShowOnXAxisPer100Pixels() const override { return 1; }
  QString formatValue(Axis axis, double value) const override;
  
  QString getItemInfoText(int index);

  struct HRDEntry
  {
    // There are two types of entries. 
    // Adding: We are adding bits to the buffer or the buffer stays constant over time.
    //         `poc` is the POC of the frame that the adding corresponds to.
    // Removal: We remove a frame from the buffer. `time_offset_end` and `time_offset_start`
    //          will be identical. The `poc` is the POC of the frame that is being removed.
    enum class EntryType
    {
      Adding,
      Removal
    };
    EntryType type {EntryType::Adding};

    int cbp_fullness_start {0};
    int cbp_fullness_end {0};
    double time_offset_start {0};
    double time_offset_end {0};
    int poc;
  };

  void addHRDEntry(HRDEntry &entry);
  void setCPBBufferSize(int size);

private:

  QList<HRDEntry> data;
  mutable QMutex dataMutex;

  int cpb_buffer_size {0};
  double time_offset_max {0};
  Range<int> bufferLevelLimits {0, 0};
};
