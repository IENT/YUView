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

#include "frame_size.h"

#include <common/Typedef.h>

#include "sequence_header_obu.h"

namespace parser::av1
{

using namespace reader;

void frame_size::parse(SubByteReaderLogging &               reader,
                       std::shared_ptr<sequence_header_obu> seqHeader,
                       bool                                 frame_size_override_flag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "frame_size()");

  if (frame_size_override_flag)
  {
    this->frame_width_minus_1 =
        reader.readBits("frame_width_minus_1", seqHeader->frame_width_bits_minus_1 + 1);
    this->frame_height_minus_1 =
        reader.readBits("frame_height_minus_1", seqHeader->frame_height_bits_minus_1 + 1);
    this->FrameWidth  = frame_width_minus_1 + 1;
    this->FrameHeight = frame_height_minus_1 + 1;
  }
  else
  {
    this->FrameWidth  = seqHeader->max_frame_width_minus_1 + 1;
    this->FrameHeight = seqHeader->max_frame_height_minus_1 + 1;
  }

  reader.logCalculatedValue("FrameWidth", this->FrameWidth);
  reader.logCalculatedValue("FrameHeight", this->FrameHeight);

  UpscaledWidth      = FrameWidth;
  auto newFrameWidth = this->superresParams.parse(reader, seqHeader, this->UpscaledWidth);
  FrameWidth         = newFrameWidth;

  this->compute_image_size();
}

void frame_size::parse_with_refs(SubByteReaderLogging &               reader,
                                 std::shared_ptr<sequence_header_obu> seqHeader,
                                 GlobalDecodingValues &               decValues,
                                 FrameRefs &                          frameRefs,
                                 bool                                 frame_size_override_flag)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "frame_size_with_refs()");

  bool ref_found = false;
  for (unsigned i = 0; i < REFS_PER_FRAME; i++)
  {
    bool found_ref = reader.readFlag("found_ref");
    if (found_ref)
    {
      this->UpscaledWidth           = decValues.RefUpscaledWidth[frameRefs.ref_frame_idx[i]];
      this->FrameWidth              = UpscaledWidth;
      this->FrameHeight             = decValues.RefFrameHeight[frameRefs.ref_frame_idx[i]];
      this->renderSize.RenderWidth  = decValues.RefRenderWidth[frameRefs.ref_frame_idx[i]];
      this->renderSize.RenderHeight = decValues.RefRenderHeight[frameRefs.ref_frame_idx[i]];
      ref_found                     = true;

      reader.logCalculatedValue("UpscaledWidth", this->UpscaledWidth);
      reader.logCalculatedValue("FrameWidth", this->FrameWidth);
      reader.logCalculatedValue("FrameHeight", this->FrameHeight);
      reader.logCalculatedValue("RenderWidth", this->renderSize.RenderWidth);
      reader.logCalculatedValue("RenderHeight", this->renderSize.RenderHeight);

      break;
    }
  }
  if (!ref_found)
  {
    this->parse(reader, seqHeader, frame_size_override_flag);
    this->renderSize.parse(reader, this->UpscaledWidth, this->FrameHeight);
  }
  else
  {
    auto newFrameWidth = this->superresParams.parse(reader, seqHeader, this->UpscaledWidth);
    UpscaledWidth      = FrameWidth;
    FrameWidth         = newFrameWidth;

    this->compute_image_size();
  }
}

void frame_size::compute_image_size()
{
  this->MiCols = 2 * ((this->FrameWidth + 7) >> 3);
  this->MiRows = 2 * ((this->FrameHeight + 7) >> 3);
}

} // namespace parser::av1