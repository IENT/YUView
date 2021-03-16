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

#pragma once

#include "../AV1/AV1OBU.h"
#include "../AnnexB.h"
#include "../Base.h"
#include "ffmpeg/FFMpegLibrariesHandling.h"
#include "filesource/FileSourceFFmpegFile.h"

#include <queue>

namespace parser
{

/* This parser is able to parse the AVPackets and the extradata from containers read with
 * libavformat. If the bitstream within the container is a supported annexB bitstream, this parser
 * can use that parser to even parser deeper.
 */
class AVFormat : public Base
{
  Q_OBJECT

public:
  AVFormat(QObject *parent = nullptr) : Base(parent) {}
  ~AVFormat() {}

  QList<QTreeWidgetItem *> getStreamInfo() override;
  unsigned int             getNrStreams() override
  {
    return streamInfoAllStreams.empty() ? 0 : streamInfoAllStreams.length() - 1;
  }
  QString getShortStreamDescription(int streamIndex) const override;

  // This function can run in a separate thread
  bool runParsingOfFile(QString compressedFilePath) override;

  int getVideoStreamIndex() override { return videoStreamIndex; }

private:
  AVCodecIDWrapper codecID;

  bool parseExtradata(ByteVector &extradata);
  bool parseMetadata(QStringPairList &metadata);
  bool parseAVPacket(unsigned packetID, unsigned streamPacketID, AVPacketWrapper &packet);

  // Used for parsing if the packets contain an annexB file that we can parse.
  std::unique_ptr<AnnexB> annexBParser;
  // Used for parsing if the packets contain an obu file that we can parse.
  std::unique_ptr<ParserAV1OBU> obuParser;

  bool parseExtradata_generic(ByteVector &extradata);
  bool parseExtradata_AVC(ByteVector &extradata);
  bool parseExtradata_hevc(ByteVector &extradata);
  bool parseExtradata_mpeg2(ByteVector &extradata);

  // Parse all NAL units in data using the AnnexB parser
  std::map<std::string, unsigned>
  parseByteVectorAnnexBStartCodes(ByteVector &                   data,
                                  packetDataFormat_t             dataFormat,
                                  BitratePlotModel::BitrateEntry packetBitrateEntry,
                                  TreeItem *                     item);

  // When the parser is used in the bitstream analysis window, the runParsingOfFile is used and
  // we update this list while parsing the file.
  QList<QStringPairList> streamInfoAllStreams;
  QList<AVRational>      timeBaseAllStreams;
  QList<QString>         shortStreamInfoAllStreams;

  int    videoStreamIndex{-1};
  double framerate{-1.0};
};

} // namespace parser