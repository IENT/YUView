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

#include "global_motion_params.h"

#include "typedef.h"

namespace parser::av1
{

using namespace reader;

namespace
{

int inverse_recenter(int r, int v)
{
  if (v > 2 * r)
    return v;
  else if (v & 1)
    return r - ((v + 1) >> 1);
  else
    return r + (v >> 1);
}

int decode_subexp(SubByteReaderLogging &reader, int numSyms)
{
  auto i  = 0;
  auto mk = 0;
  auto k  = 3;
  while (1)
  {
    auto b2 = i ? k + i - 1 : k;
    auto a  = 1 << b2;
    if (numSyms <= mk + 3 * a)
    {
      auto subexp_final_bits = reader.readNS("subexp_final_bits", numSyms - mk);
      return subexp_final_bits + mk;
    }
    else
    {
      auto subexp_more_bits = reader.readFlag("subexp_more_bits");
      if (subexp_more_bits)
      {
        i++;
        mk += a;
      }
      else
      {
        auto subexp_bits = reader.readBits("subexp_bits", b2);
        return subexp_bits + mk;
      }
    }
  }
}

int decode_unsigned_subexp_with_ref(SubByteReaderLogging &reader, int mx, int r)
{
  auto v = decode_subexp(reader, mx);
  if ((r << 1) <= mx)
  {
    return inverse_recenter(r, v);
  }
  else
  {
    return mx - 1 - inverse_recenter(mx - 1 - r, v);
  }
}

int decode_signed_subexp_with_ref(SubByteReaderLogging &reader, int low, int high, int r)
{
  auto x = decode_unsigned_subexp_with_ref(reader, high - low, r - low);
  return x + low;
}

} // namespace

void global_motion_params::parse(SubByteReaderLogging &reader,
                                 bool                  FrameIsIntra,
                                 bool                  allow_high_precision_mv,
                                 unsigned              PrevGmParams[8][6])
{
  SubByteReaderLoggingSubLevel subLevel(reader, "global_motion_params()");

  for (unsigned ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++)
  {
    this->GmType[ref] = MotionType::IDENTITY;
    for (unsigned i = 0; i < 6; i++)
    {
      this->gm_params[ref][i] = ((i % 3 == 2) ? 1u << WARPEDMODEL_PREC_BITS : 0u);
    }
  }
  if (FrameIsIntra)
    return;

  for (unsigned ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++)
  {
    this->is_global = reader.readFlag("is_global");
    if (this->is_global)
    {
      this->is_rot_zoom = reader.readFlag("is_rot_zoom");
      if (this->is_rot_zoom)
      {
        this->type = MotionType::ROTZOOM;
      }
      else
      {
        this->is_translation = reader.readFlag("is_translation");
        this->type           = this->is_translation ? MotionType::TRANSLATION : MotionType::AFFINE;
      }
    }
    else
    {
      this->type = MotionType::IDENTITY;
    }
    this->GmType[ref] = type;
    if (this->type >= MotionType::ROTZOOM)
    {
      this->read_global_param(reader, this->type, ref, 2, allow_high_precision_mv, PrevGmParams);
      this->read_global_param(reader, this->type, ref, 3, allow_high_precision_mv, PrevGmParams);
      if (type == MotionType::AFFINE)
      {
        this->read_global_param(reader, this->type, ref, 4, allow_high_precision_mv, PrevGmParams);
        this->read_global_param(reader, this->type, ref, 5, allow_high_precision_mv, PrevGmParams);
      }
      else
      {
        this->gm_params[ref][4] = -this->gm_params[ref][3];
        this->gm_params[ref][5] = this->gm_params[ref][2];
      }
    }
    if (this->type >= MotionType::TRANSLATION)
    {
      this->read_global_param(reader, this->type, ref, 0, allow_high_precision_mv, PrevGmParams);
      this->read_global_param(reader, this->type, ref, 1, allow_high_precision_mv, PrevGmParams);
    }
  }
}

void global_motion_params::read_global_param(SubByteReaderLogging &reader,
                                             MotionType            type,
                                             unsigned              ref,
                                             unsigned              idx,
                                             bool                  allow_high_precision_mv,
                                             unsigned              PrevGmParams[8][6])
{
  auto absBits  = GM_ABS_ALPHA_BITS;
  auto precBits = GM_ALPHA_PREC_BITS;
  if (idx < 2)
  {
    if (type == MotionType::TRANSLATION)
    {
      absBits  = GM_ABS_TRANS_ONLY_BITS - (!allow_high_precision_mv ? 1 : 0);
      precBits = GM_TRANS_ONLY_PREC_BITS - (!allow_high_precision_mv ? 1 : 0);
    }
    else
    {
      absBits  = GM_ABS_TRANS_BITS;
      precBits = GM_TRANS_PREC_BITS;
    }
  }
  auto precDiff = WARPEDMODEL_PREC_BITS - precBits;
  auto round    = (idx % 3) == 2 ? (1 << WARPEDMODEL_PREC_BITS) : 0;
  auto sub      = (idx % 3) == 2 ? (1 << precBits) : 0;
  auto mx       = (1 << absBits);
  auto r        = (PrevGmParams[ref][idx] >> precDiff) - sub;
  this->gm_params[ref][idx] =
      (decode_signed_subexp_with_ref(reader, -mx, mx + 1, r) << precDiff) + round;
}

} // namespace parser::av1