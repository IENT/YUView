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

#ifndef PARSERANNEXBMPEG2_H
#define PARSERANNEXBMPEG2_H

#include "parserAnnexB.h"

class parserAnnexBMpeg2 : public parserAnnexB
{
public:
  parserAnnexBMpeg2();
  ~parserAnnexBMpeg2() {};

  // Get properties
  double getFramerate() const Q_DECL_OVERRIDE { return -1; }
  QSize getSequenceSizeSamples() const Q_DECL_OVERRIDE { return QSize(); }
  yuvPixelFormat getPixelFormat() const Q_DECL_OVERRIDE { return yuvPixelFormat(); }

  void parseAndAddNALUnit(int nalID, QByteArray data, TreeItem *parent=nullptr, QUint64Pair nalStartEndPosFile = QUint64Pair(-1,-1), QString *nalTypeName=nullptr) Q_DECL_OVERRIDE;

  QList<QByteArray> getSeekFrameParamerSets(int iFrameNr, uint64_t &filePos) Q_DECL_OVERRIDE { return QList<QByteArray>(); }
  QByteArray getExtradata() Q_DECL_OVERRIDE { return QByteArray(); }
  QPair<int,int> getProfileLevel() Q_DECL_OVERRIDE { return QPair<int,int>(); }
  QPair<int,int> getSampleAspectRatio() Q_DECL_OVERRIDE { return QPair<int,int>(); }

private:

  // All the different NAL unit types (T-REC-H.262-199507 Page 24 Table 6-1)
  enum nal_unit_type_enum
  {
    UNSPECIFIED,
    PICTURE,
    SLICE,
    USER_DATA,
    SEQUENCE_HEADER,
    SEQUENCE_ERROR,
    EXTENSION_START,
    SEQUENCE_END,
    GROUP_START,
    SYSTEM_START_CODE,
    RESERVED
  };

  /* The basic Mpeg2 NAL unit. Technically, there is no concept of NAL units in mpeg2 (h262) but there are start codes for some units
   * and there is a start code so we internally use the NAL concept.
   */
  struct nal_unit_mpeg2 : nal_unit
  {
    nal_unit_mpeg2(QUint64Pair filePosStartEnd, int nal_idx) : nal_unit(filePosStartEnd, nal_idx), nal_unit_type(UNSPECIFIED), slice_id(-1), system_start_codes(-1), start_code_value(-1) {}
    nal_unit_mpeg2(QSharedPointer<nal_unit_mpeg2> nal_src) : nal_unit(nal_src->filePosStartEnd, nal_src->nal_idx) { interprete_start_code(nal_src->nal_unit_type); }
    virtual ~nal_unit_mpeg2() {}

    // Parse the parameter set from the given data bytes. If a TreeItem pointer is provided, the values will be added to the tree as well.
    void parse_nal_unit_header(const QByteArray &header_byte, TreeItem *root) Q_DECL_OVERRIDE;

    virtual QByteArray getNALHeader() const override;
    virtual bool isParameterSet() const override { return nal_unit_type == SEQUENCE_HEADER; }

    QString interprete_start_code(int start_code);

    nal_unit_type_enum nal_unit_type;
    int slice_id;   // in case of SLICE
    int system_start_codes; // in case of SYSTEM_START_CODE
    int start_code_value; 
  };

  struct sequence_header : nal_unit_mpeg2
  {
    sequence_header(const nal_unit_mpeg2 &nal);
    void parse_sequence_header(const QByteArray &parameterSetData, TreeItem *root);
  };

};

#endif // PARSERANNEXBMPEG2_H