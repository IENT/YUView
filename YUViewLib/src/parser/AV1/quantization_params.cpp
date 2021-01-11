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

#include "quantization_params.h"

#include "sequence_header_obu.h"
#include "typedef.h"

using namespace parser::reader;

namespace
{

int read_delta_q(SubByteReaderLogging &reader, std::string deltaValName)
{
  SubByteReaderLoggingSubLevel subLevel(reader, deltaValName);

  if (reader.readFlag("delta_coded"))
    return reader.readSU("delta_q", 1 + 6);
  else
    return 0;
}

} // namespace

namespace parser::av1
{

void quantization_params::parse(reader::SubByteReaderLogging &       reader,
                                std::shared_ptr<sequence_header_obu> seqHeader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "quantization_params()");

  this->base_q_idx = reader.readBits("base_q_idx", 8);
  this->DeltaQYDc  = read_delta_q(reader, "DeltaQYDc");
  if (seqHeader->color_config.NumPlanes > 1)
  {
    if (seqHeader->color_config.separate_uv_delta_q)
      this->diff_uv_delta = reader.readFlag("diff_uv_delta");
    else
      this->diff_uv_delta = false;
    this->DeltaQUDc = read_delta_q(reader, "DeltaQUDc");
    this->DeltaQUAc = read_delta_q(reader, "DeltaQUAc");
    if (diff_uv_delta)
    {
      this->DeltaQVDc = read_delta_q(reader, "DeltaQVDc");
      this->DeltaQVAc = read_delta_q(reader, "DeltaQVAc");
    }
    else
    {
      this->DeltaQVDc = this->DeltaQUDc;
      this->DeltaQVAc = this->DeltaQUAc;
    }
  }
  else
  {
    this->DeltaQUDc = 0;
    this->DeltaQUAc = 0;
    this->DeltaQVDc = 0;
    this->DeltaQVAc = 0;
  }
  this->using_qmatrix = reader.readFlag("using_qmatrix");
  if (this->using_qmatrix)
  {
    this->qm_y = reader.readBits("qm_y", 4);
    this->qm_u = reader.readBits("qm_u", 4);
    if (!seqHeader->color_config.separate_uv_delta_q)
      this->qm_v = qm_u;
    else
      this->qm_v = reader.readBits("qm_v", 4);
  }
}

} // namespace parser::av1