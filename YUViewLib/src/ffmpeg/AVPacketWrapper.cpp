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

#include "AVPacketWrapper.h"
#include <parser/common/SubByteReaderLogging.h>
#include <stdexcept>

namespace FFmpeg
{

namespace
{

bool checkForRawNALFormat(QByteArray &data, bool threeByteStartCode)
{
  if (threeByteStartCode && data.length() > 3 && data.at(0) == (char)0 && data.at(1) == (char)0 &&
      data.at(2) == (char)1)
    return true;
  if (!threeByteStartCode && data.length() > 4 && data.at(0) == (char)0 && data.at(1) == (char)0 &&
      data.at(2) == (char)0 && data.at(3) == (char)1)
    return true;
  return false;
}

bool checkForMp4Format(QByteArray &data)
{
  // Check the ISO mp4 format: Parse the whole data and check if the size bytes per Unit are
  // correct.
  uint64_t posInData = 0;
  while (posInData + 4 <= uint64_t(data.length()))
  {
    auto firstBytes = data.mid(posInData, 4);

    unsigned size = (unsigned char)firstBytes.at(3);
    size += (unsigned char)firstBytes.at(2) << 8;
    size += (unsigned char)firstBytes.at(1) << 16;
    size += (unsigned char)firstBytes.at(0) << 24;
    posInData += 4;

    if (size > 1'000'000'000)
      // A Nal with more then 1GB? This is probably an error.
      return false;
    if (posInData + size > uint64_t(data.length()))
      // Not enough data in the input array to read NAL unit.
      return false;
    posInData += size;
  }
  return true;
}

bool checkForObuFormat(QByteArray &data)
{
  // TODO: We already have an implementation of this in the parser
  //       That should also be used here so we only have one place where we parse OBUs.
  try
  {
    size_t posInData = 0;
    while (posInData + 2 <= size_t(data.length()))
    {
      parser::reader::SubByteReaderLogging reader(
          parser::reader::SubByteReaderLogging::convertToByteVector(data), nullptr, "", posInData);

      QString bitsRead;
      auto    forbiddenBit = reader.readFlag("obu_forbidden_bit");
      if (forbiddenBit)
        return false;
      auto obu_type = reader.readBits("obu_type", 4);
      if (obu_type == 0 || (obu_type >= 9 && obu_type <= 14))
        // RESERVED obu types should not occur (highly unlikely)
        return false;
      auto obu_extension_flag = reader.readFlag("obu_extension_flag");
      auto obu_has_size_field = reader.readFlag("obu_has_size_field");
      reader.readFlag("obu_reserved_1bit", parser::reader::Options().withCheckEqualTo(1));

      if (obu_extension_flag)
      {
        reader.readBits("temporal_id", 3);
        reader.readBits("spatial_id", 2);
        reader.readBits(
            "extension_header_reserved_3bits", 3, parser::reader::Options().withCheckEqualTo(0));
      }
      size_t obu_size;
      if (obu_has_size_field)
      {
        obu_size = reader.readLEB128("obu_size");
      }
      else
      {
        obu_size = (size_t(data.size()) - posInData) - 1 - (obu_extension_flag ? 1 : 0);
      }
      posInData += obu_size + reader.nrBytesRead();
    }
  }
  catch (...)
  {
    return false;
  }
  return true;
}

} // namespace

AVPacketWrapper::AVPacketWrapper(LibraryVersion libVersion, AVPacket *packet)
{
  assert(packet != nullptr);
  this->libVer = libVersion;

  if (this->libVer.avcodec.major == 56)
  {
    auto p    = reinterpret_cast<AVPacket_56 *>(packet);
    p->data   = nullptr;
    p->size   = 0;
    this->pkt = reinterpret_cast<AVPacket *>(p);
  }
  else if (this->libVer.avcodec.major == 57 || this->libVer.avcodec.major == 58)
  {
    auto p    = reinterpret_cast<AVPacket_57_58 *>(packet);
    p->data   = nullptr;
    p->size   = 0;
    this->pkt = reinterpret_cast<AVPacket *>(p);
  }
  else if (this->libVer.avcodec.major == 59)
  {
    auto p    = reinterpret_cast<AVPacket_59 *>(packet);
    p->data   = nullptr;
    p->size   = 0;
    this->pkt = reinterpret_cast<AVPacket *>(p);
  }
  else
    throw std::runtime_error("Invalid library version");
}

void AVPacketWrapper::clear()
{
  this->pkt    = nullptr;
  this->libVer = {};
}

void AVPacketWrapper::setData(QByteArray &set_data)
{
  if (this->libVer.avcodec.major == 56)
  {
    auto p  = reinterpret_cast<AVPacket_56 *>(this->pkt);
    p->data = (uint8_t *)set_data.data();
    p->size = set_data.size();
    data    = p->data;
    size    = p->size;
  }
  else if (this->libVer.avcodec.major == 57 || this->libVer.avcodec.major == 58)
  {
    auto p  = reinterpret_cast<AVPacket_57_58 *>(this->pkt);
    p->data = (uint8_t *)set_data.data();
    p->size = set_data.size();
    data    = p->data;
    size    = p->size;
  }
  else if (this->libVer.avcodec.major == 59)
  {
    auto p  = reinterpret_cast<AVPacket_59 *>(this->pkt);
    p->data = (uint8_t *)set_data.data();
    p->size = set_data.size();
    data    = p->data;
    size    = p->size;
  }
  else
    throw std::runtime_error("Invalid library version");
}

void AVPacketWrapper::setPTS(int64_t pts)
{
  if (this->libVer.avcodec.major == 56)
  {
    auto p    = reinterpret_cast<AVPacket_56 *>(this->pkt);
    p->pts    = pts;
    this->pts = pts;
  }
  else if (this->libVer.avcodec.major == 57 || this->libVer.avcodec.major == 58)
  {
    auto p    = reinterpret_cast<AVPacket_57_58 *>(this->pkt);
    p->pts    = pts;
    this->pts = pts;
  }
  else if (this->libVer.avcodec.major == 59)
  {
    auto p    = reinterpret_cast<AVPacket_59 *>(this->pkt);
    p->pts    = pts;
    this->pts = pts;
  }
  else
    throw std::runtime_error("Invalid library version");
}

void AVPacketWrapper::setDTS(int64_t dts)
{
  if (this->libVer.avcodec.major == 56)
  {
    auto p    = reinterpret_cast<AVPacket_56 *>(this->pkt);
    p->dts    = dts;
    this->dts = dts;
  }
  else if (this->libVer.avcodec.major == 57 || this->libVer.avcodec.major == 58)
  {
    auto p    = reinterpret_cast<AVPacket_57_58 *>(this->pkt);
    p->dts    = dts;
    this->dts = dts;
  }
  else if (this->libVer.avcodec.major == 59)
  {
    auto p    = reinterpret_cast<AVPacket_59 *>(this->pkt);
    p->dts    = dts;
    this->dts = dts;
  }
  else
    throw std::runtime_error("Invalid library version");
}

AVPacket *AVPacketWrapper::getPacket()
{
  this->update();
  return this->pkt;
}

int AVPacketWrapper::getStreamIndex()
{
  this->update();
  return this->stream_index;
}

int64_t AVPacketWrapper::getPTS()
{
  this->update();
  return this->pts;
}

int64_t AVPacketWrapper::getDTS()
{
  this->update();
  return this->dts;
}

int64_t AVPacketWrapper::getDuration()
{
  this->update();
  return this->duration;
}

int AVPacketWrapper::getFlags()
{
  this->update();
  return this->flags;
}

bool AVPacketWrapper::getFlagKeyframe()
{
  this->update();
  return this->flags & AV_PKT_FLAG_KEY;
}

bool AVPacketWrapper::getFlagCorrupt()
{
  this->update();
  return this->flags & AV_PKT_FLAG_CORRUPT;
}

bool AVPacketWrapper::getFlagDiscard()
{
  this->update();
  return this->flags & AV_PKT_FLAG_DISCARD;
}

uint8_t *AVPacketWrapper::getData()
{
  this->update();
  return this->data;
}

int AVPacketWrapper::getDataSize()
{
  this->update();
  return this->size;
}

PacketType AVPacketWrapper::getPacketType() const
{
  return this->packetType;
}

void AVPacketWrapper::setPacketType(PacketType packetType)
{
  this->packetType = packetType;
}

PacketDataFormat AVPacketWrapper::guessDataFormatFromData()
{
  if (this->packetFormat != PacketDataFormat::Unknown)
    return this->packetFormat;

  auto avpacketData = QByteArray::fromRawData((const char *)(getData()), getDataSize());
  if (avpacketData.length() < 4)
  {
    this->packetFormat = PacketDataFormat::Unknown;
    return this->packetFormat;
  }

  // AVPacket data can be in one of two formats:
  // 1: The raw annexB format with start codes (0x00000001 or 0x000001)
  // 2: ISO/IEC 14496-15 mp4 format: The first 4 bytes determine the size of the NAL unit followed
  // by the payload We will try to guess the format of the data from the data in this AVPacket. This
  // should always work unless a format is used which we did not encounter so far (which is not
  // listed above) Also I think this should be identical for all packets in a bitstream.
  if (checkForRawNALFormat(avpacketData, false))
    this->packetFormat = PacketDataFormat::RawNAL;
  else if (checkForMp4Format(avpacketData))
    this->packetFormat = PacketDataFormat::MP4;
  else if (checkForObuFormat(avpacketData))
    this->packetFormat = PacketDataFormat::OBU;
  else if (checkForRawNALFormat(avpacketData, true))
    this->packetFormat = PacketDataFormat::RawNAL;

  return this->packetFormat;
}

void AVPacketWrapper::update()
{
  if (this->pkt == nullptr)
    return;

  if (this->libVer.avcodec.major == 56)
  {
    auto p = reinterpret_cast<AVPacket_56 *>(this->pkt);

    this->buf             = p->buf;
    this->pts             = p->pts;
    this->dts             = p->dts;
    this->data            = p->data;
    this->size            = p->size;
    this->stream_index    = p->stream_index;
    this->flags           = p->flags;
    this->side_data       = p->side_data;
    this->side_data_elems = p->side_data_elems;
    this->duration        = p->duration;
    this->pos             = p->pos;
  }
  else if (this->libVer.avcodec.major == 57 || //
           this->libVer.avcodec.major == 58)
  {
    auto p = reinterpret_cast<AVPacket_57_58 *>(this->pkt);

    this->buf             = p->buf;
    this->pts             = p->pts;
    this->dts             = p->dts;
    this->data            = p->data;
    this->size            = p->size;
    this->stream_index    = p->stream_index;
    this->flags           = p->flags;
    this->side_data       = p->side_data;
    this->side_data_elems = p->side_data_elems;
    this->duration        = p->duration;
    this->pos             = p->pos;
  }
  else if (this->libVer.avcodec.major == 59)
  {
    auto p = reinterpret_cast<AVPacket_59 *>(this->pkt);

    this->buf             = p->buf;
    this->pts             = p->pts;
    this->dts             = p->dts;
    this->data            = p->data;
    this->size            = p->size;
    this->stream_index    = p->stream_index;
    this->flags           = p->flags;
    this->side_data       = p->side_data;
    this->side_data_elems = p->side_data_elems;
    this->duration        = p->duration;
    this->pos             = p->pos;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg
