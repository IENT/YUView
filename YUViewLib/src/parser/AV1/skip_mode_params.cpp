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

#include "skip_mode_params.h"

#include "reference_mode.h"
#include "sequence_header_obu.h"
#include "typedef.h"

namespace parser::av1
{

using namespace reader;

void skip_mode_params::parse(SubByteReaderLogging &               reader,
                             std::shared_ptr<sequence_header_obu> seqHeader,
                             bool                                 FrameIsIntra,
                             reference_mode &                     referenceMode,
                             GlobalDecodingValues &               decValues,
                             FrameRefs &                          frameRefs,
                             unsigned                             OrderHint)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "skip_mode_params()");

  if (FrameIsIntra || !referenceMode.reference_select || !seqHeader->enable_order_hint)
  {
    this->skipModeAllowed = false;
  }
  else
  {
    this->forwardIdx  = -1;
    this->backwardIdx = -1;
    for (unsigned i = 0; i < REFS_PER_FRAME; i++)
    {
      this->refHint = decValues.RefOrderHint[frameRefs.ref_frame_idx[i]];
      if (get_relative_dist(
              refHint, OrderHint, seqHeader->enable_order_hint, seqHeader->OrderHintBits) < 0)
      {
        if (forwardIdx < 0 || get_relative_dist(refHint,
                                                this->forwardHint,
                                                seqHeader->enable_order_hint,
                                                seqHeader->OrderHintBits) > 0)
        {
          this->forwardIdx  = i;
          this->forwardHint = refHint;
        }
      }
      else if (get_relative_dist(
                   refHint, OrderHint, seqHeader->enable_order_hint, seqHeader->OrderHintBits) > 0)
      {
        if (backwardIdx < 0 || get_relative_dist(refHint,
                                                 this->backwardHint,
                                                 seqHeader->enable_order_hint,
                                                 seqHeader->OrderHintBits) < 0)
        {
          this->backwardIdx  = i;
          this->backwardHint = refHint;
        }
      }
    }

    if (forwardIdx < 0)
    {
      this->skipModeAllowed = 0;
    }
    else if (backwardIdx >= 0)
    {
      this->skipModeAllowed  = 1;
      this->SkipModeFrame[1] = LAST_FRAME + std::max(forwardIdx, backwardIdx);
      this->SkipModeFrame[0] = LAST_FRAME + std::min(forwardIdx, backwardIdx);
    }
    else
    {
      this->secondForwardIdx = -1;
      for (unsigned i = 0; i < REFS_PER_FRAME; i++)
      {
        this->refHint = decValues.RefOrderHint[frameRefs.ref_frame_idx[i]];
        if (get_relative_dist(refHint,
                              this->forwardHint,
                              seqHeader->enable_order_hint,
                              seqHeader->OrderHintBits) < 0)
        {
          if (secondForwardIdx < 0 || get_relative_dist(refHint,
                                                        this->secondForwardHint,
                                                        seqHeader->enable_order_hint,
                                                        seqHeader->OrderHintBits) > 0)
          {
            this->secondForwardIdx  = i;
            this->secondForwardHint = refHint;
          }
        }
      }
      if (this->secondForwardIdx < 0)
      {
        this->skipModeAllowed = 0;
      }
      else
      {
        this->skipModeAllowed  = 1;
        this->SkipModeFrame[0] = LAST_FRAME + std::min(this->forwardIdx, this->secondForwardIdx);
        this->SkipModeFrame[1] = LAST_FRAME + std::max(this->forwardIdx, this->secondForwardIdx);
      }
    }
  }

  if (this->skipModeAllowed)
  {
    this->skip_mode_present = reader.readFlag("skip_mode_present");
  }
  else
  {
    this->skip_mode_present = false;
  }
}

} // namespace parser::av1