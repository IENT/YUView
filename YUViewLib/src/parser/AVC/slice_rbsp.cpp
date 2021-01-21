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

#include "slice_rbsp.h"

#include "seq_parameter_set_rbsp.h"
#include "parser/common/SubByteReaderLogging.h"
#include "slice_header.h"

namespace parser::avc
{

using namespace reader;

void slice_layer_without_partitioning_rbsp::parse(SubByteReaderLogging &reader,
                                                  SPSMap &                      spsMap,
                                                  PPSMap &                      ppsMap,
                                                  NalType                       nal_unit_type,
                                                  unsigned                      nal_ref_idc,
                                                  std::shared_ptr<slice_header> prev_pic)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_layer_without_partitioning_rbsp()");

  this->sliceHeader = std::make_shared<slice_header>();
  this->sliceHeader->parse(reader, spsMap, ppsMap, nal_unit_type, nal_ref_idc, prev_pic);
  // slice_data( ) /* all categories of slice_data( ) syntax */
  // rbsp_slice_trailing_bits( )
}
void slice_data_partition_a_layer_rbsp::parse(SubByteReaderLogging &reader,
                                              SPSMap &                      spsMap,
                                              PPSMap &                      ppsMap,
                                              NalType                       nal_unit_type,
                                              unsigned                      nal_ref_idc,
                                              std::shared_ptr<slice_header> prev_pic)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_data_partition_a_layer_rbsp()");

  this->sliceHeader = std::make_shared<slice_header>();
  this->sliceHeader->parse(reader, spsMap, ppsMap, nal_unit_type, nal_ref_idc, prev_pic);
  this->slice_id = reader.readUEV("slice_id");
  // slice_data( ) /* only category 2 parts of slice_data( ) syntax */
  // rbsp_slice_trailing_bits( )
}
void slice_data_partition_b_layer_rbsp::parse(SubByteReaderLogging &          reader,
                                              std::shared_ptr<seq_parameter_set_rbsp> partASPS)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_data_partition_b_layer_rbsp()");

  this->slice_id = reader.readUEV("slice_id");
  if (partASPS->seqParameterSetData.separate_colour_plane_flag)
    this->colour_plane_id = reader.readBits("colour_plane_id", 2);
  else
    this->redundant_pic_cnt = reader.readUEV("redundant_pic_cnt");
  // slice_data( ) /* only category 3 parts of slice_data( ) syntax */
  // rbsp_slice_trailing_bits( )
}
void slice_data_partition_c_layer_rbsp::parse(SubByteReaderLogging &          reader,
                                              std::shared_ptr<seq_parameter_set_rbsp> partASPS)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "slice_data_partition_c_layer_rbsp()");

  this->slice_id = reader.readUEV("slice_id");
  if (partASPS->seqParameterSetData.separate_colour_plane_flag)
    this->colour_plane_id = reader.readBits("colour_plane_id", 2);
  else
    this->redundant_pic_cnt = reader.readUEV("redundant_pic_cnt");
  // slice_data( ) /* only category 4 parts of slice_data( ) syntax */
  // rbsp_slice_trailing_bits( )
}

} // namespace parser::avc
