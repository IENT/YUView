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

#include "FrameRefs.h"

#include "typedef.h"

namespace parser::av1
{

using namespace reader;

void FrameRefs::set_frame_refs(reader::SubByteReaderLogging &reader,
                               unsigned                      OrderHintBits,
                               bool                          enable_order_hint,
                               unsigned                      last_frame_idx,
                               unsigned                      gold_frame_idx,
                               unsigned                      OrderHint,
                               GlobalDecodingValues &        decValues)
{
  for (unsigned i = 0; i < REFS_PER_FRAME; i++)
    this->ref_frame_idx[i] = -1;
  this->ref_frame_idx[LAST_FRAME - LAST_FRAME]   = last_frame_idx;
  this->ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;

  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
    this->usedFrame[i] = false;
  this->usedFrame[last_frame_idx] = true;
  this->usedFrame[gold_frame_idx] = true;

  auto curFrameHint = 1 << (OrderHintBits - 1);
  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
    this->shiftedOrderHints[i] =
        curFrameHint +
        get_relative_dist(decValues.RefOrderHint[i], OrderHint, enable_order_hint, OrderHintBits);

  auto lastOrderHint = this->shiftedOrderHints[last_frame_idx];
  if (lastOrderHint >= curFrameHint)
    throw std::logic_error("It is a requirement of bitstream conformance that lastOrderHint is "
                           "strictly less than curFrameHint.");

  auto goldOrderHint = this->shiftedOrderHints[gold_frame_idx];
  if (goldOrderHint >= curFrameHint)
    throw std::logic_error("It is a requirement of bitstream conformance that goldOrderHint is "
                           "strictly less than curFrameHint.");

  int ref = this->find_latest_backward(curFrameHint);
  if (ref >= 0)
  {
    this->ref_frame_idx[ALTREF_FRAME - LAST_FRAME] = ref;
    this->usedFrame[ref]                           = true;
  }

  ref = this->find_earliest_backward(curFrameHint);
  if (ref >= 0)
  {
    this->ref_frame_idx[BWDREF_FRAME - LAST_FRAME] = ref;
    this->usedFrame[ref]                           = true;
  }

  ref = this->find_earliest_backward(curFrameHint);
  if (ref >= 0)
  {
    this->ref_frame_idx[ALTREF2_FRAME - LAST_FRAME] = ref;
    this->usedFrame[ref]                            = true;
  }

  unsigned Ref_Frame_List[REFS_PER_FRAME - 2] = {
      LAST2_FRAME, LAST3_FRAME, BWDREF_FRAME, ALTREF2_FRAME, ALTREF_FRAME};
  for (unsigned i = 0; i < REFS_PER_FRAME - 2; i++)
  {
    int refFrame = Ref_Frame_List[i];
    if (this->ref_frame_idx[refFrame - LAST_FRAME] < 0)
    {
      ref = this->find_latest_forward(curFrameHint);
      if (ref >= 0)
      {
        this->ref_frame_idx[refFrame - LAST_FRAME] = ref;
        this->usedFrame[ref]                       = true;
      }
    }
  }

  // Finally, any remaining references are set to the reference frame with smallest output order as
  // follows:
  ref = -1;
  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
  {
    auto hint = this->shiftedOrderHints[i];
    if (ref < 0 || hint < this->earliestOrderHint)
    {
      ref                     = i;
      this->earliestOrderHint = hint;
    }
  }
  for (unsigned i = 0; i < REFS_PER_FRAME; i++)
  {
    if (this->ref_frame_idx[i] < 0)
      this->ref_frame_idx[i] = ref;
  }
}

int FrameRefs::find_latest_backward(unsigned curFrameHint)
{
  int ref = -1;
  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
  {
    auto hint = shiftedOrderHints[i];
    if (!this->usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint >= this->latestOrderHint))
    {
      ref                   = i;
      this->latestOrderHint = hint;
    }
  }
  return ref;
}

int FrameRefs::find_earliest_backward(unsigned curFrameHint)
{
  int ref = -1;
  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
  {
    int hint = this->shiftedOrderHints[i];
    if (!this->usedFrame[i] && hint >= curFrameHint && (ref < 0 || hint < this->earliestOrderHint))
    {
      ref                     = i;
      this->earliestOrderHint = hint;
    }
  }
  return ref;
}

int FrameRefs::find_latest_forward(unsigned curFrameHint)
{
  int ref = -1;
  for (unsigned i = 0; i < NUM_REF_FRAMES; i++)
  {
    int hint = this->shiftedOrderHints[i];
    if (!this->usedFrame[i] && hint < curFrameHint && (ref < 0 || hint >= this->latestOrderHint))
    {
      ref                   = i;
      this->latestOrderHint = hint;
    }
  }
  return ref;
}

int FrameRefs::get_relative_dist(int a, int b, bool enable_order_hint, int OrderHintBits)
{
  if (!enable_order_hint)
    return 0;

  int diff = a - b;
  int m    = 1 << (OrderHintBits - 1);
  diff     = (diff & (m - 1)) - (diff & m);
  return diff;
}

} // namespace parser::av1