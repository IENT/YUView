/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
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

#include "fileSourceJEMAnnexBFile.h"

#define READFLAG(into) {into=(reader.readBits(1)!=0); if (itemTree) new TreeItem(#into,into,QString("u(1)"),(into!=0)?"1":"0",itemTree);}
#define READBITS(into,numBits) {QString code; into=reader.readBits(numBits, &code); if (itemTree) new TreeItem(#into,into,QString("u(v) -> u(%1)").arg(numBits),code, itemTree);}

void fileSourceJEMAnnexBFile::parseAndAddNALUnit(int nalID)
{
  // Reset the values before emitting
  nalInfoPoc = -1;
  nalInfoIsRAP = false;
  nalInfoIsParameterSet = false;

  // Save the position of the first byte of the start code
  quint64 curFilePos = tell() - 3;

  // Read two bytes (the nal header)
  QByteArray nalHeaderBytes;
  nalHeaderBytes.append(getCurByte());
  gotoNextByte();
  nalHeaderBytes.append(getCurByte());
  gotoNextByte();

  // Create a new TreeItem root for the NAL unit.
  TreeItem *nalRoot = nullptr;
  if (!nalUnitModel.rootItem.isNull())
    nalRoot = new TreeItem(nalUnitModel.rootItem.data());

  auto nal_jem = QSharedPointer<nal_unit_jem>(new nal_unit_jem(curFilePos, nalID));
  nal_jem->parse_nal_unit_header(nalHeaderBytes, nalRoot);

  // Get the NAL as raw bytes and emit the signal to get some information on the NAL.
  nal_jem->nalPayload = getRemainingNALBytes();
  emit signalGetNALUnitInfo(nal_jem->getRawNALData());

  // If this NAL will generate an output POC, save the POC number.
  if (nalInfoPoc >= 0)
    addPOCToList(nalInfoPoc);

  // We do not know much about NAL units. Save all parameter sets and random access points.
  if (nalInfoIsParameterSet || nalInfoIsRAP)
  {
    nal_jem->isParameterSetNAL = nalInfoIsParameterSet;
    if (nalInfoIsRAP)
    {
      Q_ASSERT_X(!nal_jem->isParameterSet(), "fileSourceAnnexBFile::parseAndAddNALUnit", "NAL can not be RAP and parameter set at the same time.");
      nal_jem->poc = nalInfoPoc;
      // For a random access point (a slice) we don't need to save the raw payload.
      nal_jem->nalPayload.clear();
    }

    nalUnitList.append(nal_jem);
  }

  if (nalRoot)
    // Set a useful name of the TreeItem (the root for this NAL)
    nalRoot->itemData.append(QString("NAL %1").arg(nal_jem->nal_idx));
}

void fileSourceJEMAnnexBFile::nal_unit_jem::parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root)
{
  // Create a sub byte parser to access the bits
  sub_byte_reader reader(parameterSetData);

  // Create a new TreeItem root for the item
  // The macros will use this variable to add all the parsed variables
  TreeItem *const itemTree = root ? new TreeItem("nal_unit_header()", root) : nullptr;

  // Read forbidden_zeor_bit
  int forbidden_zero_bit;
  READFLAG(forbidden_zero_bit);
  if (forbidden_zero_bit != 0)
    throw std::logic_error("The nal unit header forbidden zero bit was not zero.");

  // Read nal unit type
  READBITS(nal_unit_type_id, 6);
  READBITS(nuh_layer_id, 6);
  READBITS(nuh_temporal_id_plus1, 3);

  // Set the nal unit type
  nal_type = (nal_unit_type_id > UNSPECIFIED || nal_unit_type_id < 0) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;
}

bool fileSourceJEMAnnexBFile::nal_unit_jem::isIRAP()
{ 
  return (nal_type == BLA_W_LP       || nal_type == BLA_W_RADL ||
          nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
          nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
          nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23); 
}

bool fileSourceJEMAnnexBFile::nal_unit_jem::isSLNR() 
{ 
  return (nal_type == TRAIL_N     || nal_type == TSA_N       ||
          nal_type == STSA_N      || nal_type == RADL_N      ||
          nal_type == RASL_N      || nal_type == RSV_VCL_N10 ||
          nal_type == RSV_VCL_N12 || nal_type == RSV_VCL_N14); 
}

bool fileSourceJEMAnnexBFile::nal_unit_jem::isRADL() { 
  return (nal_type == RADL_N || nal_type == RADL_R); 
}

bool fileSourceJEMAnnexBFile::nal_unit_jem::isRASL() 
{ 
  return (nal_type == RASL_N || nal_type == RASL_R); 
}

bool fileSourceJEMAnnexBFile::nal_unit_jem::isSlice() 
{ 
  return (nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT  ||
          nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ||
          nal_type == TRAIL_N    || nal_type == TRAIL_R    || nal_type == TSA_N    ||
          nal_type == TSA_R      || nal_type == STSA_N     || nal_type == STSA_R   ||
          nal_type == RADL_N     || nal_type == RADL_R     || nal_type == RASL_N   ||
          nal_type == RASL_R); 
}

QByteArray fileSourceJEMAnnexBFile::nal_unit_jem::getNALHeader() const
{ 
  int out = ((int)nal_unit_type_id << 9) + (nuh_layer_id << 3) + nuh_temporal_id_plus1;
  char c[6] = { 0, 0, 0, 1,  (char)(out >> 8), (char)out };
  return QByteArray(c, 6);
}