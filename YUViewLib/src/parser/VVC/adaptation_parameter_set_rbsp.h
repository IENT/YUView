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

#include "NalUnitVVC.h"
#include "alf_data.h"
#include "lmcs_data.h"
#include "parser/common/SubByteReaderLogging.h"
#include "rbsp_trailing_bits.h"
#include "scaling_list_data.h"


namespace parser::vvc
{

class adaptation_parameter_set_rbsp : public NalRBSP
{
public:
  adaptation_parameter_set_rbsp()  = default;
  ~adaptation_parameter_set_rbsp() = default;
  void parse(reader::SubByteReaderLogging &reader);

  enum class APSParamType
  {
    ALF_APS,
    LMCS_APS,
    SCALING_APS
  };

  APSParamType       aps_params_type{};
  unsigned           aps_adaptation_parameter_set_id{};
  bool               aps_chroma_present_flag{};
  alf_data           alf_data_instance;
  lmcs_data          lmcs_data_instance;
  scaling_list_data  scaling_list_data_instance;
  bool               aps_extension_flag{};
  bool               aps_extension_data_flag{};
  rbsp_trailing_bits rbsp_trailing_bits_instance;
};

} // namespace parser::vvc
