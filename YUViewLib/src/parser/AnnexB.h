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

#include <QList>
#include <QTreeWidgetItem>

#include <optional>
#include <set>

#include "common/BitratePlotModel.h"
#include "common/TreeItem.h"
#include "filesource/FileSourceAnnexBFile.h"
#include "parser/Base.h"
#include "video/videoHandlerYUV.h"

namespace parser
{

using FrameIndexDisplayOrder = unsigned;
using FrameIndexCodingOrder  = unsigned;

/* The (abstract) base class for the various types of AnnexB files (AVC, HEVC, VVC) that we can
 * parse.
 */
class AnnexB : public Base
{
  Q_OBJECT

public:
  AnnexB(QObject *parent = nullptr) : Base(parent){};
  virtual ~AnnexB(){};

  // How many POC's have been found in the file
  size_t getNumberPOCs() const { return this->frameListCodingOrder.size(); }

  // Clear all knowledge about the bitstream.
  void clearData();

  [[nodiscard]] StreamsInfo getStreamsInfo() const override;
  [[nodiscard]] int         getNrStreams() const override { return 1; }
  QString                   getShortStreamDescription(int streamIndex) const override;

  // Get some format properties
  virtual double                     getFramerate() const           = 0;
  virtual Size                       getSequenceSizeSamples() const = 0;
  virtual video::yuv::PixelFormatYUV getPixelFormat() const         = 0;

  // When we want to seek to a specific frame number, this function return the parameter sets that
  // you need to start decoding (without start codes). If file positions were set for the NAL units,
  // the file position where decoding can begin will also be returned.
  struct SeekData
  {
    std::vector<ByteVector> parameterSets;
    std::optional<uint64_t> filePos;
  };
  virtual std::optional<SeekData> getSeekData(int iFrameNr) = 0;

  // Look through the random access points and find the closest one before (or equal)
  // the given frameIdx where we can start decoding
  // frameIdx: The frame index in display order that we want to seek to
  struct SeekPointInfo
  {
    FrameIndexDisplayOrder frameIndex{};
    unsigned               frameDistanceInCodingOrder{};
  };
  auto getClosestSeekPoint(FrameIndexDisplayOrder targetFrame, FrameIndexDisplayOrder currentFrame)
      -> SeekPointInfo;

  // Get the parameters sets as extradata. The format of this depends on the underlying codec.
  virtual QByteArray getExtradata() = 0;
  // Get some other properties of the bitstream in order to configure the FFMpegDecoder
  virtual IntPair getProfileLevel()      = 0;
  virtual Ratio   getSampleAspectRatio() = 0;

  std::optional<FileStartEndPos> getFrameStartEndPos(FrameIndexCodingOrder idx);

  bool parseAnnexBFile(std::unique_ptr<FileSourceAnnexBFile> &file, QWidget *mainWindow = nullptr);

  // Called from the bitstream analyzer. This function can run in a background process.
  bool runParsingOfFile(QString compressedFilePath) override;

  virtual ParseResult parseAndAddNALUnit(int                                           unitID,
                                         const ByteVector &                            data,
                                         std::optional<BitratePlotModel::BitrateEntry> bitrateEntry,
                                         std::optional<FileStartEndPos> nalStartEndPosFile = {},
                                         std::shared_ptr<TreeItem>      parent = nullptr) = 0;

protected:
  struct AnnexBFrame
  {
    AnnexBFrame() = default;
    int poc{-1}; //< The poc of this frame
    std::optional<FileStartEndPos>
         fileStartEndPos;          //< The start and end position of all slice NAL units (if known)
    bool randomAccessPoint{false}; //< Can we start decoding here?

    bool operator<(AnnexBFrame const &b) const { return (this->poc < b.poc); }
    bool operator==(AnnexBFrame const &b) const { return (this->poc == b.poc); }
  };

  // Returns false if the POC was already present int the list
  bool
  addFrameToList(int poc, std::optional<FileStartEndPos> fileStartEndPos, bool randomAccessPoint);

  static void logNALSize(const ByteVector &             data,
                         std::shared_ptr<TreeItem>      root,
                         std::optional<FileStartEndPos> nalStartEndPos);

  int pocOfFirstRandomAccessFrame{-1};

  int getFramePOC(FrameIndexDisplayOrder frameIdx);

private:
  // A list of all frames in the sequence (in coding order) with POC and the file positions of all
  // slice NAL units associated with a frame. POC's don't have to be consecutive, so the only way to
  // know how many pictures are in a sequences is to keep a list of all POCs.
  vector<AnnexBFrame> frameListCodingOrder;
  // The same list of frames but sorted in display order. Generated from the list above whenever
  // needed.
  vector<AnnexBFrame> frameListDisplayOder;
  void                updateFrameListDisplayOrder();
};

} // namespace parser