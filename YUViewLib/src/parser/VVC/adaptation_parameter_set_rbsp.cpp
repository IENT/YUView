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

#include "adaptation_parameter_set_rbsp.h"

namespace parser::vvc
{

namespace
{

const std::map<unsigned, std::pair<adaptation_parameter_set_rbsp::APSParamType, std::string>>
    apsParamTypeMap({{0, {adaptation_parameter_set_rbsp::APSParamType::ALF_APS, "ALF_APS"}},
                     {1, {adaptation_parameter_set_rbsp::APSParamType::LMCS_APS, "LMCS_APS"}},
                     {2,
                      {adaptation_parameter_set_rbsp::APSParamType::SCALING_APS, "SCALING_APS"}}});

} // namespace

using namespace parser::reader;

void adaptation_parameter_set_rbsp::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "adaptation_parameter_set_rbsp");

  auto aps_params_type_ID =
      reader.readBits("aps_params_type",
                      3,
                      Options().withCheckRange({0, 2}).withMeaningVector(
                          {"ALF parameters", "LMCS parameters", "Scaling list parameters"}));
  this->aps_params_type = apsParamTypeMap.at(aps_params_type_ID).first;

  this->aps_adaptation_parameter_set_id =
      reader.readBits("aps_adaptation_parameter_set_id", 5, Options().withCheckRange({0, 7}));
  this->aps_chroma_present_flag = reader.readFlag("aps_chroma_present_flag");
  if (this->aps_params_type == APSParamType::ALF_APS)
  {
    this->alf_data_instance.parse(reader, this);
  }
  else if (this->aps_params_type == APSParamType::LMCS_APS)
  {
    this->lmcs_data_instance.parse(reader, this);
  }
  else if (this->aps_params_type == APSParamType::SCALING_APS)
  {
    this->scaling_list_data_instance.parse(reader, this);
  }
  this->aps_extension_flag = reader.readFlag("aps_extension_flag");
  if (this->aps_extension_flag)
  {
    while (reader.more_rbsp_data())
    {
      this->aps_extension_data_flag = reader.readFlag("aps_extension_data_flag");
    }
  }
  this->rbsp_trailing_bits_instance.parse(reader);
}

} // namespace parser::vvc
