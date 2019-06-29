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

#include "parserAVFormat.h"

#include "mainwindow.h"
#include "parserAnnexBAVC.h"
#include "parserAnnexBHEVC.h"
#include "parserAnnexBMpeg2.h"
#include "parserCommonMacros.h"

#include <QThread>

using namespace parserCommon;

#define PARSERAVCFORMAT_DEBUG_OUTPUT 0
#if PARSERAVCFORMAT_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_AVFORMAT qDebug
#else
#define DEBUG_AVFORMAT(fmt,...) ((void)0)
#endif

parserAVFormat::parserAVFormat(QObject *parent) : parserBase(parent)
{ 
  // Set the start code to look for (0x00 0x00 0x01)
  startCode.append((char)0);
  startCode.append((char)0);
  startCode.append((char)1);
}

QList<QTreeWidgetItem*> parserAVFormat::getStreamInfo()
{
  // streamInfoAllStreams containse all the info for all streams.
  // The first QStringPairList contains the general info, next all infos for each stream follows

  QList<QTreeWidgetItem*> info;
  if (streamInfoAllStreams.count() == 0)
    return info;
  
  QStringPairList generalInfo = streamInfoAllStreams[0];
  QTreeWidgetItem *general = new QTreeWidgetItem(QStringList() << "General");
  for (QStringPair p : generalInfo)
    new QTreeWidgetItem(general, QStringList() << p.first << p.second);
  info.append(general);

  for (int i=1; i<streamInfoAllStreams.count(); i++)
  {
    QTreeWidgetItem *streamInfo = new QTreeWidgetItem(QStringList() << QString("Stream %1").arg(i-1));
    for (QStringPair p : streamInfoAllStreams[i])
      new QTreeWidgetItem(streamInfo, QStringList() << p.first << p.second);
    info.append(streamInfo);
  }

  return info;
}

bool parserAVFormat::parseExtradata(QByteArray &extradata)
{
  if (extradata.isEmpty())
    return true;

  if (codecID.isAVC())
    return parseExtradata_AVC(extradata);
  else if (codecID.isHEVC())
    return parseExtradata_hevc(extradata);
  else if (codecID.isMpeg2())
    return parseExtradata_mpeg2(extradata);
  else
    return parseExtradata_generic(extradata);
  return true;
}

bool parserAVFormat::parseMetadata(QStringPairList &metadata)
{
  if (metadata.isEmpty() || packetModel->isNull())
    return true;

  // Log all entries in the metadata list
  TreeItem *metadataRoot = new TreeItem("Metadata", packetModel->getRootItem());
  for (QStringPair p : metadata)
    new TreeItem(p.first, p.second, "", "", metadataRoot);
  return true;
}

bool parserAVFormat::parseExtradata_generic(QByteArray &extradata)
{
  if (extradata.isEmpty() || packetModel->isNull())
    return true;

  // Log all bytes in the extradata
  TreeItem *extradataRoot = new TreeItem("Extradata", packetModel->getRootItem());
  for (int i = 0; i < extradata.length(); i++)
  {
    int val = (unsigned char)extradata.at(i);
    QString code = QString("%1 (0x%2)").arg(val, 8, 2, QChar('0')).arg(val, 2, 16, QChar('0'));
    new TreeItem(QString("Byte %1").arg(i), val, "b(8)", code, extradataRoot);
  }
  return true;
}

bool parserAVFormat::parseExtradata_AVC(QByteArray &extradata)
{
  if (extradata.isEmpty() || packetModel->isNull())
    return true;

  if (extradata.at(0) == 1 && extradata.length() >= 7)
  {
    reader_helper reader(extradata, packetModel->getRootItem(), "Extradata (Raw AVC NAL units)");
    IGNOREBITS(8); // Ignore the "1" byte which we already found

    // The extradata uses the avcc format (see avc.c in libavformat)
    unsigned int profile, profile_compat, level, reserved_6_one_bits, nal_size_length_minus1, reserved_3_one_bits, number_of_sps;
    READBITS(profile, 8);
    READBITS(profile_compat, 8);
    READBITS(level, 8);
    READBITS(reserved_6_one_bits, 6);
    READBITS(nal_size_length_minus1, 2);
    READBITS(reserved_3_one_bits, 3);
    READBITS(number_of_sps, 5);

    int pos = 6;
    int nalID = 0;
    for (unsigned int i = 0; i < number_of_sps; i++)
    {
      QByteArray size_bytes = extradata.mid(pos, 2);
      reader_helper sps_size_reader(size_bytes, reader.getCurrentItemTree(), QString("SPS %1").arg(i));
      unsigned int sps_size;
      if (!sps_size_reader.readBits(16, sps_size, "sps_size"))
        return false;

      TreeItem *subTree = sps_size_reader.getCurrentItemTree();
      QByteArray rawNAL = extradata.mid(pos+2, sps_size);
      if (!annexBParser->parseAndAddNALUnit(nalID, rawNAL, subTree))
        subTree->setError();
      nalID++;
      pos += sps_size + 2;
    }

    int nrPPS = extradata.at(pos++);
    for (int i = 0; i < nrPPS; i++)
    {
      QByteArray size_bytes = extradata.mid(pos, 2);
      reader_helper pps_size_reader(size_bytes, reader.getCurrentItemTree(), QString("PPS %1").arg(i));
      unsigned int pps_size;
      if (!pps_size_reader.readBits(16, pps_size, "pps_size"))
        return false;

      TreeItem *subTree = pps_size_reader.getCurrentItemTree();
      QByteArray rawNAL = extradata.mid(pos+2, pps_size);
      if (!annexBParser->parseAndAddNALUnit(nalID, rawNAL, subTree))
        subTree->setError();
      nalID++;
      pos += pps_size + 2;
    }
  }

  return true;
}

