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

#include "HRD.h"

#include "SEI/buffering_period.h"
#include "SEI/pic_timing.h"
#include "seq_parameter_set_rbsp.h"

#include <cmath>

#define PARSER_AVC_HRD_DEBUG_OUTPUT 1
#if PARSER_AVC_HRD_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVC_HRD(msg) qDebug() << msg
#else
#define DEBUG_AVC_HRD(fmt) ((void)0)
#endif

namespace parser::avc

{

void HRD::addAU(size_t                                         auBits,
                unsigned                                       poc,
                std::shared_ptr<seq_parameter_set_rbsp> const &sps,
                std::shared_ptr<buffering_period> const &      lastBufferingPeriodSEI,
                std::shared_ptr<pic_timing> const &            lastPicTimingSEI,
                HRDPlotModel *                                 plotModel)
{
  if (plotModel == nullptr)
    throw std::logic_error("No plot model given for HRD");
  if (!sps)
    throw std::logic_error("No SPS given for HRD");
  if (!lastBufferingPeriodSEI)
    throw std::logic_error("No Buffering Period SEI given for HRD");
  if (!lastPicTimingSEI)
    throw std::logic_error("No Picture timing SEI given for HRD");

  if (!sps->seqParameterSetData.vui_parameters_present_flag ||
      !sps->seqParameterSetData.vuiParameters.nal_hrd_parameters_present_flag)
    return;

  auto &vuiParam = sps->seqParameterSetData.vuiParameters;

  // There could be multiple but we currently only support one.
  const auto SchedSelIdx = 0;

  const auto initial_cpb_removal_delay =
      lastBufferingPeriodSEI->initial_cpb_removal_delay[SchedSelIdx];
  const auto initial_cpb_removal_delay_offset =
      lastBufferingPeriodSEI->initial_cpb_removal_delay_offset[SchedSelIdx];

  const bool cbr_flag = vuiParam.nalHrdParameters.cbr_flag[SchedSelIdx];

  // TODO: Investigate the difference between our results and the results from stream-Eye
  // I noticed that the results from stream eye differ by a (seemingly random) additional
  // number of bits that stream eye counts for au 0.
  // For one example I investigated it looked like the parameter sets (SPS + PPS) were counted twice
  // for the first AU. if (this->au_n == 0)
  // {
  //   auBits += 49 * 8;
  // }

  /* Some notation:
    t_ai: The time at which the first bit of access unit n begins to enter the CPB is
    referred to as the initial arrival time. t_r_nominal_n: the nominal removal time of the
    access unit from the CPB t_r_n: The removal time of access unit n
  */

  // Annex C: The variable t c is derived as follows and is called a clock tick:
  time_t t_c = time_t(vuiParam.num_units_in_tick) / vuiParam.time_scale;

  // ITU-T Rec H.264 (04/2017) - C.1.2 - Timing of coded picture removal
  // Part 1: the nominal removal time of the access unit from the CPB
  time_t t_r_nominal_n;
  if (this->au_n == 0)
    t_r_nominal_n = time_t(initial_cpb_removal_delay) / 90000;
  else
  {
    // n is not equal to 0. The removal time depends on the removal time of the previous AU.
    const auto cpb_removal_delay = unsigned(lastPicTimingSEI->cpb_removal_delay);
    t_r_nominal_n                = this->t_r_nominal_n_first + t_c * cpb_removal_delay;
  }

  if (this->isFirstAUInBufferingPeriod)
    this->t_r_nominal_n_first = t_r_nominal_n;

  // ITU-T Rec H.264 (04/2017) - C.1.1 - Timing of bitstream arrival
  time_t t_ai;
  if (this->au_n == 0)
    t_ai = 0;
  else
  {
    if (cbr_flag)
      t_ai = this->t_af_nm1;
    else
    {
      time_t t_ai_earliest = 0;
      time_t init_delay;
      if (this->isFirstAUInBufferingPeriod)
        init_delay = time_t(initial_cpb_removal_delay) / 90000;
      else
        init_delay = time_t(initial_cpb_removal_delay + initial_cpb_removal_delay_offset) / 90000;
      t_ai_earliest = t_r_nominal_n - init_delay;
      t_ai          = std::max(this->t_af_nm1, t_ai_earliest);
    }
  }

  // The final arrival time for access unit n is derived by:
  const auto bitrate = vuiParam.nalHrdParameters.BitRate[SchedSelIdx];
  time_t     t_af    = t_ai + time_t(auBits) / bitrate;

  // ITU-T Rec H.264 (04/2017) - C.1.2 - Timing of coded picture removal
  // Part 2: The removal time of access unit n is specified as follows:
  time_t t_r_n;
  if (!vuiParam.low_delay_hrd_flag || t_r_nominal_n >= t_af)
    t_r_n = t_r_nominal_n;
  else
    // NOTE: This indicates that the size of access unit n, b(n), is so large that it
    // prevents removal at the nominal removal time.
    t_r_n = t_r_nominal_n * t_c * (t_af - t_r_nominal_n) / t_c;

  // C.3 - Bitstream conformance
  // 1.
  if (this->au_n > 0 && this->isFirstAUInBufferingPeriod)
  {
    time_t t_g_90 = (t_r_nominal_n - this->t_af_nm1) * 90000;
    time_t initial_cpb_removal_delay_time(initial_cpb_removal_delay);
    if (!cbr_flag && initial_cpb_removal_delay_time > std::ceil(t_g_90))
    {
      // TODO: These logs should go somewhere!
      DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc
                              << " - Warning: Conformance fail. initial_cpb_removal_delay "
                              << initial_cpb_removal_delay << " - should be <= ceil(t_g_90) "
                              << double(ceil(t_g_90)));
    }

    if (cbr_flag && initial_cpb_removal_delay_time < std::floor(t_g_90))
    {
      DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc
                              << " - Warning: Conformance fail. initial_cpb_removal_delay "
                              << initial_cpb_removal_delay << " - should be >= floor(t_g_90) "
                              << double(std::floor(t_g_90)));
    }
  }

  // C.3. - 2 Check the decoding buffer fullness

  // Calculate the fill state of the decoding buffer. This works like this:
  // For each frame (AU), the buffer is fileed from t_ai to t_af (which indicate) from
  // when to when bits are recieved for a frame. time t_r, the access unit is removed from
  // the buffer. If none of this applies, the buffer fill stays constant.
  // (All values are scaled by 90000 to avoid any rounding errors)
  const auto buffer_size = vuiParam.nalHrdParameters.CpbSize[SchedSelIdx];

  // The time windows we have to process is from t_af_nm1 to t_af (t_ai is in between
  // these two).

  // 1: Process the time from t_af_nm1 (>) to t_ai (<=) (previously processed AUs might be removed
  //    in this time window (tr_n)). In this time, no data is added to the buffer (no
  //    frame is being received) but frames may be removed from the buffer. This time can
  //    be zero.
  //    In case of a CBR encode, no time of constant bitrate should occur.
  if (this->t_af_nm1 < t_ai)
  {
    auto it            = this->framesToRemove.begin();
    auto lastFrameTime = this->t_af_nm1;
    while (it != this->framesToRemove.end())
    {
      if (it->t_r < this->t_af_nm1 || (*it).t_r <= t_ai)
      {
        if (it->t_r < this->t_af_nm1)
        {
          // This should not happen (all frames prior to t_af_nm1 should have been
          // removed from the buffer already). Remove now and warn.
          DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc
                                  << " - Warning: Removing frame with removal time ("
                                  << double(it->t_r) << ") before final arrival time ("
                                  << double(t_af_nm1) << "). Buffer underflow");
        }
        this->addConstantBufferLine(poc, lastFrameTime, (*it).t_r, plotModel);
        this->removeFromBufferAndCheck((*it), poc, (*it).t_r, plotModel);
        it = this->framesToRemove.erase(it);
        if (it != this->framesToRemove.end())
          lastFrameTime = (*it).t_r;
      }
      else
        break;
    }
  }

  // 2. For the time from t_ai to t_af, the buffer is filled with the signaled bitrate.
  //    The overall bits which are added in this time period correspond to the size
  //    of the frame in bits which will be later removed again.
  //    Also, previously received frames can be removed in this time interval.
  assert(t_af > t_ai);
  auto relevant_frames = this->popRemoveFramesInTimeInterval(t_ai, t_af);

  bool underflowRemoveCurrentAU = false;
  if (t_r_n <= t_af)
  {
    // The picture is removed from the buffer before the last bit of it could be
    // received. This is a buffer underflow. We handle this by aborting transmission of bits for
    // that AU when the frame is recieved.
    underflowRemoveCurrentAU = true;
  }

  auto au_buffer_add = auBits;
  {
    // time_t au_time_expired = t_af - t_ai;
    // uint64_t au_bits_add   = (uint64_t)(au_time_expired * (unsigned int) bitrate);
    // assert(au_buffer_add == au_bits_add || au_buffer_add == au_bits_add + 1 ||
    // au_buffer_add + 1 == au_bits_add);
  }
  if (relevant_frames.empty() && !underflowRemoveCurrentAU)
  {
    this->addToBufferAndCheck(au_buffer_add, buffer_size, poc, t_ai, t_af, plotModel);
  }
  else
  {
    // While bits are coming into the buffer, frames are removed as well.
    // For each period, we add the bits (and check the buffer state) and remove the
    // frames from the buffer (and check the buffer state).
    auto        t_ai_sub             = t_ai;
    size_t      buffer_add_sum       = 0;
    long double buffer_add_remainder = 0;
    for (auto frame : relevant_frames)
    {
      assert(frame.t_r >= t_ai_sub && frame.t_r < t_af);
      const time_t time_expired          = frame.t_r - t_ai_sub;
      long double  buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      unsigned int buffer_add            = std::floor(buffer_add_fractional);
      buffer_add_remainder               = buffer_add_fractional - buffer_add;
      buffer_add_sum += buffer_add;
      this->addToBufferAndCheck(buffer_add, buffer_size, poc, t_ai_sub, frame.t_r, plotModel);
      this->removeFromBufferAndCheck(frame, poc, frame.t_r, plotModel);
      t_ai_sub = frame.t_r;
    }
    if (underflowRemoveCurrentAU)
    {
      if (t_r_n < t_ai_sub)
      {
        // The removal time is even before the previous frame?!? Is this even possible?
      }
      // The last interval from t_ai_sub to t_r_n. After t_r_n we stop the current frame.
      const time_t time_expired          = t_r_n - t_ai_sub;
      long double  buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      unsigned int buffer_add            = std::floor(buffer_add_fractional);
      this->addToBufferAndCheck(buffer_add, buffer_size, poc, t_ai_sub, t_r_n, plotModel);
      this->removeFromBufferAndCheck(HRDFrameToRemove(t_r_n, auBits, poc), poc, t_r_n, plotModel);
      // "Stop transmission" at this point. We have removed the frame so transmission of more bytes
      // for it make no sense.
      t_af = t_r_n;
    }
    else
    {
      // The last interval from t_ai_sub to t_af
      assert(au_buffer_add >= buffer_add_sum);
      auto buffer_add_remain = au_buffer_add - buffer_add_sum;
      // The sum should correspond to the size of the complete AU
      time_t      time_expired          = t_af - t_ai_sub;
      long double buffer_add_fractional = bitrate * time_expired + buffer_add_remainder;
      {
        auto buffer_add = size_t(std::round(buffer_add_fractional));
        buffer_add_sum += buffer_add;
        // assert(buffer_add_sum == au_buffer_add || buffer_add_sum + 1 == au_buffer_add
        // || buffer_add_sum == au_buffer_add + 1);
      }
      this->addToBufferAndCheck(buffer_add_remain, buffer_size, poc, t_ai_sub, t_af, plotModel);
    }
  }

  if (!underflowRemoveCurrentAU)
    framesToRemove.push_back(HRDFrameToRemove(t_r_n, auBits, poc));

  this->t_af_nm1 = t_af;

  // C.3 - 3. A CPB underflow is specified as the condition in which t_r,n(n) is less than
  // t_af(n). When low_delay_hrd_flag is equal to 0, the CPB shall never underflow.
  if (t_r_nominal_n < t_af && !vuiParam.low_delay_hrd_flag)
  {
    DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc
                            << " - Warning: Decoding Buffer underflow t_r_n " << double(t_r_n)
                            << " t_af " << double(t_af));
  }

  this->au_n++;
  this->isFirstAUInBufferingPeriod = false;
}

