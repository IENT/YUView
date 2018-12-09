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

#ifndef PARSERAVFORMAT_H
#define PARSERAVFORMAT_H

#include "parserBase.h"
#include "parserAnnexB.h"
#include "parserAV1OBU.h"
#include "FFMpegLibrariesHandling.h"
#include "fileSourceFFmpegFile.h"

/* This parser is able to parse the AVPackets and the extradata from containers read with libavformat.
 * If the bitstream within the container is a supported annexB bitstream, this parser can use that parser
 * to even parser deeper.
 */
class parserAVFormat : public parserBase
{
  Q_OBJECT

public:
  parserAVFormat(QObject *parent = nullptr);
  ~parserAVFormat() {}

  QList<QTreeWidgetItem*> getStreamInfo() Q_DECL_OVERRIDE;
  unsigned int getNrStreams() Q_DECL_OVERRIDE { return streamInfoAllStreams.empty() ? 0 : streamInfoAllStreams.length() - 1; }
  
  // This function can run in a separate thread
  bool runParsingOfFile(QString compressedFilePath) Q_DECL_OVERRIDE;

  int getVideoStreamIndex() Q_DECL_OVERRIDE { return videoStreamIndex; }

private:
  AVCodecIDWrapper codecID;

  bool parseExtradata(QByteArray &extradata);
  bool parseMetadata(QStringPairList &metadata);
  bool parseAVPacket(int packetID, AVPacketWrapper &packet);

  struct hvcC_nalUnit
  {
    bool parse_hvcC_nalUnit(int unitID, parserCommon::reader_helper &reader, QScopedPointer<parserAnnexB> &annexBParser);

    unsigned int nalUnitLength;
  };

  struct hvcC_naluArray
  {
    bool parse_hvcC_naluArray(int arrayID, parserCommon::reader_helper &reader, QScopedPointer<parserAnnexB> &annexBParser);

    bool array_completeness;
    bool reserved_flag_false;
    unsigned int NAL_unit_type;
    unsigned int numNalus;
    QList<hvcC_nalUnit> nalList;
  };

  struct hvcC
  {
    bool parse_hvcC(QByteArray &hvcCData, parserCommon::TreeItem *root, QScopedPointer<parserAnnexB> &annexBParser);

    unsigned int configurationVersion;
    unsigned int general_profile_space;
    bool general_tier_flag;
    unsigned int general_profile_idc;
    unsigned int general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    unsigned int general_level_idc;
    unsigned int min_spatial_segmentation_idc;
    unsigned int parallelismType;
    unsigned int chromaFormat;
    unsigned int bitDepthLumaMinus8;
    unsigned int bitDepthChromaMinus8;
    unsigned int avgFrameRate;
    unsigned int constantFrameRate;
    unsigned int numTemporalLayers;
    bool temporalIdNested;
    unsigned int lengthSizeMinusOne;
    unsigned int numOfArrays;

    QList<hvcC_naluArray> naluArrayList;
  };

  // Used for parsing if the packets contain an annexB file that we can parse.
  QScopedPointer<parserAnnexB> annexBParser;
  // Used for parsing if the packets contain an obu file that we can parse.
  QScopedPointer<parserAV1OBU> obuParser;

  bool parseExtradata_generic(QByteArray &extradata);
  bool parseExtradata_AVC(QByteArray &extradata);
  bool parseExtradata_hevc(QByteArray &extradata);
  bool parseExtradata_mpeg2(QByteArray &extradata);

  // The start code pattern to look for in case of a raw format
  QByteArray startCode;

  // When the parser is used in the bitstream analysis window, the runParsingOfFile is used and
  // we update this list while parsing the file.
  QList<QStringPairList> streamInfoAllStreams;

  int videoStreamIndex { -1 };
};

#endif // PARSERAVFORMAT_H
