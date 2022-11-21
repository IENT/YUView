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

#include <QMap>
#include <QString>
#include <QTreeWidgetItem>

#include "common/BitratePlotModel.h"
#include "common/HRDPlotModel.h"
#include "common/PacketItemModel.h"

// If the file parsing limit is enabled (setParsingLimitEnabled) parsing will be aborted after
// 500 frames have been parsed. This should be enough in most situations and full parsing can be
// enabled manually if needed.
#define PARSER_FILE_FRAME_NR_LIMIT 500

namespace parser
{

/* Abstract base class that prvides features which are common to all parsers
 */
class Base : public QObject
{
  Q_OBJECT

public:
  Base(QObject *parent);
  virtual ~Base() = 0;

  QAbstractItemModel *getPacketItemModel() { return streamIndexFilter.data(); }
  BitratePlotModel *  getBitratePlotModel() { return bitratePlotModel.data(); }
  HRDPlotModel *      getHRDPlotModel();
  void                setRedirectPlotModel(HRDPlotModel *plotModel);

  void updateNumberModelItems();
  void enableModel();

  // Get info about the stream organized in a tree
  [[nodiscard]] virtual StreamsInfo getStreamsInfo() const = 0;
  [[nodiscard]] virtual int         getNrStreams() const   = 0;

  /* Parse the NAL unit / OBU and what it contains
   *
   * When there are no more units in the file (the file ends), call this function one last time
   * with empty data and a unitID of -1. \unitID A counter (ID) of the unit \data. The raw data of
   * the unit. For NALs, this may include the start code or not. \bitrateEntry Pass the bitrate
   * entry data into the function that may already be known. E.g. the ffmpeg parser already decodes
   * the DTS/PTS values from the container. \parent The tree item of the parent where the items will
   * be appended. \nalStartEndPosFile The position of the first and last byte of the unit.
   */
  struct ParseResult
  {
    bool                                          success{false};
    std::string                                   errorMessage;
    std::optional<std::string>                    unitTypeName;
    std::optional<int64_t>                        unitSize;
    std::optional<BitratePlotModel::BitrateEntry> bitrateEntry;
  };

  // For parsing files in the background (threading) in the bitstream analysis dialog:
  virtual bool runParsingOfFile(QString fileName) = 0;
  int          getParsingProgressPercent() { return progressPercentValue; }
  void         setAbortParsing() { cancelBackgroundParser = true; }

  virtual int     getVideoStreamIndex() { return -1; }
  virtual QString getShortStreamDescription(int streamIndex) const = 0;

  void setStreamColorCoding(bool colorCoding) { packetModel->setUseColorCoding(colorCoding); }
  void setFilterStreamIndex(int streamIndex)
  {
    streamIndexFilter->setFilterStreamIndex(streamIndex);
  }
  void setParsingLimitEnabled(bool limitEnabled) { parsingLimitEnabled = limitEnabled; }
  void setBitrateSortingIndex(int sortingIndex)
  {
    bitratePlotModel->setBitrateSortingIndex(sortingIndex);
  }

signals:
  // Some data was updated and the models can be updated to reflec this. This is called regularly
  // but not for every packet/Nal unit that is parsed.
  void modelDataUpdated();
  void backgroundParsingDone(QString error);

  // Signal that the getStreamsInfo() function will now return an updated info
  void streamInfoUpdated();

protected:
  QScopedPointer<PacketItemModel>               packetModel;
  QScopedPointer<FilterByStreamIndexProxyModel> streamIndexFilter;
  QScopedPointer<BitratePlotModel>              bitratePlotModel;

  static QString convertSliceTypeMapToString(QMap<QString, unsigned int> &currentAUSliceTypes);

  // If this variable is set (from an external thread), the parsing process should cancel
  // immediately
  bool cancelBackgroundParser{false};
  int  progressPercentValue{0};
  bool parsingLimitEnabled{false};

  // Save general information about the file here
  struct StreamInfo
  {
    [[nodiscard]] StringPairVec getStreamInfo() const;

    size_t   fileSize;
    unsigned nrUnits{0};
    unsigned nrFrames{0};
    bool     parsing{false};
  };
  StreamInfo streamInfo;

private:
  QScopedPointer<HRDPlotModel> hrdPlotModel;
  HRDPlotModel *               redirectPlotModel{nullptr};
};

} // namespace parser