void HRD::endOfFile(HRDPlotModel *plotModel)
{
  // From time this->t_af_nm1 onwards, just remove all of the frames which have not been removed
  // yet.
  auto lastFrameTime = this->t_af_nm1;
  for (const auto &frame : this->framesToRemove)
  {
    this->addConstantBufferLine(frame.poc, lastFrameTime, frame.t_r, plotModel);
    this->removeFromBufferAndCheck(frame, frame.poc, frame.t_r, plotModel);
    lastFrameTime = frame.t_r;
  }
  this->framesToRemove.clear();
}

std::vector<HRD::HRDFrameToRemove> HRD::popRemoveFramesInTimeInterval(time_t from, time_t to)
{
  std::vector<HRD::HRDFrameToRemove> l;
  auto                               it           = this->framesToRemove.begin();
  double                             t_r_previous = 0;
  while (it != this->framesToRemove.end())
  {
    if ((*it).t_r < from)
    {
      DEBUG_AVC_HRD("Warning: Frame " << (*it).poc << " was not removed at the time ("
                                      << double((*it).t_r)
                                      << ") it should have been. Dropping it now.");
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r < t_r_previous)
    {
      DEBUG_AVC_HRD("Warning: Frame " << (*it).poc << " has a removal time (" << double((*it).t_r)
                                      << ") before the previous frame (" << t_r_previous
                                      << "). Dropping it now.");
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r >= from && (*it).t_r < to)
    {
      t_r_previous = (*it).t_r;
      l.push_back((*it));
      it = this->framesToRemove.erase(it);
      continue;
    }
    if ((*it).t_r >= to)
      break;
    // Prevent an infinite loop in case of wrong data. We should never reach this if all timings are
    // correct.
    it++;
  }
  return l;
}

