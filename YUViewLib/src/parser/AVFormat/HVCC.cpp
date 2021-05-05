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

#include "HVCC.h"

namespace parser::avformat
{

using namespace reader;

void HVCCNalUnit::parse(unsigned              unitID,
                        SubByteReaderLogging &reader,
                        AnnexBHEVC *          hevcParser,
                        BitratePlotModel *    bitrateModel)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal unit " + std::to_string(unitID));

  this->nalUnitLength = reader.readBits("nalUnitLength", 16);

  // Get the bytes of the raw nal unit to pass to the "real" hevc parser
  auto nalData = reader.readBytes("", nalUnitLength, Options().withLoggingDisabled());

  // Let the hevc annexB parser parse this
  auto parseResult =
      hevcParser->parseAndAddNALUnit(unitID, nalData, {}, {}, reader.getCurrentItemTree());
  if (parseResult.success && bitrateModel != nullptr && parseResult.bitrateEntry)
    bitrateModel->addBitratePoint(0, *parseResult.bitrateEntry);
}

void HVCCNalArray::parse(unsigned              arrayID,
                         SubByteReaderLogging &reader,
                         AnnexBHEVC *          hevcParser,
                         BitratePlotModel *    bitrateModel)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal unit array " + std::to_string(arrayID));

  // The next 3 bytes contain info about the array
  this->array_completeness = reader.readFlag("array_completeness");
  reader.readFlag("reserved_flag_false", Options().withCheckEqualTo(0));
  this->nal_unit_type = reader.readBits("nal_unit_type", 6);
  this->numNalus      = reader.readBits("numNalus", 16);

  for (unsigned i = 0; i < numNalus; i++)
  {
    HVCCNalUnit nal;
    nal.parse(i, reader, hevcParser, bitrateModel);
    nalList.push_back(nal);
  }
}

void HVCC::parse(ByteVector &              data,
                 std::shared_ptr<TreeItem> root,
                 AnnexBHEVC *              hevcParser,
                 BitratePlotModel *        bitrateModel)
{
  SubByteReaderLogging reader(data, root, "Extradata (HEVC hvcC format)");
  reader.disableEmulationPrevention();

  // The first 22 bytes are the hvcC header
  this->configurationVersion =
      reader.readBits("configurationVersion", 8, Options().withCheckEqualTo(1));
  this->general_profile_space = reader.readBits("general_profile_space", 2);
  this->general_tier_flag     = reader.readFlag("general_tier_flag");
  this->general_profile_idc   = reader.readBits("general_profile_idc", 5);
  this->general_profile_compatibility_flags =
      reader.readBits("general_profile_compatibility_flags", 32);
  this->general_constraint_indicator_flags =
      reader.readBits("general_constraint_indicator_flags", 48);
  this->general_level_idc = reader.readBits("general_level_idc", 8);
  reader.readBits("reserved_4onebits", 4, Options().withCheckEqualTo(15));
  this->min_spatial_segmentation_idc = reader.readBits("min_spatial_segmentation_idc", 12);
  reader.readBits("reserver_6onebits", 6, Options().withCheckEqualTo(63));
  this->parallelismType =
      reader.readBits("parallelismType",
                      2,
                      Options().withMeaningVector({"mixed-type parallel decoding",
                                                   "slice-based parallel decoding",
                                                   "tile-based parallel decoding",
                                                   "wavefront-based parallel decoding"}));
  reader.readBits("reserver_6onebits", 6, Options().withCheckEqualTo(63));
  this->chromaFormat = reader.readBits("chromaFormat", 2);
  reader.readBits("reserved_5onebits", 5, Options().withCheckEqualTo(31));
  this->bitDepthLumaMinus8 = reader.readBits("bitDepthLumaMinus8", 3);
  reader.readBits("reserved_5onebits", 5, Options().withCheckEqualTo(31));
  this->bitDepthChromaMinus8 = reader.readBits("bitDepthChromaMinus8", 3);
  this->avgFrameRate         = reader.readBits("avgFrameRate", 16);
  this->constantFrameRate    = reader.readBits("constantFrameRate", 2);
  this->numTemporalLayers    = reader.readBits("numTemporalLayers", 3);
  this->temporalIdNested     = reader.readFlag("temporalIdNested");
  this->lengthSizeMinusOne   = reader.readBits("lengthSizeMinusOne", 2);
  this->numOfArrays          = reader.readBits("numOfArrays", 8);

  // Now parse the contained raw NAL unit arrays
  for (unsigned i = 0; i < this->numOfArrays; i++)
  {
    HVCCNalArray a;
    a.parse(i, reader, hevcParser, bitrateModel);
    this->naluArrays.push_back(a);
  }
}

} // namespace parser::avformat