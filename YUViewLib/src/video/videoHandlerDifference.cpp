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

#include "videoHandlerDifference.h"

#include <QPainter>
#include <algorithm>

#include <common/Functions.h>
#include <video/yuv/videoHandlerYUV.h>

#include "ui_FrameHandler.h"
#include "ui_videoHandlerDifference.h"

namespace video
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERDIFFERENCE_DEBUG_LOADING 0
#if VIDEOHANDLERDIFFERENCE_DEBUG_LOADING && !NDEBUG
#define DEBUG_VIDEO qDebug
#else
#define DEBUG_VIDEO(fmt, ...) ((void)0)
#endif

namespace
{

struct PositionAndPartIndex
{
  PositionAndPartIndex(Position position, int partIndex)
  {
    this->position  = position;
    this->partIndex = partIndex;
  };
  Position position{};
  int      partIndex;
};

std::optional<PositionAndPartIndex> findFirstDifferenceInHEVCBlock(Position      pos,
                                                                   int           blockSize,
                                                                   const QImage &diffImg,
                                                                   bool          markDifference)
{
  const auto frameSize = diffImg.size();
  if (pos.x >= int(frameSize.width()) || pos.y >= int(frameSize.height()))
    return {};

  if (blockSize == 4)
  {
    // Check for a difference
    for (auto subX = pos.x; subX < pos.x + 4; subX++)
    {
      for (auto subY = pos.y; subY < pos.y + 4; subY++)
      {
        auto rgb = diffImg.pixel(QPoint(subX, subY));

        if (markDifference)
        {
          // Black means no difference
          if (qRed(rgb) != 0 || qGreen(rgb) != 0 || qBlue(rgb) != 0)
            return PositionAndPartIndex(pos, 0);
        }
        else
        {
          // TODO: Double check if this is always true
          // Do other values also convert to RGB(130,130,130) ?
          // What about 10 bit input material
          auto red   = qRed(rgb);
          auto green = qGreen(rgb);
          auto blue  = qBlue(rgb);

          if (red != 130 || green != 130 || blue != 130)
            return PositionAndPartIndex(pos, 0);
        }
      }
    }
  }
  else
  {
    // Walk further into the hierarchy
    const int bs                 = blockSize / 2;
    const int nr4x4BlocksPerPart = (bs / 4) * (bs / 4);
    if (auto diff = findFirstDifferenceInHEVCBlock(pos, bs, diffImg, markDifference))
      return diff;
    if (auto diff = findFirstDifferenceInHEVCBlock(
            Position(pos.x + bs, pos.y), bs, diffImg, markDifference))
      return PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart);
    if (auto diff = findFirstDifferenceInHEVCBlock(
            Position(pos.x, pos.y + bs), bs, diffImg, markDifference))
      return PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart * 2);
    if (auto diff = findFirstDifferenceInHEVCBlock(
            Position(pos.x + bs, pos.y + bs), bs, diffImg, markDifference))
      return PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart * 3);
  }
  return {};
}

inline int
getValueFromSource(const unsigned char *src, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx * 2] << 8 | src[idx * 2 + 1]
                       : src[idx * 2] | src[idx * 2 + 1] << 8;
  else
    // Just read one byte
    return src[idx];
}

std::optional<PositionAndPartIndex>
findFirstDifferenceInHEVCBlockYUV(Position                   pos,
                                  int                        blockSize,
                                  const QByteArray &         diffYUV,
                                  const Size &               frameSize,
                                  const yuv::PixelFormatYUV &diffYUVFormat)
{
  if (pos.x >= int(frameSize.width) || pos.y >= int(frameSize.height))
    return {};

  // The items can be of different size (we then calculate the difference of the top left aligned
  // part)
  const auto w_in = frameSize.width;
  const auto h_in = frameSize.height;

  // Get subsampling modes (they are identical for both inputs and the output)
  const auto subH = diffYUVFormat.getSubsamplingHor();
  const auto subV = diffYUVFormat.getSubsamplingVer();

  const bool bigEndian = diffYUVFormat.isBigEndian();
  const auto bps_in    = diffYUVFormat.getBitsPerSample();
  const auto diffZero  = 128 << (bps_in - 8);

  // Get pointers to the inputs
  const auto componentSizeLuma_In   = w_in * h_in;
  const auto componentSizeChroma_In = (w_in / subH) * (h_in / subV);
  const auto nrBytesLumaPlane_In    = bps_in > 8 ? 2 * componentSizeLuma_In : componentSizeLuma_In;
  const auto nrBytesChromaPlane_In =
      bps_in > 8 ? 2 * componentSizeChroma_In : componentSizeChroma_In;

  // Calculate Luma sample difference
  const int stride_in = bps_in > 8 ? w_in * 2 : w_in; // How many bytes to the next y line?
  const int strideC_in =
      w_in / subH * (bps_in > 8 ? 2 : 1); // How many bytes to the next U/V y line

  if (blockSize == 4)
  {
    auto srcY1 = (unsigned char *)diffYUV.data();
    auto srcU1 = (diffYUVFormat.getPlaneOrder() == yuv::PlaneOrder::YUV ||
                  diffYUVFormat.getPlaneOrder() == yuv::PlaneOrder::YUVA)
                     ? srcY1 + nrBytesLumaPlane_In
                     : srcY1 + nrBytesLumaPlane_In + nrBytesChromaPlane_In;
    auto srcV1 = (diffYUVFormat.getPlaneOrder() == yuv::PlaneOrder::YUV ||
                  diffYUVFormat.getPlaneOrder() == yuv::PlaneOrder::YUVA)
                     ? srcY1 + nrBytesLumaPlane_In + nrBytesChromaPlane_In
                     : srcY1 + nrBytesLumaPlane_In;

    // adjust source pointer according to block position
    srcY1 += pos.y * stride_in;
    srcU1 += pos.y / subV * strideC_in;
    srcV1 += pos.y / subV * strideC_in;

    // Check for a difference
    for (int subY = pos.y; subY < pos.y + 4; subY++)
    {

      for (int subX = pos.x; subX < pos.x + 4; subX++)
      {

        auto val1 = getValueFromSource(srcY1, subX, bps_in, bigEndian);
        if (val1 != diffZero)
          return PositionAndPartIndex(pos, 0);

        // is this a position at which we have a chroma sample?
        if (subX % subH == 0 && subY % subV == 0 && subX * subV < int(w_in))
        {
          auto valU1 = getValueFromSource(srcU1, subX, bps_in, bigEndian);
          auto valV1 = getValueFromSource(srcV1, subX, bps_in, bigEndian);
          if (valU1 != diffZero || valV1 != diffZero)
            return PositionAndPartIndex(pos, 0);
        }
      }

      // Goto the next y line
      srcY1 += stride_in;

      // is this a position at which we have a chroma line?
      if (subY % subV == 0)
      {
        // Goto the next y line
        srcU1 += strideC_in;
        srcV1 += strideC_in;
      }
    }
  }
  else
  {
    // Walk further into the hierarchy
    const int bs                 = blockSize / 2;
    const int nr4x4BlocksPerPart = (bs / 4) * (bs / 4);
    if (auto diff = findFirstDifferenceInHEVCBlockYUV(pos, bs, diffYUV, frameSize, diffYUVFormat))
      return diff;
    if (auto diff = findFirstDifferenceInHEVCBlockYUV(
            Position(pos.x + bs, pos.y), bs, diffYUV, frameSize, diffYUVFormat))
      PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart);
    if (auto diff = findFirstDifferenceInHEVCBlockYUV(
            Position(pos.x, pos.y + bs), bs, diffYUV, frameSize, diffYUVFormat))
      PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart * 2);
    if (auto diff = findFirstDifferenceInHEVCBlockYUV(
            Position(pos.x + bs, pos.y + bs), bs, diffYUV, frameSize, diffYUVFormat))
      PositionAndPartIndex(diff->position, diff->partIndex + nr4x4BlocksPerPart * 3);
  }
  return {};
}

} // namespace

videoHandlerDifference::videoHandlerDifference() : videoHandler()
{
  this->ui = std::make_unique<SafeUi<Ui::videoHandlerDifference>>();
}

void videoHandlerDifference::drawDifferenceFrame(QPainter *painter,
                                                 int       frameIdx,
                                                 double    zoomFactor,
                                                 bool      drawRawValues)
{
  if (!this->inputsValid())
    return;

  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != this->currentImageIndex)
  {
    // The current buffer is out of date. Update it.

    // Check the double buffer
    if (frameIdx == this->doubleBufferImageFrameIndex)
    {
      this->currentImage      = this->doubleBufferImage;
      this->currentImageIndex = frameIdx;
      DEBUG_VIDEO("videoHandler::drawFrame %d loaded from double buffer", frameIdx);
    }
    else
    {
      QMutexLocker lock(&this->imageCacheAccess);
      if (this->cacheValid && this->imageCache.contains(frameIdx))
      {
        this->currentImage      = this->imageCache[frameIdx];
        this->currentImageIndex = frameIdx;
        DEBUG_VIDEO("videoHandler::drawFrame %d loaded from cache", frameIdx);
      }
    }
  }

  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(QSize(frameSize.width * zoomFactor, frameSize.height * zoomFactor));
  videoRect.moveCenter(QPoint(0, 0));

  // Draw the current image (currentImage)
  {
    QMutexLocker lock(&this->currentImageSetMutex);
    painter->drawImage(videoRect, currentImage);
  }

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    this->inputVideo[0]->drawPixelValues(
        painter, frameIdx, videoRect, zoomFactor, inputVideo[1], this->markDifference, frameIdx);
  }
}

