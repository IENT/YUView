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

#include "AVFrameWrapper.h"
#include <stdexcept>

namespace FFmpeg
{

namespace
{

// AVFrames is part of AVUtil
typedef struct AVFrame_54
{
  uint8_t *     data[AV_NUM_DATA_POINTERS];
  int           linesize[AV_NUM_DATA_POINTERS];
  uint8_t **    extended_data;
  int           width, height;
  int           nb_samples;
  int           format;
  int           key_frame;
  AVPictureType pict_type;
  uint8_t *     base[AV_NUM_DATA_POINTERS];
  AVRational    sample_aspect_ratio;
  int64_t       pts;
  int64_t       pkt_pts;
  int64_t       pkt_dts;
  int           coded_picture_number;
  int           display_picture_number;
  int           quality;
  // Actually, there is more here, but the variables above are the only we need.
} AVFrame_54;

typedef struct AVFrame_55_56
{
  uint8_t *     data[AV_NUM_DATA_POINTERS];
  int           linesize[AV_NUM_DATA_POINTERS];
  uint8_t **    extended_data;
  int           width, height;
  int           nb_samples;
  int           format;
  int           key_frame;
  AVPictureType pict_type;
  AVRational    sample_aspect_ratio;
  int64_t       pts;
  int64_t       pkt_pts;
  int64_t       pkt_dts;
  int           coded_picture_number;
  int           display_picture_number;
  int           quality;
  // Actually, there is more here, but the variables above are the only we need.
} AVFrame_55_56;

typedef struct AVFrame_57_58
{
  uint8_t *                          data[AV_NUM_DATA_POINTERS];
  int                                linesize[AV_NUM_DATA_POINTERS];
  uint8_t **                         extended_data;
  int                                width, height;
  int                                nb_samples;
  int                                format;
  int                                key_frame;
  AVPictureType                      pict_type;
  AVRational                         sample_aspect_ratio;
  int64_t                            pts;
  int64_t                            pkt_dts;
  AVRational                         time_base;
  int                                coded_picture_number;
  int                                display_picture_number;
  int                                quality;
  void *                             opaque;
  int                                repeat_pict;
  int                                interlaced_frame;
  int                                top_field_first;
  int                                palette_has_changed;
  int64_t                            reordered_opaque;
  int                                sample_rate;
  uint64_t                           channel_layout;
  AVBufferRef *                      buf[AV_NUM_DATA_POINTERS];
  AVBufferRef **                     extended_buf;
  int                                nb_extended_buf;
  AVFrameSideData **                 side_data;
  int                                nb_side_data;
  int                                flags;
  enum AVColorRange                  color_range;
  enum AVColorPrimaries              color_primaries;
  enum AVColorTransferCharacteristic color_trc;
  enum AVColorSpace                  colorspace;
  enum AVChromaLocation              chroma_location;
  int64_t                            best_effort_timestamp;
  int64_t                            pkt_pos;
  int64_t                            pkt_duration;
  AVDictionary *                     metadata;

  // Actually, there is more here, but the variables above are the only we need.
} AVFrame_57_58;

} // namespace

AVFrameWrapper::AVFrameWrapper(AVFrame *frame, const LibraryVersions &libraryVersions)
{
  this->frame           = frame;
  this->libraryVersions = libraryVersions;
}

void AVFrameWrapper::clear()
{
  this->frame           = nullptr;
  this->libraryVersions = {};
}

uint8_t *AVFrameWrapper::getData(int component)
{
  this->update();
  return this->data[component];
}

int AVFrameWrapper::getLineSize(int component)
{
  this->update();
  return this->linesize[component];
}

AVFrame *AVFrameWrapper::getFrame() const
{
  return this->frame;
}

int AVFrameWrapper::getWidth()
{
  this->update();
  return this->width;
}

int AVFrameWrapper::getHeight()
{
  this->update();
  return this->height;
}

Size AVFrameWrapper::getSize()
{
  this->update();
  return Size(width, height);
}

int AVFrameWrapper::getPTS()
{
  this->update();
  return this->pts;
}

AVPictureType AVFrameWrapper::getPictType()
{
  this->update();
  return this->pict_type;
}

int AVFrameWrapper::getKeyFrame()
{
  this->update();
  return this->key_frame;
}

AVDictionary *AVFrameWrapper::getMetadata()
{
  assert(this->libraryVersions.avutil.major >= 57);
  this->update();
  return this->metadata;
}

void AVFrameWrapper::update()
{
  if (this->frame == nullptr)
    return;

  if (this->libraryVersions.avutil.major == 54)
  {
    auto p = reinterpret_cast<AVFrame_54 *>(this->frame);
    for (auto i = 0; i < AV_NUM_DATA_POINTERS; ++i)
    {
      this->data[i]     = p->data[i];
      this->linesize[i] = p->linesize[i];
    }
    this->width                  = p->width;
    this->height                 = p->height;
    this->nb_samples             = p->nb_samples;
    this->format                 = p->format;
    this->key_frame              = p->key_frame;
    this->pict_type              = p->pict_type;
    this->sample_aspect_ratio    = p->sample_aspect_ratio;
    this->pts                    = p->pts;
    this->pkt_pts                = p->pkt_pts;
    this->pkt_dts                = p->pkt_dts;
    this->coded_picture_number   = p->coded_picture_number;
    this->display_picture_number = p->display_picture_number;
    this->quality                = p->quality;
  }
  else if (this->libraryVersions.avutil.major == 55 || //
           this->libraryVersions.avutil.major == 56)
  {
    auto p = reinterpret_cast<AVFrame_55_56 *>(this->frame);
    for (auto i = 0; i < AV_NUM_DATA_POINTERS; ++i)
    {
      this->data[i]     = p->data[i];
      this->linesize[i] = p->linesize[i];
    }
    this->width                  = p->width;
    this->height                 = p->height;
    this->nb_samples             = p->nb_samples;
    this->format                 = p->format;
    this->key_frame              = p->key_frame;
    this->pict_type              = p->pict_type;
    this->sample_aspect_ratio    = p->sample_aspect_ratio;
    this->pts                    = p->pts;
    this->pkt_pts                = p->pkt_pts;
    this->pkt_dts                = p->pkt_dts;
    this->coded_picture_number   = p->coded_picture_number;
    this->display_picture_number = p->display_picture_number;
    this->quality                = p->quality;
  }
  else if (this->libraryVersions.avutil.major == 57 || //
           this->libraryVersions.avutil.major == 58)
  {
    auto p = reinterpret_cast<AVFrame_57_58 *>(this->frame);
    for (auto i = 0; i < AV_NUM_DATA_POINTERS; ++i)
    {
      this->data[i]     = p->data[i];
      this->linesize[i] = p->linesize[i];
    }
    this->width                  = p->width;
    this->height                 = p->height;
    this->nb_samples             = p->nb_samples;
    this->format                 = p->format;
    this->key_frame              = p->key_frame;
    this->pict_type              = p->pict_type;
    this->sample_aspect_ratio    = p->sample_aspect_ratio;
    this->pts                    = p->pts;
    this->pkt_pts                = -1;
    this->pkt_dts                = p->pkt_dts;
    this->coded_picture_number   = p->coded_picture_number;
    this->display_picture_number = p->display_picture_number;
    this->quality                = p->quality;
    this->metadata               = p->metadata;
  }
  else
    throw std::runtime_error("Invalid library version");
}

} // namespace FFmpeg
