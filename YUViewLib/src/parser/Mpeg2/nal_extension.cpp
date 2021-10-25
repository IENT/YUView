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

#include "nal_extension.h"

#include "parser/common/CodingEnum.h"
#include "parser/common/SubByteReaderLogging.h"
#include <parser/common/functions.h>
#include "picture_coding_extension.h"
#include "sequence_extension.h"

namespace
{

using namespace parser::mpeg2;

parser::CodingEnum<ExtensionType> extensionTypeCoding(
    {{0, ExtensionType::EXT_SEQUENCE, "sequence_extension()"},
     {1, ExtensionType::EXT_SEQUENCE_DISPLAY, "sequence_display_extension()"},
     {2, ExtensionType::EXT_QUANT_MATRIX, "quant_matrix_extension()"},
     {3, ExtensionType::EXT_COPYRIGHT, "copyright_extension()"},
     {4, ExtensionType::EXT_SEQUENCE_SCALABLE, "sequence_scalable_extension()"},
     {5, ExtensionType::EXT_PICTURE_DISPLAY, "picture_display_extension()"},
     {6, ExtensionType::EXT_PICTURE_CODING, "picture_coding_extension()"},
     {7, ExtensionType::EXT_PICTURE_SPATICAL_SCALABLE, "picture_spatial_scalable_extension()"},
     {8, ExtensionType::EXT_PICTURE_TEMPORAL_SCALABLE, "picture_temporal_scalable_extension()"},
     {9, ExtensionType::EXT_RESERVED, "Reserved"}},
    ExtensionType::EXT_RESERVED);

} // namespace

namespace parser::mpeg2
{

using namespace parser::reader;

void nal_extension::parse(SubByteReaderLogging &reader)
{
  SubByteReaderLoggingSubLevel subLevel(reader, "nal_extension");

  this->extension_start_code_identifier = reader.readBits(
      "extension_start_code_identifier", 4, Options().withMeaningMap(extensionTypeCoding.getMeaningMap()));
  this->extension_type = extensionTypeCoding.getValue(this->extension_start_code_identifier);

  if (this->extension_type == ExtensionType::EXT_SEQUENCE)
  {
    auto newSequenceExtension = std::make_shared<sequence_extension>();
    newSequenceExtension->parse(reader);

    this->payload = newSequenceExtension;
  }
  else if (this->extension_type == ExtensionType::EXT_PICTURE_CODING)
  {
    auto newPictureCodingExtension = std::make_shared<picture_coding_extension>();
    newPictureCodingExtension->parse(reader);

    this->payload = newPictureCodingExtension;
  }
}

std::string nal_extension::get_extension_function_name()
{
  return extensionTypeCoding.getMeaning(this->extension_type);
}

} // namespace parser::mpeg2
