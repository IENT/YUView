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

#include "lr_params.h"

#include "Typedef.h"
#include "sequence_header_obu.h"

namespace parser::av1
{

using namespace reader;

void lr_params::parse(reader::SubByteReaderLogging &       reader,
                      std::shared_ptr<sequence_header_obu> seqHeader,
                      bool                                 AllLossless,
                      bool                                 allow_intrabc)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "lr_params()");

  if (AllLossless || allow_intrabc || !seqHeader->enable_restoration)
  {
    this->frameRestorationType[0] = FrameRestorationType::RESTORE_NONE;
    this->frameRestorationType[1] = FrameRestorationType::RESTORE_NONE;
    this->frameRestorationType[2] = FrameRestorationType::RESTORE_NONE;
    this->UsesLr                  = false;
    return;
  }

  const FrameRestorationType Remap_Lr_Type[] = {FrameRestorationType::RESTORE_NONE,
                                                FrameRestorationType::RESTORE_SWITCHABLE,
                                                FrameRestorationType::RESTORE_WIENER,
                                                FrameRestorationType::RESTORE_SGRPROJ};

  this->UsesLr       = false;
  this->usesChromaLr = false;
  for (unsigned i = 0; i < seqHeader->colorConfig.NumPlanes; i++)
  {
    this->lr_type.push_back(reader.readBits("lr_type", 2));
    this->frameRestorationType[i] = Remap_Lr_Type[lr_type[i]];
    if (this->frameRestorationType[i] != FrameRestorationType::RESTORE_NONE)
    {
      this->UsesLr = true;
      if (i > 0)
      {
        this->usesChromaLr = true;
      }
    }
  }

  if (this->UsesLr)
  {
    if (seqHeader->use_128x128_superblock)
    {
      this->lr_unit_shift = reader.readBits("lr_unit_shift", 1);
      this->lr_unit_shift++;
    }
    else
    {
      this->lr_unit_shift = reader.readBits("lr_unit_shift", 1);
      if (this->lr_unit_shift)
      {
        this->lr_unit_extra_shift = reader.readBits("lr_unit_extra_shift", 1);
        this->lr_unit_shift += this->lr_unit_extra_shift;
      }
    }
    this->LoopRestorationSize[0] = RESTORATION_TILESIZE_MAX >> (2 - this->lr_unit_shift);
    if (seqHeader->colorConfig.subsampling_x && seqHeader->colorConfig.subsampling_y &&
        this->usesChromaLr)
    {
      this->lr_uv_shift = reader.readBits("lr_uv_shift", 1);
    }
    else
    {
      this->lr_uv_shift = 0;
    }
    this->LoopRestorationSize[1] = this->LoopRestorationSize[0] >> this->lr_uv_shift;
    this->LoopRestorationSize[2] = this->LoopRestorationSize[0] >> this->lr_uv_shift;
  }
}

} // namespace parser::av1