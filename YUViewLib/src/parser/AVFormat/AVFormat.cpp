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

#include "AVFormat.h"

#include <QElapsedTimer>
#include <cmath>
#include <iomanip>

#include "../AVC/AnnexBAVC.h"
#include "../HEVC/AnnexBHEVC.h"
#include "../Mpeg2/AnnexBMpeg2.h"
#include "../Subtitles/Subtitle608.h"
#include "../Subtitles/SubtitleDVB.h"
#include "HVCC.h"
#include "parser/common/SubByteReaderLogging.h"
#include "parser/common/functions.h"

#define PARSERAVCFORMAT_DEBUG_OUTPUT 0
#if PARSERAVCFORMAT_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVFORMAT qDebug
#else
#define DEBUG_AVFORMAT(fmt, ...) ((void)0)
#endif

using namespace std::string_literals;

namespace parser
{

const ByteVector startCode({0, 0, 1});

using namespace reader;

QList<QTreeWidgetItem *> AVFormat::getStreamInfo()
{
  // streamInfoAllStreams containse all the info for all streams.
  // The first QStringPairList contains the general info, next all infos for each stream follows

  QList<QTreeWidgetItem *> info;
  if (this->streamInfoAllStreams.count() == 0)
    return info;

  QStringPairList  generalInfo = this->streamInfoAllStreams[0];
  QTreeWidgetItem *general     = new QTreeWidgetItem(QStringList() << "General");
  for (QStringPair p : generalInfo)
    new QTreeWidgetItem(general, QStringList() << p.first << p.second);
  info.append(general);

  for (int i = 1; i < this->streamInfoAllStreams.count(); i++)
  {
    QTreeWidgetItem *streamInfo =
        new QTreeWidgetItem(QStringList() << QString("Stream %1").arg(i - 1));
    for (QStringPair p : this->streamInfoAllStreams[i])
      new QTreeWidgetItem(streamInfo, QStringList() << p.first << p.second);
    info.append(streamInfo);
  }

  return info;
}

QString AVFormat::getShortStreamDescription(int streamIndex) const
{
  if (streamIndex >= this->shortStreamInfoAllStreams.count())
    return {};
  return this->shortStreamInfoAllStreams[streamIndex];
}

bool AVFormat::parseExtradata(ByteVector &extradata)
{
  if (extradata.empty())
    return true;

  if (this->codecID.isAVC())
    return this->parseExtradata_AVC(extradata);
  else if (this->codecID.isHEVC())
    return this->parseExtradata_hevc(extradata);
  else if (this->codecID.isMpeg2())
    return this->parseExtradata_mpeg2(extradata);
  else
    return this->parseExtradata_generic(extradata);
}

void AVFormat::parseMetadata(const StringPairVec &metadata)
{
  if (metadata.size() == 0 || !packetModel->rootItem)
    return;

  // Log all entries in the metadata list
  auto metadataRoot = packetModel->rootItem->createChildItem("Metadata");
  for (const auto &p : metadata)
    metadataRoot->createChildItem(p.first, p.second);
}

bool AVFormat::parseExtradata_generic(ByteVector &extradata)
{
  if (extradata.empty() || !packetModel->rootItem)
    return true;

  SubByteReaderLogging reader(extradata, packetModel->rootItem, "Extradata");
  reader.disableEmulationPrevention();
  unsigned             i = 0;
  while (reader.canReadBits(8))
    reader.readBytes(formatArray("raw_byte", i++), 1);

  return true;
}

bool AVFormat::parseExtradata_AVC(ByteVector &extradata)
{
  if (extradata.empty() || !packetModel->rootItem)
    return true;

  if (extradata.at(0) == 1 && extradata.size() >= 7)
  {
    SubByteReaderLogging reader(extradata, packetModel->rootItem, "Extradata (Raw AVC NAL units)");
    reader.disableEmulationPrevention();

    reader.readBits("Version", 8);

    // The extradata uses the avcc format (see avc.c in libavformat)
    reader.readBits("profile", 8);
    reader.readBits("profile_compat", 8);
    reader.readBits("level", 8);
    reader.readBits("reserved_6_one_bits", 6);
    reader.readBits("nal_size_length_minus1", 2);
    reader.readBits("reserved_3_one_bits", 3);
    auto number_of_sps = reader.readBits("number_of_sps", 5);

    unsigned nalID = 0;
    for (unsigned i = 0; i < number_of_sps; i++)
    {
      SubByteReaderLoggingSubLevel spsSubLevel(reader, "SPS " + std::to_string(i));
      auto                         sps_size = reader.readBits("sps_size", 16);
      auto spsData     = reader.readBytes("", sps_size, Options().withLoggingDisabled());
      auto parseResult = this->annexBParser->parseAndAddNALUnit(
          nalID++, spsData, {}, {}, reader.getCurrentItemTree());
      if (parseResult.success && parseResult.bitrateEntry)
        this->bitratePlotModel->addBitratePoint(this->videoStreamIndex, *parseResult.bitrateEntry);
    }

    auto number_of_pps = reader.readBits("number_of_pps", 8);
    for (unsigned i = 0; i < number_of_pps; i++)
    {
      SubByteReaderLoggingSubLevel ppsSubLevel(reader, "PPS " + std::to_string(i));
      auto                         pps_size = reader.readBits("pps_size", 16);
      auto pspsData    = reader.readBytes("", pps_size, Options().withLoggingDisabled());
      auto parseResult = this->annexBParser->parseAndAddNALUnit(
          nalID++, pspsData, {}, {}, reader.getCurrentItemTree());
      if (parseResult.success && parseResult.bitrateEntry)
        this->bitratePlotModel->addBitratePoint(this->videoStreamIndex, *parseResult.bitrateEntry);
    }
  }

  return true;
}

bool AVFormat::parseExtradata_hevc(ByteVector &extradata)
{
  if (extradata.empty() || !packetModel->rootItem)
    return true;

  if (extradata.at(0) == 1)
  {
    auto hevcParser = dynamic_cast<AnnexBHEVC *>(this->annexBParser.get());
    if (!hevcParser)
      return false;

    try
    {
      avformat::HVCC hvcc;
      hvcc.parse(extradata, packetModel->rootItem, hevcParser, this->bitratePlotModel.data());
    }
    catch (...)
    {
      DEBUG_AVFORMAT("AVFormat::parseExtradata_hevc Error parsing HEVC Extradata");
      return false;
    }
  }
  else if (extradata.at(0) == 0)
  {
    auto item = packetModel->rootItem->createChildItem("Extradata (Raw HEVC NAL units)");
    this->parseByteVectorAnnexBStartCodes(extradata, PacketDataFormat::RawNAL, {}, item);
  }
  else
  {
    packetModel->rootItem->createChildItem(
        "Unsupported extradata format (configurationVersion != 1)"s, {}, {}, {}, {}, true);
    return false;
  }

  return true;
}

bool AVFormat::parseExtradata_mpeg2(ByteVector &extradata)
{
  if (extradata.empty() || !packetModel->rootItem)
    return true;

  if (extradata.at(0) == 0)
  {
    this->parseByteVectorAnnexBStartCodes(
        extradata,
        PacketDataFormat::RawNAL,
        {},
        packetModel->rootItem->createChildItem("Extradata (Raw Mpeg2 units)"));
  }
  else
    packetModel->rootItem->createChildItem(
        "Unsupported extradata format (configurationVersion != 1)");

  return true;
}

std::map<std::string, unsigned>
AVFormat::parseByteVectorAnnexBStartCodes(ByteVector &                   data,
                                          PacketDataFormat               dataFormat,
                                          BitratePlotModel::BitrateEntry packetBitrateEntry,
                                          std::shared_ptr<TreeItem>      item)
{
  if (dataFormat != PacketDataFormat::RawNAL && dataFormat != PacketDataFormat::MP4)
  {
    DEBUG_AVFORMAT("AVFormat::parseByteVectorAnnexBStartCodes Unsupported data format");
    return {};
  }

  auto getNextNalStart = [&data, &dataFormat](ByteVector::iterator searchStart) {
    if (dataFormat == PacketDataFormat::RawNAL)
    {
      if (std::distance(searchStart, data.end()) <= 3)
        return data.end();
      auto itStartCode =
          std::search(searchStart + 3, data.end(), startCode.begin(), startCode.end());
      if (itStartCode == data.end())
        return data.end();
      return itStartCode;
    }
    else if (dataFormat == PacketDataFormat::MP4)
    {
      int size = 0;
      size += ((*(searchStart++)) << 24);
      size += ((*(searchStart++)) << 16);
      size += ((*(searchStart++)) << 8);
      size += (*(searchStart++));
      if (size > std::distance(searchStart, data.end()))
        return data.end();
      return searchStart + size;
    }
    return searchStart;
  };

  auto itStartCode = data.begin();
  if (dataFormat == PacketDataFormat::RawNAL)
  {
    itStartCode = getNextNalStart(itStartCode);
    if (itStartCode == data.end())
    {
      DEBUG_AVFORMAT("AVFormat::parseByteVectorAnnexBStartCodes Not a single start code could be "
                     "found in the data. Are you sure the data format is correct?");
      return {};
    }
  }

  const auto sizeStartCode = (dataFormat == PacketDataFormat::RawNAL ? 3u : 4u);

  auto                            nalID = 0u;
  std::map<std::string, unsigned> naNames;
  while (itStartCode != data.end())
  {
    auto itNextStartCode = getNextNalStart(itStartCode);
    auto nalData         = ByteVector(itStartCode + sizeStartCode, itNextStartCode);
    auto parseResult =
        this->annexBParser->parseAndAddNALUnit(nalID++, nalData, packetBitrateEntry, {}, item);
    if (parseResult.success && parseResult.bitrateEntry)
      this->bitratePlotModel->addBitratePoint(this->videoStreamIndex, *parseResult.bitrateEntry);
    if (parseResult.success && parseResult.nalTypeName)
      naNames[*parseResult.nalTypeName]++;
    itStartCode = itNextStartCode;
  }
  return naNames;
}

bool AVFormat::parseAVPacket(unsigned packetID, unsigned streamPacketID, AVPacketWrapper &packet)
{
  if (!packetModel->rootItem)
    return true;

  // Use the given tree item. If it is not set, use the nalUnitMode (if active).
  // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
  // yet. We want to parse the item and then set a good description.
  std::string specificDescription;
  auto        itemTree = packetModel->rootItem->createChildItem();

  ByteVector avpacketData;
  {
    auto dataPointer = packet.getData();
    auto dataLength  = packet.getDataSize();
    avpacketData.assign(dataPointer, dataPointer + dataLength);
  }

  AVRational timeBase = timeBaseAllStreams[packet.getStreamIndex()];

  auto formatTimestamp = [](int64_t timestamp, AVRational timebase) -> std::string {
    std::ostringstream ss;
    ss << timestamp << " (";
    if (timestamp < 0)
      ss << "-";

    auto time = std::abs(timestamp) * 1000 / timebase.num / timebase.den;

    auto hours = time / 1000 / 60 / 60;
    time -= hours * 60 * 60 * 1000;
    auto minutes = time / 1000 / 60;
    time -= minutes * 60 * 1000;
    auto seconds      = time / 1000;
    auto milliseconds = time - (seconds * 1000);

    if (hours > 0)
      ss << hours << ":";
    if (hours > 0 || minutes > 0)
      ss << std::setfill('0') << std::setw(2) << minutes << ":";
    ss << std::setfill('0') << std::setw(2) << seconds << ".";
    ss << std::setfill('0') << std::setw(3) << milliseconds;
    ss << ")";

    return ss.str();
  };

  // Log all the packet info
  itemTree->createChildItem("stream_index", packet.getStreamIndex());
  itemTree->createChildItem("Global AVPacket Count", packetID);
  itemTree->createChildItem("pts", formatTimestamp(packet.getPTS(), timeBase));
  itemTree->createChildItem("dts", formatTimestamp(packet.getDTS(), timeBase));
  itemTree->createChildItem("duration", formatTimestamp(packet.getDuration(), timeBase));
  itemTree->createChildItem("flag_keyframe", packet.getFlagKeyframe());
  itemTree->createChildItem("flag_corrupt", packet.getFlagCorrupt());
  itemTree->createChildItem("flag_discard", packet.getFlagDiscard());
  itemTree->createChildItem("data_size", packet.getDataSize());

  itemTree->setStreamIndex(packet.getStreamIndex());

  if (packet.getPacketType() == PacketType::VIDEO && (this->annexBParser || this->obuParser))
  {
    // Colloect the types of OBus/NALs to create a good name later
    std::map<std::string, unsigned> unitNames;

    if (this->annexBParser)
    {
      // Colloect the types of NALs to create a good name later
      auto                           packetFormat = packet.guessDataFormatFromData();
      BitratePlotModel::BitrateEntry packetBitrateEntry;
      packetBitrateEntry.dts      = packet.getDTS();
      packetBitrateEntry.pts      = packet.getPTS();
      packetBitrateEntry.duration = packet.getDuration();
      unitNames                   = this->parseByteVectorAnnexBStartCodes(
          avpacketData, packetFormat, packetBitrateEntry, itemTree);

      // Create a good detailed and compact description of the AVpacket
      if (this->codecID.isMpeg2())
        specificDescription = " - "; // In mpeg2 there is no concept of NAL units
      else
        specificDescription = " - NALs:";
    }
    else if (this->obuParser)
    {
      auto obuID = 0u;

      auto posInData = avpacketData.begin();
      while (true)
      {
        pairUint64 obuStartEndPosFile; // Not used
        try
        {
          auto data = ByteVector(posInData, avpacketData.end());
          auto [nrBytesRead, obuTypeName] =
              this->obuParser->parseAndAddOBU(obuID, data, itemTree, obuStartEndPosFile);
          DEBUG_AVFORMAT(
              "AVFormat::parseAVPacket parsed OBU %d header %d bytes", obuID, nrBytesRead);

          constexpr auto minOBUSize = 3u;
          auto           remaining  = std::distance(posInData, avpacketData.end());
          if (remaining < 0 || nrBytesRead + minOBUSize >= size_t(std::abs(remaining)))
            break;
          posInData += nrBytesRead;

          if (!obuTypeName.empty())
            unitNames[obuTypeName]++;
        }
        catch (...)
        {
          // Catch exceptions and just return
          break;
        }

        obuID++;

        if (obuID > 200)
        {
          DEBUG_AVFORMAT("AVFormat::parseAVPacket We encountered more than 200 OBUs in one packet. "
                         "This is probably an error.");
          return false;
        }
      }

      specificDescription = " - OBUs:";
    }

    for (const auto &entry : unitNames)
    {
      specificDescription += " " + entry.first;
      if (entry.second > 1)
        specificDescription += "(x" + std::to_string(entry.second) + ")";
    }
  }
  else if (packet.getPacketType() == PacketType::SUBTITLE_DVB)
  {
    auto segmentID = 0u;

    auto posInData = avpacketData.begin();
    while (true)
    {
      try
      {
        auto data                = ByteVector(posInData, avpacketData.end());
        auto [nrBytesRead, name] = subtitle::dvb::parseDVBSubtitleSegment(data, itemTree);
        (void)name;
        DEBUG_AVFORMAT(
            "AVFormat::parseAVPacket parsed DVB segment %d - %d bytes", obuID, nrBytesRead);

        constexpr auto minDVBSegmentSize = 6u;
        auto           remaining         = std::distance(posInData, avpacketData.end());
        if (remaining < 0 || nrBytesRead + minDVBSegmentSize >= size_t(std::abs(remaining)))
          break;

        posInData += nrBytesRead;
      }
      catch (...)
      {
        DEBUG_AVFORMAT(
            "AVFormat::parseAVPacket Exception occured while parsing DBV subtitle segment.");
        break;
      }

      segmentID++;

      if (segmentID > 200)
      {
        DEBUG_AVFORMAT("AVFormat::parseAVPacket We encountered more than 200 DVB segments in one "
                       "packet. This is probably an error.");
        return false;
      }
    }
  }
  else if (packet.getPacketType() == PacketType::SUBTITLE_608)
  {
    try
    {
      subtitle::sub_608::parse608SubtitlePacket(avpacketData, itemTree);
    }
    catch (...)
    {
      DEBUG_AVFORMAT(
          "AVFormat::parseAVPacket Exception occured while parsing 608 subtitle segment.");
    }
  }
  else
  {
    const auto nrBytesToLog = std::min(avpacketData.size(), size_t(100));

    SubByteReaderLogging reader(avpacketData, itemTree, "Data");
    reader.disableEmulationPrevention();
    reader.readBytes("raw_byte", nrBytesToLog);

    BitratePlotModel::BitrateEntry entry;
    entry.pts      = packet.getPTS();
    entry.dts      = packet.getDTS();
    entry.bitrate  = packet.getDataSize();
    entry.keyframe = packet.getFlagKeyframe();
    entry.duration = packet.getDuration();
    if (entry.duration == 0)
    {
      // Unknown. We have to guess.
      entry.duration = 10; // The backup guess
      if (this->framerate > 0 && this->videoStreamIndex >= 0 &&
          this->videoStreamIndex < this->timeBaseAllStreams.size())
      {
        auto videoTimeBase = this->timeBaseAllStreams[this->videoStreamIndex];
        auto duration      = 1.0 / this->framerate * videoTimeBase.den / videoTimeBase.num;
        entry.duration     = int(std::round(duration));
      }
    }

    bitratePlotModel->addBitratePoint(packet.getStreamIndex(), entry);
  }

  // Set a useful name of the TreeItem (the root for this NAL)
  auto name = "AVPacket " + std::to_string(streamPacketID) +
              (packet.getFlagKeyframe() ? " - Keyframe" : "") + specificDescription;
  itemTree->setProperties(name);

  return true;
}

bool AVFormat::runParsingOfFile(QString compressedFilePath)
{
  // Open the file but don't parse it yet.
  QScopedPointer<FileSourceFFmpegFile> ffmpegFile(new FileSourceFFmpegFile());
  if (!ffmpegFile->openFile(compressedFilePath, nullptr, nullptr, false))
  {
    emit backgroundParsingDone("Error opening the ffmpeg file.");
    return false;
  }

  this->codecID = ffmpegFile->getVideoStreamCodecID();
  if (this->codecID.isAVC())
    this->annexBParser.reset(new AnnexBAVC());
  else if (this->codecID.isHEVC())
    this->annexBParser.reset(new AnnexBHEVC());
  else if (this->codecID.isMpeg2())
    this->annexBParser.reset(new AnnexBMpeg2());
  else if (this->codecID.isAV1())
    this->obuParser.reset(new ParserAV1OBU());
  else if (this->codecID.isNone())
  {
    emit backgroundParsingDone("Unknown codec ID " + this->codecID.getCodecName());
    return false;
  }

  if (this->annexBParser)
    this->annexBParser->setRedirectPlotModel(this->getHRDPlotModel());
  if (this->obuParser)
    this->obuParser->setRedirectPlotModel(this->getHRDPlotModel());

  // Don't seek to the beginning here. This causes more problems then it solves.
  // ffmpegFile->seekFileToBeginning();

  // First get the extradata and push it to the parser
  try
  {
    auto extradata = SubByteReaderLogging::convertToByteVector(ffmpegFile->getExtradata());
    this->parseExtradata(extradata);
  }
  catch (...)
  {
    emit backgroundParsingDone("Error parsing Extradata from container");
    return false;
  }
  try
  {
    auto metadata = ffmpegFile->getMetadata();
    this->parseMetadata(metadata);
  }
  catch (...)
  {
    emit backgroundParsingDone("Error parsing Metadata from container");
    return false;
  }

  int max_ts                      = ffmpegFile->getMaxTS();
  this->videoStreamIndex          = ffmpegFile->getVideoStreamIndex();
  this->framerate                 = ffmpegFile->getFramerate();
  this->streamInfoAllStreams      = ffmpegFile->getFileInfoForAllStreams();
  this->timeBaseAllStreams        = ffmpegFile->getTimeBaseAllStreams();
  this->shortStreamInfoAllStreams = ffmpegFile->getShortStreamDescriptionAllStreams();

  emit streamInfoUpdated();

  // Now iterate over all packets and send them to the parser
  AVPacketWrapper packet   = ffmpegFile->getNextPacket(false, false);
  int64_t         start_ts = packet.getDTS();

  unsigned                packetID{};
  std::map<int, unsigned> packetCounterPerStream;

  unsigned      videoFrameCounter = 0;
  bool          abortParsing      = false;
  QElapsedTimer signalEmitTimer;
  signalEmitTimer.start();
  while (!ffmpegFile->atEnd() && !abortParsing)
  {
    if (packet.getPacketType() == PacketType::VIDEO)
    {
      if (max_ts != 0)
        progressPercentValue = clip(int((packet.getDTS() - start_ts) * 100 / max_ts), 0, 100);
      videoFrameCounter++;
    }

    auto streamPacketID = packetCounterPerStream[packet.getStreamIndex()];
    if (!this->parseAVPacket(packetID, streamPacketID, packet))
    {
      DEBUG_AVFORMAT("AVFormat::runParsingOfFile error parsing Packet %d", packetID);
    }
    else
    {
      DEBUG_AVFORMAT("AVFormat::runParsingOfFile Packet %d", packetID);
    }

    packetID++;
    packetCounterPerStream[packet.getStreamIndex()]++;
    packet = ffmpegFile->getNextPacket(false, false);

    // For signal slot debugging purposes, sleep
    // QThread::msleep(200);

    if (signalEmitTimer.elapsed() > 1000 && packetModel)
    {
      signalEmitTimer.start();
      emit modelDataUpdated();
    }

    if (cancelBackgroundParser)
    {
      abortParsing = true;
      DEBUG_AVFORMAT("AVFormat::runParsingOfFile Abort parsing by user request");
    }
    if (parsingLimitEnabled && videoFrameCounter > PARSER_FILE_FRAME_NR_LIMIT)
    {
      DEBUG_AVFORMAT("AVFormat::runParsingOfFile Abort parsing because frame limit was reached.");
      abortParsing = true;
    }
  }

  // Seek back to the beginning of the stream.
  ffmpegFile->seekFileToBeginning();

  if (packetModel)
    emit modelDataUpdated();

  this->streamInfoAllStreams = ffmpegFile->getFileInfoForAllStreams();
  emit streamInfoUpdated();
  emit backgroundParsingDone("");

  return !cancelBackgroundParser;
}

} // namespace parser