bool parserAVFormat::parseExtradata_hevc(QByteArray &extradata)
{
  if (extradata.isEmpty() || packetModel->isNull())
    return true;

  if (extradata.at(0) == 1)
  {
    // The extradata is using the hvcC format
    TreeItem *extradataRoot = new TreeItem("Extradata (HEVC hvcC format)", packetModel->getRootItem());
    hvcC h;
    if (!h.parse_hvcC(extradata, extradataRoot, annexBParser))
      return false;
  }
  else if (extradata.at(0) == 0)
  {
    // The extradata does just contain the raw HEVC parameter sets (with start codes).
    QByteArray startCode;
    startCode.append((char)0);
    startCode.append((char)0);
    startCode.append((char)1);

    TreeItem *extradataRoot = new TreeItem("Extradata (Raw HEVC NAL units)", packetModel->getRootItem());

    int nalID = 0;
    int nextStartCode = extradata.indexOf(startCode);
    int posInData = nextStartCode + 3;
    while (nextStartCode >= 0)
    {
      nextStartCode = extradata.indexOf(startCode, posInData);
      int length = nextStartCode - posInData;
      QByteArray nalData = (nextStartCode >= 0) ? extradata.mid(posInData, length) : extradata.mid(posInData);
      // Let the hevc annexB parser parse this
      if (!annexBParser->parseAndAddNALUnit(nalID, nalData, extradataRoot))
        extradataRoot->setError();
      nalID++;
      posInData = nextStartCode + 3;
    }
  }
  else
    return reader_helper::addErrorMessageChildItem("Unsupported extradata format (configurationVersion != 1)", packetModel->getRootItem());
  
  return true;
}

bool parserAVFormat::parseExtradata_mpeg2(QByteArray &extradata)
{
  if (extradata.isEmpty() || !packetModel->isNull())
    return true;

  if (extradata.at(0) == 0)
  {
    // The extradata does just contain the raw MPEG2 information
    QByteArray startCode;
    startCode.append((char)0);
    startCode.append((char)0);
    startCode.append((char)1);

    TreeItem *extradataRoot = new TreeItem("Extradata (Raw Mpeg2 units)", packetModel->getRootItem());

    int nalID = 0;
    int nextStartCode = extradata.indexOf(startCode);
    int posInData = nextStartCode + 3;
    while (nextStartCode >= 0)
    {
      nextStartCode = extradata.indexOf(startCode, posInData);
      int length = nextStartCode - posInData;
      QByteArray nalData = (nextStartCode >= 0) ? extradata.mid(posInData, length) : extradata.mid(posInData);
      // Let the hevc annexB parser parse this
      if (!annexBParser->parseAndAddNALUnit(nalID, nalData, extradataRoot))
        extradataRoot->setError();
      nalID++;
      posInData = nextStartCode + 3;
    }
  }
  else
    return reader_helper::addErrorMessageChildItem("Unsupported extradata format (configurationVersion != 1)", packetModel->getRootItem());

  return true;
}