void videoHandlerDifference::loadFrameDifference(int frameIndex, bool)
{
  // Calculate the difference between the inputVideos
  if (!this->inputsValid())
    return;

  this->differenceInfoList.clear();

  // Check if the second item is a video and the first one is not. In that case,
  // make sure that the right frame is loaded for the video item.
  auto video0 = dynamic_cast<videoHandler *>(this->inputVideo[0].data());
  auto video1 = dynamic_cast<videoHandler *>(this->inputVideo[1].data());
  if (video0 == nullptr && video1 != nullptr && video1->getCurrentImageIndex() != frameIndex)
    video1->loadFrame(frameIndex);

  // Calculate the difference
  auto newFrame = this->inputVideo[0]->calculateDifference(this->inputVideo[1],
                                                           frameIndex,
                                                           frameIndex,
                                                           this->differenceInfoList,
                                                           this->amplificationFactor,
                                                           this->markDifference);

  if (!newFrame.isNull())
  {
    // The new difference frame is ready
    this->currentImageIndex = frameIndex;
    QMutexLocker lock(&this->currentImageSetMutex);
    this->currentImage = newFrame;
  }
}

bool videoHandlerDifference::inputsValid() const
{
  if (this->inputVideo[0].isNull() || this->inputVideo[1].isNull())
    return false;

  if (!this->inputVideo[0]->isFormatValid() || !this->inputVideo[1]->isFormatValid())
    return false;

  return true;
}

