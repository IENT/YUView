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

#include "NalUnit.h"

#include "parser/common/parserMacros.h"
#include "parser/common/ReaderHelper.h"

#define PARSER_VVC_NAL_DEBUG_OUTPUT 1
#if PARSER_VVC_NAL_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VVC_NAL(msg) qDebug() << msg
#else
#define DEBUG_VVC_NAL(msg) ((void)0)
#endif

namespace VVC
{

QByteArray NalUnit::getNALHeader() const
{ 
  int out = ((int)this->nal_unit_type_id << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[2] = { (char)(out >> 8), (char)out };
  return QByteArray(c, 2);
}

bool NalUnit::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  ReaderHelper reader(parameterSetData, root, "nal_unit_header()");

  READZEROBITS(1, "forbidden_zero_bit");
  READZEROBITS(1, "nuh_reserved_zero_bit");
  READBITS(this->nuh_layer_id, 6);
  // Read nal unit type

  QStringList nal_unit_type_id_meaning = QStringList()
    << "TRAIL_NUT Coded slice of a trailing picture or subpicture*"
    << "STSA_NUT Coded slice of an STSA picture or subpicture*"
    << "RADL_NUT Coded slice of a RADL picture or subpicture*"
    << "RASL_NUT Coded slice of a RASL picture or subpicture*"
    << "RSV_VCL_4 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL_5 Reserved non-IRAP VCL NAL unit types"
    << "RSV_VCL_6 Reserved non-IRAP VCL NAL unit types"
    << "IDR_W_RADL Coded slice of an IDR picture or subpicture*"
    << "IDR_N_LP Coded slice of an IDR picture or subpicture*"
    << "CRA_NUT Coded slice of a CRA picture or subpicture*"
    << "GDR_NUT Coded slice of a GDR picture or subpicture*"
    << "RSV_IRAP_11 Reserved IRAP VCL NAL unit types"
    << "RSV_IRAP_12 Reserved IRAP VCL NAL unit types"
    << "DCI_NUT Decoding capability information"
    << "VPS_NUT Video parameter set"
    << "SPS_NUT Sequence parameter set"
    << "PPS_NUT Picture parameter set"
    << "PREFIX_APS_NUT Adaptation parameter set"
    << "SUFFIX_APS_NUT Adaptation parameter set"
    << "PH_NUT Picture header"
    << "AUD_NUT AU delimiter"
    << "EOS_NUT End of sequence"
    << "EOB_NUT End of bitstream"
    << "PREFIX_SEI_NUT Supplemental enhancement information"
    << "SUFFIX_SEI_NUT Supplemental enhancement information"
    << "FD_NUT Filler data"
    << "RSV_NVCL_26 Reserved non-VCL NAL unit types"
    << "RSV_NVCL_27	Reserved non-VCL NAL unit types"
    << "UNSPEC_28	Unspecified non-VCL NAL unit types"
    << "UNSPEC_29	Unspecified non-VCL NAL unit types"
    << "UNSPEC_30	Unspecified non-VCL NAL unit types"
    << "UNSPEC_31	Unspecified non-VCL NAL unit types";
  READBITS_M(this->nal_unit_type_id, 5, nal_unit_type_id_meaning);
  READBITS(this->nuh_temporal_id_plus1, 3);

  this->nal_unit_type = (nal_unit_type_id > UNSPECIFIED) ? UNSPECIFIED : (NalUnitType)this->nal_unit_type_id;
  DEBUG_VVC_NAL("VVC Nal type " << this->nal_unit_type_id);

  return true;
}

bool NalUnit::isSlice() const
{
  static const auto sliceNalTypes = QList<NalUnitType>() << TRAIL_NUT << STSA_NUT << RADL_NUT << RASL_NUT << IDR_W_RADL << IDR_N_LP << CRA_NUT << GDR_NUT;
  return sliceNalTypes.contains(this->nal_unit_type);
}

} // namespace VVC