bool parserAVFormat::parseAVPacket(unsigned int packetID, AVPacketWrapper &packet)
{
  if (!packetModel->isNull())
  {
    // Use the given tree item. If it is not set, use the nalUnitMode (if active).
    // Create a new TreeItem root for the NAL unit. We don't set data (a name) for this item
    // yet. We want to parse the item and then set a good description.
    QString specificDescription;
    TreeItem *itemTree = new TreeItem(packetModel->getRootItem());

    int posInData = 0;
    QByteArray avpacketData = QByteArray::fromRawData((const char*)(packet.get_data()), packet.get_data_size());
    
    // Log all the packet info
    new TreeItem("stream_index", packet.get_stream_index(), itemTree);
    new TreeItem("pts", packet.get_pts(), itemTree);
    new TreeItem("dts", packet.get_dts(), itemTree);
    new TreeItem("duration", packet.get_duration(), itemTree);
    new TreeItem("flag_keyframe", packet.get_flag_keyframe(), itemTree);
    new TreeItem("flag_corrupt", packet.get_flag_corrupt(), itemTree);
    new TreeItem("flag_discard", packet.get_flag_discard(), itemTree);
    new TreeItem("data_size", packet.get_data_size(), itemTree);

    itemTree->setStreamIndex(packet.get_stream_index());

    if (packet.is_video_packet)
    {
      if (annexBParser)
      {
        // Colloect the types of NALs to create a good name later
        QStringList nalNames;

        int nalID = 0;
        packetDataFormat_t packetFormat = packet.guessDataFormatFromData();
        const int MIN_NAL_SIZE = 3;
        while (posInData + MIN_NAL_SIZE <= avpacketData.length())
        {
          QByteArray firstBytes = avpacketData.mid(posInData, 4);

          QByteArray nalData;
          if (packetFormat == packetFormatRawNAL)
          {
            int offset;
            if (firstBytes.at(1) == (char)0 && firstBytes.at(2) == (char)0 && firstBytes.at(3) == (char)1)
              offset = 4;
            else if (firstBytes.at(0) == (char)0 && firstBytes.at(1) == (char)0 && firstBytes.at(2) == (char)1)
              offset = 3;
            else
              return reader_helper::addErrorMessageChildItem("Start code could not be found.", itemTree);

            // Look for the next start code (or the end of the file)
            int nextStartCodePos = avpacketData.indexOf(startCode, posInData + 3);

            if (nextStartCodePos == -1)
            {
              nalData = avpacketData.mid(posInData + offset);
              posInData = avpacketData.length() + 1;
              DEBUG_AVFORMAT("parserAVFormat::parseAVPacket start code -1 - NAL from %d to %d", posInData + offset, avpacketData.length());
            }
            else
            {
              const int size = nextStartCodePos - posInData - offset;
              nalData = avpacketData.mid(posInData + offset, size);
              posInData += 3 + size;
              DEBUG_AVFORMAT("parserAVFormat::parseAVPacket start code %d - NAL from %d to %d", nextStartCodePos, posInData + offset, nextStartCodePos);
            }
          }
          else
          {
            int size = (unsigned char)firstBytes.at(3);
            size += (unsigned char)firstBytes.at(2) << 8;
            size += (unsigned char)firstBytes.at(1) << 16;
            size += (unsigned char)firstBytes.at(0) << 24;
            posInData += 4;

            if (size < 0)
              // The int did overflow. This means that the NAL unit is > 2GB in size. This is probably an error
              return reader_helper::addErrorMessageChildItem("Invalid size indicator in packet.", itemTree);
            if (posInData + size > avpacketData.length())
              return reader_helper::addErrorMessageChildItem("Not enough data in the input array to read NAL unit.", itemTree);

            nalData = avpacketData.mid(posInData, size);
            posInData += size;
            DEBUG_AVFORMAT("parserAVFormat::parseAVPacket NAL from %d to %d", posInData, posInData + size);
          }

          // Parse the NAL data
          QString nalTypeName;
          QUint64Pair nalStartEndPosFile; // Not used
          if (!annexBParser->parseAndAddNALUnit(nalID, nalData, itemTree, nalStartEndPosFile, &nalTypeName))
            itemTree->setError();
          if (!nalTypeName.isEmpty())
            nalNames.append(nalTypeName);
          nalID++;
        }

        // Create a good detailed and compact description of the AVpacket
        if (codecID.isMpeg2())
          specificDescription = " - ";    // In mpeg2 there is no concept of NAL units
        else
          specificDescription = " - NALs:";
        for (QString n : nalNames)
          specificDescription += (" " + n);
      }
      else if (obuParser)
      {
        int obuID = 0;
        // Colloect the types of OBus to create a good name later
        QStringList obuNames;

        const int MIN_OBU_SIZE = 2;
        while (posInData + MIN_OBU_SIZE <= avpacketData.length())
        {
          QString obuTypeName;
          QUint64Pair obuStartEndPosFile; // Not used
          try
          {  
            int nrBytesRead = obuParser->parseAndAddOBU(obuID, avpacketData.mid(posInData), itemTree, obuStartEndPosFile, &obuTypeName);
            DEBUG_AVFORMAT("parserAVFormat::parseAVPacket parsed OBU %d header %d bytes", obuID, nrBytesRead);
            posInData += nrBytesRead;
          }
          catch (...)
          {
            // Catch exceptions and just return
            break;
          }

          if (!obuTypeName.isEmpty())
            obuNames.append(obuTypeName);
          obuID++;

          if (obuID > 200)
          {
            DEBUG_AVFORMAT("parserAVFormat::parseAVPacket We encountered more than 200 OBUs in one packet. This is probably an error.");
            return false;
          }
        }

        specificDescription = " - OBUs:";
        for (QString n : obuNames)
          specificDescription += (" " + n);
      }
    }

    // Set a useful name of the TreeItem (the root for this NAL)
    itemTree->itemData.append(QString("AVPacket %1%2").arg(packetID).arg(packet.get_flag_keyframe() ? " - Keyframe": "") + specificDescription);
  }

  return true;
}

