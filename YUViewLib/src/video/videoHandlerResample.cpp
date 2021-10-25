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

#include "videoHandlerResample.h"

#include <QPainter>
#include <QPushButton>
#include <algorithm>

#include "videoHandlerYUV.h"

namespace video
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERRESAMPLE_DEBUG_LOADING 0
#if VIDEOHANDLERRESAMPLE_DEBUG_LOADING && !NDEBUG
#define DEBUG_RESAMPLE qDebug
#else
#define DEBUG_RESAMPLE(fmt, ...) ((void)0)
#endif

videoHandlerResample::videoHandlerResample() : videoHandler()
{
}

void videoHandlerResample::drawFrame(QPainter *painter,
                                     int       frameIndex,
                                     double    zoomFactor,
                                     bool      drawRawValues)
{
  auto mappedIndex = this->mapFrameIndex(frameIndex);
  DEBUG_RESAMPLE("videoHandlerResample::drawFrame idx %d", mappedIndex);
  videoHandler::drawFrame(painter, mappedIndex, zoomFactor, drawRawValues);
}

QImage videoHandlerResample::calculateDifference(FrameHandler *   item2,
                                                 const int        frameIndex0,
                                                 const int        frameIndex1,
                                                 QList<InfoItem> &differenceInfoList,
                                                 const int        amplificationFactor,
                                                 const bool       markDifference)
{
  if (!this->inputValid())
    return {};

  auto mappedIndex = this->mapFrameIndex(frameIndex0);
  return videoHandler::calculateDifference(
      item2, mappedIndex, frameIndex1, differenceInfoList, amplificationFactor, markDifference);
}

ItemLoadingState videoHandlerResample::needsLoading(int frameIndex, bool loadRawValues)
{
  auto mappedIndex = this->mapFrameIndex(frameIndex);
  return videoHandler::needsLoading(mappedIndex, loadRawValues);
}

void videoHandlerResample::loadResampledFrame(int frameIndex, bool loadToDoubleBuffer)
{
  if (!this->inputValid())
    return;

  auto mappedIndex = this->mapFrameIndex(frameIndex);

  auto video = dynamic_cast<videoHandler *>(this->inputVideo.data());
  if (video && video->getCurrentImageIndex() != mappedIndex)
    video->loadFrame(mappedIndex);

  auto interpolationMode = (this->interpolation == Interpolation::Bilinear)
                               ? Qt::SmoothTransformation
                               : Qt::FastTransformation;

  auto qFrameSize = QSize(this->getFrameSize().width, this->getFrameSize().height);
  auto newFrame   = this->inputVideo->getCurrentFrameAsImage().scaled(
      qFrameSize, Qt::IgnoreAspectRatio, interpolationMode);

  if (newFrame.isNull())
    return;

  if (loadToDoubleBuffer)
  {
    doubleBufferImage           = newFrame;
    doubleBufferImageFrameIndex = mappedIndex;
    DEBUG_RESAMPLE("videoHandlerResample::loadResampledFrame Loaded frame %d to double buffer",
                   mappedIndex);
  }
  else
  {
    // The new difference frame is ready
    QMutexLocker lock(&this->currentImageSetMutex);
    currentImage      = newFrame;
    currentImageIndex = mappedIndex;
    DEBUG_RESAMPLE("videoHandlerResample::loadResampledFrame Loaded frame %d to current buffer",
                   mappedIndex);
  }
}

bool videoHandlerResample::inputValid() const
{
  return (!this->inputVideo.isNull() && this->inputVideo->isFormatValid());
}

void videoHandlerResample::setInputVideo(FrameHandler *childVideo)
{
  if (this->inputVideo == childVideo)
    return;

  this->inputVideo = childVideo;
  DEBUG_RESAMPLE("videoHandlerResample::loadResampledFrame setting new video");

  if (this->inputValid())
    this->setFrameSize(childVideo->getFrameSize());

  emit signalHandlerChanged(true, RECACHE_NONE);
}

void videoHandlerResample::setScaledSize(Size scaledSize)
{
  if (!scaledSize.isValid())
    return;

  this->scaledSize = scaledSize;
  this->setFrameSize(scaledSize);

  this->invalidateAllBuffers();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerResample::setInterpolation(Interpolation interpolation)
{
  this->interpolation = interpolation;
  this->invalidateAllBuffers();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerResample::setCutAndSample(indexRange startEnd, int sampling)
{
  if (startEnd.first > startEnd.second || sampling < 1)
    return;

  this->cutRange = startEnd;
  this->sampling = sampling;

  this->invalidateAllBuffers();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerResample::setFormatFromSizeAndName(
    const Size, int, DataLayout, int64_t, const QFileInfo &)
{
  assert(false);
}

int videoHandlerResample::mapFrameIndex(int frameIndex)
{
  auto mappedIndex = (frameIndex * this->sampling) + this->cutRange.first;
  DEBUG_RESAMPLE(
      "videoHandlerResample::mapFrameIndex frameIndex %d mapped to %d", frameIndex, mappedIndex);
  return mappedIndex;
}

} // namespace video
