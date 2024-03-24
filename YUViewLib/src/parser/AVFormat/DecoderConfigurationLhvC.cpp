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

#include "DecoderConfigurationLhvC.h"

namespace parser::avformat
{

using namespace reader;

void DecoderConfigurationLhvC::parse(const ByteVector &        data,
                                     std::shared_ptr<TreeItem> root,
                                     ParserAnnexBHEVC *        hevcParser,
                                     BitratePlotModel *        bitrateModel)
{
  SubByteReaderLogging reader(data, root, "Extradata (HEVC lhvC format)");
  reader.disableEmulationPrevention();

  // ISO/IEC 14496-15, 9.6.3
  this->configurationVersion =
      reader.readBits("configurationVersion", 8, Options().withCheckEqualTo(1));
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
  reader.readBits("reserver_2onebits", 2, Options().withCheckEqualTo(3));
  this->numTemporalLayers  = reader.readBits("numTemporalLayers", 3);
  this->temporalIdNested   = reader.readFlag("temporalIdNested");
  this->lengthSizeMinusOne = reader.readBits("lengthSizeMinusOne", 2);
  this->numOfArrays        = reader.readBits("numOfArrays", 8);

  // Now parse the contained raw NAL unit arrays
  for (unsigned i = 0; i < this->numOfArrays; i++)
  {
    DecoderConfigurationNALArray array;
    array.parse(i, reader, hevcParser, bitrateModel);
    this->naluArrays.push_back(array);
  }
}

} // namespace parser::avformat