bool parserAVFormat::hvcC::parse_hvcC(QByteArray &hvcCData, TreeItem *root, QScopedPointer<parserAnnexB> &annexBParser)
{
  reader_helper reader(hvcCData, root, "hvcC");
  reader.disableEmulationPrevention();

  unsigned int reserved_4onebits, reserved_5onebits, reserver_6onebits;

  // The first 22 bytes are the hvcC header
  READBITS(configurationVersion, 8);
  if (configurationVersion != 1)
    return reader.addErrorMessageChildItem("Only configuration version 1 supported.");
  READBITS(general_profile_space, 2);
  READFLAG(general_tier_flag);
  READBITS(general_profile_idc, 5);
  READBITS(general_profile_compatibility_flags, 32);
  READBITS(general_constraint_indicator_flags, 48);
  READBITS(general_level_idc, 8);
  READBITS(reserved_4onebits, 4);
  if (reserved_4onebits != 15)
    return reader.addErrorMessageChildItem("The reserved 4 one bits should all be one.");
  READBITS(min_spatial_segmentation_idc, 12);
  READBITS(reserver_6onebits, 6);
  if (reserver_6onebits != 63)
    return reader.addErrorMessageChildItem("The reserved 6 one bits should all be one.");
  QStringList parallelismTypeMeaning = QStringList()
    << "mixed-type parallel decoding"
    << "slice-based parallel decoding"
    << "tile-based parallel decoding"
    << "wavefront-based parallel decoding";
  READBITS_M(parallelismType, 2, parallelismTypeMeaning);
  READBITS(reserver_6onebits, 6);
  if (reserver_6onebits != 63)
    return reader.addErrorMessageChildItem("The reserved 6 one bits should all be one.");
  READBITS(chromaFormat, 2);
  READBITS(reserved_5onebits, 5);
  if (reserved_5onebits != 31)
    return reader.addErrorMessageChildItem("The reserved 6 one bits should all be one.");
  READBITS(bitDepthLumaMinus8, 3);
  READBITS(reserved_5onebits, 5);
  if (reserved_5onebits != 31)
    return reader.addErrorMessageChildItem("The reserved 6 one bits should all be one.");
  READBITS(bitDepthChromaMinus8, 3);
  READBITS(avgFrameRate, 16);
  READBITS(constantFrameRate, 2);
  READBITS(numTemporalLayers, 3);
  READFLAG(temporalIdNested);
  READBITS(lengthSizeMinusOne, 2);
  READBITS(numOfArrays, 8);

  // Now parse the contained raw NAL unit arrays
  for (unsigned int i = 0; i < numOfArrays; i++)
  {
    hvcC_naluArray a;
    if (!a.parse_hvcC_naluArray(i, reader, annexBParser))
      return false;
    naluArrayList.append(a);
  }
  return true;
}

bool parserAVFormat::hvcC_naluArray::parse_hvcC_naluArray(int arrayID, reader_helper &reader, QScopedPointer<parserAnnexB> &annexBParser)
{
  reader_sub_level sub_level_adder(reader, QString("nal unit array %1").arg(arrayID));

  // The next 3 bytes contain info about the array
  READFLAG(array_completeness);
  READFLAG(reserved_flag_false);
  if (reserved_flag_false)
    return reader.addErrorMessageChildItem("The reserved_flag_false should be false.");
  READBITS(NAL_unit_type, 6);
  READBITS(numNalus, 16);
  
  for (unsigned int i = 0; i < numNalus; i++)
  {
    hvcC_nalUnit nal;
    if (!nal.parse_hvcC_nalUnit(i, reader, annexBParser))
      return false;
    nalList.append(nal);
  }

  return true;
}

