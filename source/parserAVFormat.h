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

#ifndef PARSERAVFORMAT_H
#define PARSERAVFORMAT_H

#include "parserBase.h"
#include "parserAnnexB.h"
#include "FFMpegLibrariesHandling.h"

using namespace FFmpeg;

/* This parser is able to parse the AVPackets and the extradata from containers read with libavformat.
 * If the bitstream within the container is a supported annexB bitstream, this parser can use that parser
 * to even parser deeper.
 */
class parserAVFormat : public parserBase
{
public:
  parserAVFormat(AVCodecID codec);
  ~parserAVFormat() {}

  void parseExtradata(QByteArray &extradata);
  void parseAVPacket(int packetID, AVPacketWrapper &packet);

private:
  AVCodecID codecID;

  struct hvcC_nalUnit
  {
    void parse_hvcC_nalUnit(int unitID, sub_byte_reader &reader, TreeItem *root, QScopedPointer<parserAnnexB> &annexBParser);

    int nalUnitLength;
  };

  struct hvcC_naluArray
  {
    void parse_hvcC_naluArray(int arrayID, sub_byte_reader &reader, TreeItem *root, QScopedPointer<parserAnnexB> &annexBParser);

    bool array_completeness;
    bool reserved_flag_false;
    int NAL_unit_type;
    int numNalus;
    QList<hvcC_nalUnit> nalList;
  };

  struct hvcC
  {
    void parse_hvcC(QByteArray &hvcCData, TreeItem *root, QScopedPointer<parserAnnexB> &annexBParser);

    int configurationVersion;
    int general_profile_space;
    bool general_tier_flag;
    int general_profile_idc;
    int general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    int general_level_idc;
    int min_spatial_segmentation_idc;
    int parallelismType;
    int chromaFormat;
    int bitDepthLumaMinus8;
    int bitDepthChromaMinus8;
    int avgFrameRate;
    int constantFrameRate;
    int numTemporalLayers;
    bool temporalIdNested;
    int lengthSizeMinusOne;
    int numOfArrays;

    QList<hvcC_naluArray> naluArrayList;
  };

  // Used for parsing if the packets contain an annexB file that we can parse.
  QScopedPointer<parserAnnexB> annexBParser;

  void parseExtradata_generic(QByteArray &extradata);
  void parseExtradata_AVC(QByteArray &extradata);
  void parseExtradata_hevc(QByteArray &extradata);
};

#endif // PARSERAVFORMAT_H
