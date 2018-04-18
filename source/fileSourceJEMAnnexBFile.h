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

#ifndef FILESOURCEJEMANNEXBFILE_H
#define FILESOURCEJEMANNEXBFILE_H

#include <QAbstractItemModel>
#include <QMap>
#include "fileSourceAnnexBFile.h"
#include "videoHandlerYUV.h"

using namespace YUV_Internals;

#define BUFFER_SIZE 40960

class fileSourceJEMAnnexBFile : public fileSourceAnnexBFile
{
  Q_OBJECT

public:
  fileSourceJEMAnnexBFile() : fileSourceAnnexBFile() { firstPOCRandomAccess = INT_MAX; }
  ~fileSourceJEMAnnexBFile() {}

  // When the signal signalGetNALUnitInfo is emitted, this function can be used to return information.
  void setNALUnitInfo(int poc, bool isRAP, bool isParameterSet) { nalInfoPoc = poc; nalInfoIsRAP = isRAP; nalInfoIsParameterSet = isParameterSet; }

signals:
  // This class does not know how to interprete any data from within a NAL unit (except for the NAL unit header).
  // So if we want to know more about a NAL unit, we emit this signal and hope that this will result in a call to
  // setNALUnitInfo so that we get some info on the NAL unit.
  void signalGetNALUnitInfo(QByteArray nalData);

protected:

  // All the different NAL unit types (T-REC-H.265-201504 Page 85)
  // Right now these are the same as for HEVC. However, this may change in the future.
  enum nal_unit_type
  {
    TRAIL_N,        TRAIL_R,     TSA_N,          TSA_R,          STSA_N,      STSA_R,      RADL_N,     RADL_R,     RASL_N,     RASL_R, 
    RSV_VCL_N10,    RSV_VCL_N12, RSV_VCL_N14,    RSV_VCL_R11,    RSV_VCL_R13, RSV_VCL_R15, BLA_W_LP,   BLA_W_RADL, BLA_N_LP,   IDR_W_RADL,
    IDR_N_LP,       CRA_NUT,     RSV_IRAP_VCL22, RSV_IRAP_VCL23, RSV_VCL24,   RSV_VCL25,   RSV_VCL26,  RSV_VCL27,  RSV_VCL28,  RSV_VCL29,
    RSV_VCL30,      RSV_VCL31,   VPS_NUT,        SPS_NUT,        PPS_NUT,     AUD_NUT,     EOS_NUT,    EOB_NUT,    FD_NUT,     PREFIX_SEI_NUT,
    SUFFIX_SEI_NUT, RSV_NVCL41,  RSV_NVCL42,     RSV_NVCL43,     RSV_NVCL44,  RSV_NVCL45,  RSV_NVCL46, RSV_NVCL47, UNSPECIFIED
  };

  /* The basic HEVC NAL unit. Additionally to the basic NAL unit, it knows the HEVC nal unit types.
  */
  struct nal_unit_jem : nal_unit
  {
    nal_unit_jem(quint64 filePos, int nal_idx) : nal_unit(filePos, nal_idx), nal_type(UNSPECIFIED) {}
    virtual ~nal_unit_jem() {}

    // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    void parse_nal_unit_header(const QByteArray &parameterSetData, TreeItem *root) Q_DECL_OVERRIDE;

    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return isParameterSetNAL; }

    bool isIRAP();
    bool isSLNR();
    bool isRADL();
    bool isRASL();
    bool isSlice();

    bool isParameterSetNAL;
    int poc;
    
    // The information of the NAL unit header
    nal_unit_type nal_type;
    int nuh_layer_id;
    int nuh_temporal_id_plus1;
  };

  void parseAndAddNALUnit(int nalID) Q_DECL_OVERRIDE;
  
  int nalInfoPoc;
  bool nalInfoIsRAP;
  bool nalInfoIsParameterSet;

};

#endif //FILESOURCEJEMANNEXBFILE_H
