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

#include "cdef_params.h"

#include <common/Typedef.h>
#include <parser/common/Functions.h>

#include "sequence_header_obu.h"

namespace parser::av1
{

using namespace reader;

void cdef_params::parse(reader::SubByteReaderLogging &       reader,
             std::shared_ptr<sequence_header_obu> seqHeader,
             bool                                 CodedLossless,
             bool                                 allow_intrabc)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "cdef_params()");
  
  if (CodedLossless || allow_intrabc || !seqHeader->enable_cdef)
  {
    this->cdef_bits = 0;
    this->cdef_y_pri_strength[0] = 0;
    this->cdef_y_sec_strength[0] = 0;
    this->cdef_uv_pri_strength[0] = 0;
    this->cdef_uv_sec_strength[0] = 0;
    this->CdefDamping = 3;
    return;
  }

  this->cdef_damping_minus_3 = reader.readBits("cdef_damping_minus_3", 2);
  this->CdefDamping = this->cdef_damping_minus_3 + 3;
  this->cdef_bits = reader.readBits("cdef_bits", 2);
  for (unsigned i = 0; i < (1u << this->cdef_bits); i++)
  {
    this->cdef_y_pri_strength[i] = reader.readBits(formatArray("cdef_y_pri_strength", i), 4);
    this->cdef_y_sec_strength[i] = reader.readBits(formatArray("cdef_y_sec_strength", i), 2);
    if (this->cdef_y_sec_strength[i] == 3)
      this->cdef_y_sec_strength[i]++;
    if (seqHeader->colorConfig.NumPlanes > 1)
    {
      this->cdef_uv_pri_strength[i] = reader.readBits(formatArray("cdef_uv_pri_strength", i), 4);
      this->cdef_uv_sec_strength[i] = reader.readBits(formatArray("cdef_uv_sec_strength", i), 4);
      if (this->cdef_uv_sec_strength[i] == 3)
        this->cdef_uv_sec_strength[i]++;
    }
  }
}

} // namespace parser::av1