void videoHandlerDifference::setInputVideos(FrameHandler *childVideo0, FrameHandler *childVideo1)
{
  if (this->inputVideo[0] != childVideo0 || inputVideo[1] != childVideo1)
  {
    // Something changed
    this->inputVideo[0] = childVideo0;
    this->inputVideo[1] = childVideo1;

    if (inputsValid())
    {
      // We have two valid video "children"

      // Get the frame size of the difference (min in x and y direction), and set it.
      auto size0 = this->inputVideo[0]->getFrameSize();
      auto size1 = this->inputVideo[1]->getFrameSize();
      auto diffSize =
          Size(std::min(size0.width, size1.width), std::min(size0.height, size1.height));
      this->setFrameSize(diffSize);
    }

    // If something changed, we might need a redraw
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
}

QStringPairList videoHandlerDifference::getPixelValues(const QPoint &pixelPos,
                                                       int           frameIdx,
                                                       FrameHandler *,
                                                       const int frameIdx1)
{
  if (!this->inputsValid())
    return {};

  return this->inputVideo[0]->getPixelValues(pixelPos, frameIdx, this->inputVideo[1], frameIdx1);
}

void videoHandlerDifference::setFormatFromSizeAndName(
    const Size, int, DataLayout, int64_t, const QFileInfo &)
{
  assert(false);
}

QLayout *videoHandlerDifference::createDifferenceHandlerControls()
{
  Q_ASSERT_X(
      !this->ui->created(), "createResampleHandlerControls", "Controls must only be created once");

  this->ui->setupUi();

  // Set all the values of the properties widget to the values of this class
  this->ui->markDifferenceCheckBox->setChecked(markDifference);
  this->ui->amplificationFactorSpinBox->setValue(amplificationFactor);
  this->ui->codingOrderComboBox->addItems(QStringList() << "HEVC");
  this->ui->codingOrderComboBox->setCurrentIndex((int)codingOrder);

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(this->ui->markDifferenceCheckBox,
          &QCheckBox::stateChanged,
          this,
          &videoHandlerDifference::slotDifferenceControlChanged);
  connect(this->ui->codingOrderComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &videoHandlerDifference::slotDifferenceControlChanged);
  connect(this->ui->amplificationFactorSpinBox,
          QOverload<int>::of(&QSpinBox::valueChanged),
          this,
          &videoHandlerDifference::slotDifferenceControlChanged);

  return this->ui->topVBoxLayout;
}

void videoHandlerDifference::slotDifferenceControlChanged()
{
  // The control that caused the slot to be called
  auto sender = QObject::sender();

  if (sender == this->ui->markDifferenceCheckBox)
  {
    this->markDifference = this->ui->markDifferenceCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and emit the signal that something has
    // changed
    this->currentImageIndex = -1;
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
  else if (sender == this->ui->codingOrderComboBox)
  {
    this->codingOrder = (CodingOrder)this->ui->codingOrderComboBox->currentIndex();

    // The calculation of the first difference in coding order changed but no redraw is necessary
    emit signalHandlerChanged(false, RECACHE_NONE);
  }
  else if (sender == this->ui->amplificationFactorSpinBox)
  {
    this->amplificationFactor = this->ui->amplificationFactorSpinBox->value();

    // Set the current frame in the buffer to be invalid and emit the signal that something has
    // changed
    this->currentImageIndex = -1;
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
}

void videoHandlerDifference::reportFirstDifferencePosition(QList<InfoItem> &infoList) const
{
  if (!this->inputsValid())
    return;

  if (functions::clipToUnsigned(this->currentImage.width()) != this->frameSize.width ||
      functions::clipToUnsigned(this->currentImage.height()) != this->frameSize.height)
    return;

  if (this->codingOrder == CodingOrder::HEVC)
  {
    // Assume the following:
    // - The picture is split into LCUs of 64x64 pixels which are scanned in raster scan
    // - Each LCU is scanned in a hierarchical tree until the smallest unit size (4x4 pixels) is
    // reached This is exactly what we are going to do here now

    int widthLCU  = (this->frameSize.width + 63) / 64; // Round up
    int heightLCU = (this->frameSize.height + 63) / 64;

    for (int y = 0; y < heightLCU; y++)
    {
      for (int x = 0; x < widthLCU; x++)
      {
        // Go into LCUs in a tree fashion

        auto videoYUV0 = dynamic_cast<yuv::videoHandlerYUV *>(this->inputVideo[0].data());
        if (videoYUV0 != nullptr && videoYUV0->isDiffReady())
        {
          // find first difference using YUV instead of QImage. The latter does not work for 10bit
          // videos and very small differences, since it only supports 8bit
          if (auto posAndIndex = findFirstDifferenceInHEVCBlockYUV(Position(x * 64, y * 64),
                                                                   64,
                                                                   videoYUV0->getDiffYUV(),
                                                                   this->frameSize,
                                                                   videoYUV0->getDiffYUVFormat()))
          {
            infoList.append(InfoItem("First diff LCU", QString::number(y * widthLCU + x)));
            infoList.append(InfoItem(
                "First diff X,Y",
                QString("%1,%2").arg(posAndIndex->position.x).arg(posAndIndex->position.y)));
            infoList.append(
                InfoItem("First diff partIndex", QString::number(posAndIndex->partIndex)));
            return;
          }
        }
        else
        {
          if (auto posAndIndex = findFirstDifferenceInHEVCBlock(
                  Position(x * 64, y * 64), 64, currentImage, this->markDifference))
          {
            infoList.append(InfoItem("First diff LCU", QString::number(y * widthLCU + x)));
            infoList.append(InfoItem(
                "First diff X,Y",
                QString("%1,%2").arg(posAndIndex->position.x).arg(posAndIndex->position.y)));
            infoList.append(
                InfoItem("First diff partIndex", QString::number(posAndIndex->partIndex)));
            return;
          }
        }
      }
    }
  }

  // No difference was found
  infoList.append(InfoItem("Difference", "Frames are identical"));
}

void videoHandlerDifference::savePlaylist(YUViewDomElement &element) const
{
  FrameHandler::savePlaylist(element);

  if (this->amplificationFactor != 1)
    element.appendProperiteChild("amplificationFactor", QString::number(this->amplificationFactor));
  if (this->markDifference)
    element.appendProperiteChild("markDifference", functions::boolToString(this->markDifference));
}

void videoHandlerDifference::loadPlaylist(const YUViewDomElement &element)
{
  FrameHandler::loadPlaylist(element);

  auto amplification = element.findChildValue("amplificationFactor");
  if (!amplification.isEmpty())
    this->amplificationFactor = amplification.toInt();

  if (element.findChildValue("markDifference") == "True")
    this->markDifference = true;
}

ItemLoadingState videoHandlerDifference::needsLoadingRawValues(int frameIndex)
{
  if (auto video = dynamic_cast<videoHandler *>(this->inputVideo[0].data()))
    if (video->needsLoadingRawValues(frameIndex) == ItemLoadingState::LoadingNeeded)
      return ItemLoadingState::LoadingNeeded;

  if (auto video = dynamic_cast<videoHandler *>(this->inputVideo[1].data()))
    if (video->needsLoadingRawValues(frameIndex) == ItemLoadingState::LoadingNeeded)
      return ItemLoadingState::LoadingNeeded;

  return ItemLoadingState::LoadingNotNeeded;
}

} // namespace video