void HRD::addToBufferAndCheck(size_t        bufferAdd,
                              size_t        bufferSize,
                              int           poc,
                              time_t        t_begin,
                              time_t        t_end,
                              HRDPlotModel *plotModel)
{
  const auto bufferOld = this->decodingBufferLevel;
  this->decodingBufferLevel += bufferAdd;

  {
    HRDPlotModel::HRDEntry entry;
    entry.type               = HRDPlotModel::HRDEntry::EntryType::Adding;
    entry.cbp_fullness_start = bufferOld;
    entry.cbp_fullness_end   = this->decodingBufferLevel;
    entry.time_offset_start  = t_begin;
    entry.time_offset_end    = t_end;
    entry.poc                = poc;
    plotModel->addHRDEntry(entry);
  }
  if (this->decodingBufferLevel > int64_t(bufferSize))
  {
    this->decodingBufferLevel = bufferSize;
    DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc << " - Warning: Time " << double(t_end)
                            << " Decoding Buffer overflow by "
                            << (int(this->decodingBufferLevel) - bufferSize) << "bits"
                            << " added bits " << bufferAdd << ")");
  }
}

void HRD::removeFromBufferAndCheck(const HRDFrameToRemove &frame,
                                   int                     poc,
                                   time_t                  removalTime,
                                   HRDPlotModel *          plotModel)
{
  (void)poc;

  // Remove the frame from the buffer
  auto       bufferSub = frame.bits;
  const auto bufferOld = this->decodingBufferLevel;
  this->decodingBufferLevel -= bufferSub;
  {
    HRDPlotModel::HRDEntry entry;
    entry.type               = HRDPlotModel::HRDEntry::EntryType::Removal;
    entry.cbp_fullness_start = bufferOld;
    entry.cbp_fullness_end   = this->decodingBufferLevel;
    entry.time_offset_start  = removalTime;
    entry.time_offset_end    = removalTime;
    entry.poc                = frame.poc;
    plotModel->addHRDEntry(entry);
  }
  if (this->decodingBufferLevel < 0)
  {
    // The buffer did underflow; i.e. we need to decode a pictures
    // at the time but there is not enough data in the buffer to do so (to take the AU
    // out of the buffer).
    DEBUG_AVC_HRD("HRD AU " << this->au_n << " POC " << poc << " - Warning: Time "
                            << double(frame.t_r) << " Decoding Buffer underflow by "
                            << this->decodingBufferLevel << "bits");
  }
}

void HRD::addConstantBufferLine(int poc, time_t t_begin, time_t t_end, HRDPlotModel *plotModel)
{
  HRDPlotModel::HRDEntry entry;
  entry.type               = HRDPlotModel::HRDEntry::EntryType::Adding;
  entry.cbp_fullness_start = this->decodingBufferLevel;
  entry.cbp_fullness_end   = this->decodingBufferLevel;
  entry.time_offset_start  = t_begin;
  entry.time_offset_end    = t_end;
  entry.poc                = poc;
  plotModel->addHRDEntry(entry);
}

} // namespace parser::avc