bool parserAVFormat::hvcC_nalUnit::parse_hvcC_nalUnit(int unitID, reader_helper &reader, QScopedPointer<parserAnnexB> &annexBParser)
{
  reader_sub_level sub_level_adder(reader, QString("nal unit %1").arg(unitID));

  READBITS(nalUnitLength, 16);

  // Get the bytes of the raw nal unit to pass to the "real" hevc parser
  QByteArray nalData = reader.readBytes(nalUnitLength);

  // Let the hevc annexB parser parse this
  if (!annexBParser->parseAndAddNALUnit(unitID, nalData, reader.getCurrentItemTree()))
    return false;

  return true;
}

bool parserAVFormat::runParsingOfFile(QString compressedFilePath)
{
  // Open the file but don't parse it yet.
  QScopedPointer<fileSourceFFmpegFile> ffmpegFile(new fileSourceFFmpegFile());
 if (!ffmpegFile->openFile(compressedFilePath, nullptr, nullptr, false))
  {
    emit backgroundParsingDone("Error opening the ffmpeg file.");
    return false;
  }

  codecID = ffmpegFile->getVideoStreamCodecID();
  if (codecID.isAVC())
    annexBParser.reset(new parserAnnexBAVC());
  else if (codecID.isHEVC())
    annexBParser.reset(new parserAnnexBHEVC());
  else if (codecID.isMpeg2())
    annexBParser.reset(new parserAnnexBMpeg2());
  else if (codecID.isAV1())
    obuParser.reset(new parserAV1OBU());
  else if (codecID.isNone())
  {
    emit backgroundParsingDone("Unknown codec ID " + codecID.getCodecName());
    return false;
  }

  int max_ts = ffmpegFile->getMaxTS();
  videoStreamIndex = ffmpegFile->getVideoStreamIndex();

  // Don't seek to the beginning here. This causes more problems then it solves.
  // ffmpegFile->seekFileToBeginning();

  // First get the extradata and push it to the parser
  QByteArray extradata = ffmpegFile->getExtradata();
  parseExtradata(extradata);

  // Parse the metadata
  QStringPairList metadata = ffmpegFile->getMetadata();
  parseMetadata(metadata);

  // After opening the file, we can get information on it
  streamInfoAllStreams = ffmpegFile->getFileInfoForAllStreams();
  emit streamInfoUpdated();

  // Now iterate over all packets and send them to the parser
  AVPacketWrapper packet = ffmpegFile->getNextPacket(false, false);
  int64_t start_ts = packet.get_dts();

  unsigned int packetID = 0;
  unsigned int videoFrameCounter = 0;
  bool abortParsing = false;
  QElapsedTimer signalEmitTimer;
  signalEmitTimer.start();
  while (!ffmpegFile->atEnd() && !abortParsing)
  {
    if (packet.is_video_packet)
    {
      progressPercentValue = clip((int)((packet.get_dts() - start_ts) * 100 / max_ts), 0, 100);
      videoFrameCounter++;
    }

    if (!parseAVPacket(packetID, packet))
    {
      DEBUG_AVFORMAT("parserAVFormat::parseAVPacket error parsing Packet %d", packetID);
    }
    else
    {
      DEBUG_AVFORMAT("parserAVFormat::parseAVPacket Packet %d", packetID);
    }

    packetID++;
    packet = ffmpegFile->getNextPacket(false, false);
    
    // For signal slot debugging purposes, sleep
    // QThread::msleep(200);
    
    if (signalEmitTimer.elapsed() > 1000 && packetModel)
    {
      signalEmitTimer.start();
      emit nalModelUpdated(packetModel->getNumberFirstLevelChildren());
    }

    if (cancelBackgroundParser)
    {
      abortParsing = true;
      DEBUG_AVFORMAT("parserAVFormat::parseAVPacket Abort parsing by user request");
    }
    if (parsingLimitEnabled && videoFrameCounter > PARSER_FILE_FRAME_NR_LIMIT)
    {
      DEBUG_AVFORMAT("parserAVFormat::parseAVPacket Abort parsing because frame limit was reached.");
      abortParsing = true;
    }
  }

  // Seek back to the beginning of the stream.
  ffmpegFile->seekFileToBeginning();

  if (packetModel)
    emit nalModelUpdated(packetModel->getNumberFirstLevelChildren());

  streamInfoAllStreams = ffmpegFile->getFileInfoForAllStreams();
  emit streamInfoUpdated();
  emit backgroundParsingDone("");

  return !cancelBackgroundParser;
}
