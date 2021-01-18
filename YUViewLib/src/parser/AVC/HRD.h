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

#include "parser/common/HRDPlotModel.h"

#include <memory>
#include <vector>

namespace parser::avc
{

class seq_parameter_set_rbsp;
class buffering_period;
class pic_timing;

class HRD
{
public:
  HRD() = default;
  void addAU(size_t                                       auBits,
             unsigned                                       poc,
             std::shared_ptr<seq_parameter_set_rbsp> const &sps,
             std::shared_ptr<buffering_period> const &      lastBufferingPeriodSEI,
             std::shared_ptr<pic_timing> const &            lastPicTimingSEI,
             HRDPlotModel *                                 plotModel);
  void endOfFile(HRDPlotModel *plotModel);

  bool isFirstAUInBufferingPeriod{true};

private:
  typedef long double time_t;

  // We keep a list of frames which will be removed in the future
  struct HRDFrameToRemove
  {
    HRDFrameToRemove(time_t t_r, int bits, int poc) : t_r(t_r), bits(bits), poc(poc) {}
    time_t       t_r;
    unsigned int bits;
    int          poc;
  };
  std::vector<HRDFrameToRemove> framesToRemove;

  // The access unit count (n) for this HRD. The HRD is initialized with au n=0.
  uint64_t au_n{0};
  // Final arrival time (t_af for n minus 1)
  time_t t_af_nm1{0};
  // t_r,n(nb) is the nominal removal time of the first access unit of the previous buffering period
  time_t t_r_nominal_n_first;

  std::vector<HRDFrameToRemove> popRemoveFramesInTimeInterval(time_t from, time_t to);
  void                          addToBufferAndCheck(unsigned      bufferAdd,
                                                    unsigned      bufferSize,
                                                    int           poc,
                                                    time_t        t_begin,
                                                    time_t        t_end,
                                                    HRDPlotModel *plotModel);
  void                          removeFromBufferAndCheck(const HRDFrameToRemove &frame,
                                                         int                     poc,
                                                         time_t                  removalTime,
                                                         HRDPlotModel *          plotModel);
  void addConstantBufferLine(int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel);

  int64_t decodingBufferLevel{0};
};

} // namespace parser::avc
