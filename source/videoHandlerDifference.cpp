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
#include "videoHandlerYUV.h"

#include <algorithm>
#include <QPainter>

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERDIFFERENCE_DEBUG_LOADING 0
#if VIDEOHANDLERDIFFERENCE_DEBUG_LOADING && !NDEBUG
#define DEBUG_VIDEO qDebug
#else
#define DEBUG_VIDEO(fmt,...) ((void)0)
#endif

videoHandlerDifference::videoHandlerDifference() : videoHandler()
{
  markDifference = false;
  amplificationFactor = 1;
  codingOrder = CodingOrder_HEVC;
}

void videoHandlerDifference::drawDifferenceFrame(QPainter *painter, int frameIdx, int frameIdxItem0, int frameIdxItem1, double zoomFactor, bool drawRawValues)
{
  if (!inputsValid())
    return;

  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != currentImageIdx)
  {
    // The current buffer is out of date. Update it.

    // Check the double buffer
    if (frameIdx == doubleBufferImageFrameIdx)
    {
      currentImage = doubleBufferImage;
      currentImageIdx = frameIdx;
      DEBUG_VIDEO("videoHandler::drawFrame %d loaded from double buffer", frameIdx);
    }
    else
    {
      QMutexLocker lock(&imageCacheAccess);
      if (cacheValid && imageCache.contains(frameIdx))
      {
        currentImage = imageCache[frameIdx];
        currentImageIdx = frameIdx;
        DEBUG_VIDEO("videoHandler::drawFrame %d loaded from cache", frameIdx);
      }
    }
  }

  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(frameSize * zoomFactor);
  videoRect.moveCenter(QPoint(0, 0));

  // Draw the current image (currentImage)
  currentImageSetMutex.lock();
  painter->drawImage(videoRect, currentImage);
  currentImageSetMutex.unlock();

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    inputVideo[0]->drawPixelValues(painter, frameIdxItem0, videoRect, zoomFactor, inputVideo[1], this->markDifference, frameIdxItem1);
  }
}

void videoHandlerDifference::loadFrameDifference(int frameIndex, int frameIndex0, int frameIndex1, bool loadToDoubleBuffer)
{
  // No double buffering for difference items
  Q_UNUSED(loadToDoubleBuffer);

  // Calculate the difference between the inputVideos
  if (!inputsValid())
    return;
  
  differenceInfoList.clear();

  // Check if the second item is a video and the first one is not. In that case,
  // make sure that the right frame is loaded for the video item.
  videoHandler* video0 = dynamic_cast<videoHandler*>(inputVideo[0].data());
  videoHandler* video1 = dynamic_cast<videoHandler*>(inputVideo[1].data());
  if (video0 == nullptr && video1 != nullptr && video1->getCurrentImageIndex() != frameIndex1)
    video1->loadFrame(frameIndex1);
  
  // Calculate the difference  
  QImage newFrame = inputVideo[0]->calculateDifference(inputVideo[1], frameIndex0, frameIndex1, differenceInfoList, amplificationFactor, markDifference);

  if (!newFrame.isNull())
  {
    // The new difference frame is ready
    currentImageIdx = frameIndex;
    currentImageSetMutex.lock();
    currentImage = newFrame;
    currentImageSetMutex.unlock();
  }
}

bool videoHandlerDifference::inputsValid() const
{
  if (inputVideo[0].isNull() || inputVideo[1].isNull())
    return false;
  
  if (!inputVideo[0]->isFormatValid() || !inputVideo[1]->isFormatValid())
    return false;

  return true;
}

void videoHandlerDifference::setInputVideos(frameHandler *childVideo0, frameHandler *childVideo1)
{
  if (inputVideo[0] != childVideo0 || inputVideo[1] != childVideo1)
  {
    // Something changed
    inputVideo[0] = childVideo0;
    inputVideo[1] = childVideo1;

    if (inputsValid())
    {
      // We have two valid video "children"

      // Get the frame size of the difference (min in x and y direction), and set it.
      QSize size0 = inputVideo[0]->getFrameSize();
      QSize size1 = inputVideo[1]->getFrameSize();
      QSize diffSize = QSize(std::min(size0.width(), size1.width()), std::min(size0.height(), size1.height()));
      setFrameSize(diffSize);
    }

    // If something changed, we might need a redraw
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
}

QStringPairList videoHandlerDifference::getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2, const int frameIdx1)
{
  Q_UNUSED(item2);

  if (!inputsValid())
    return QStringPairList();

  return inputVideo[0]->getPixelValues(pixelPos, frameIdx, inputVideo[1], frameIdx1);
}

QLayout *videoHandlerDifference::createDifferenceHandlerControls()
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  ui.setupUi();

  // Set all the values of the properties widget to the values of this class
  ui.markDifferenceCheckBox->setChecked(markDifference);
  ui.amplificationFactorSpinBox->setValue(amplificationFactor);
  ui.codingOrderComboBox->addItems(QStringList() << "HEVC");
  ui.codingOrderComboBox->setCurrentIndex((int)codingOrder);
   
  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.markDifferenceCheckBox, &QCheckBox::stateChanged, this, &videoHandlerDifference::slotDifferenceControlChanged);
  connect(ui.codingOrderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerDifference::slotDifferenceControlChanged);
  connect(ui.amplificationFactorSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerDifference::slotDifferenceControlChanged);
    
  return ui.topVBoxLayout;
}

void videoHandlerDifference::slotDifferenceControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  if (sender == ui.markDifferenceCheckBox)
  {
    markDifference = ui.markDifferenceCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentImageIdx = -1;
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
  else if (sender == ui.codingOrderComboBox)
  {
    codingOrder = (CodingOrder)ui.codingOrderComboBox->currentIndex();

     // The calculation of the first difference in coding order changed but no redraw is necessary
    emit signalHandlerChanged(false, RECACHE_NONE);
  }
  else if (sender == ui.amplificationFactorSpinBox)
  {
    amplificationFactor = ui.amplificationFactorSpinBox->value();

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentImageIdx = -1;
    emit signalHandlerChanged(true, RECACHE_NONE);
  }
}

void videoHandlerDifference::reportFirstDifferencePosition(QList<infoItem> &infoList) const
{
  if (!inputsValid())
    return;

  if (currentImage.width() != frameSize.width() || currentImage.height() != frameSize.height())
    return;

  if (codingOrder == CodingOrder_HEVC)
  {
    // Assume the following:
    // - The picture is split into LCUs of 64x64 pixels which are scanned in raster scan
    // - Each LCU is scanned in a hierarchical tree until the smallest unit size (4x4 pixels) is reached
    // This is exactly what we are going to do here now

    int widthLCU  = (frameSize.width()  + 63) / 64;  // Round up
    int heightLCU = (frameSize.height() + 63) / 64;

    for (int y = 0; y < heightLCU; y++)
    {
      for (int x = 0; x < widthLCU; x++)
      {
        // Now take the tree approach
        int firstX, firstY, partIndex = 0;


        videoHandlerYUV* video0 = dynamic_cast<videoHandlerYUV*>(inputVideo[0].data());
        if(video0 != NULL && video0->getIs_YUV_diff())
        {

            // find first difference using YUV instead of QImage. The latter does not work for 10bit videos and very small differences, since it only supports 8bit
            if (hierarchicalPositionYUV(x*64, y*64, 64, firstX, firstY, partIndex, video0->getDiffYUV(), video0->getDiffYUVFormat()))
            {
              // We found a difference in this block
              infoList.append(infoItem("First Difference LCU", QString::number(y * widthLCU + x)));
              infoList.append(infoItem("First Difference X", QString::number(firstX)));
              infoList.append(infoItem("First Difference Y", QString::number(firstY)));
              infoList.append(infoItem("First Difference partIndex", QString::number(partIndex)));
              return;
            }

        }
        else
        {
            if (hierarchicalPosition(x*64, y*64, 64, firstX, firstY, partIndex, currentImage))
            {
              // We found a difference in this block
              infoList.append(infoItem("First Difference LCU", QString::number(y * widthLCU + x)));
              infoList.append(infoItem("First Difference X", QString::number(firstX)));
              infoList.append(infoItem("First Difference Y", QString::number(firstY)));
              infoList.append(infoItem("First Difference partIndex", QString::number(partIndex)));
              return;
            }
        }
      }
    }
  }

  // No difference was found
  infoList.append(infoItem("Difference", "Frames are identical"));
}

bool videoHandlerDifference::hierarchicalPosition(int x, int y, int blockSize, int &firstX, int &firstY, int &partIndex, const QImage &diffImg) const
{
  if (x >= frameSize.width() || y >= frameSize.height())
    // This block is entirely outside of the picture
    return false;

  if (blockSize == 4)
  {
    // Check for a difference
    for (int subX=x; subX < x+4; subX++)
    {
      for (int subY=y; subY < y+4; subY++)
      {
        QRgb rgb = diffImg.pixel(QPoint(subX, subY));

        if (markDifference)
        {
          // Black means no difference
          if (qRed(rgb) != 0 || qGreen(rgb) != 0 || qBlue(rgb) != 0)
          {
            // First difference found
            firstX = x;
            firstY = y;
            return true;
          }
        }
        else
        {
          // TODO: Double check if this is always true
          // Do other values also convert to RGB(130,130,130) ?
          // What about ten bit input material
          int red = qRed(rgb);
          int green = qGreen(rgb);
          int blue = qBlue(rgb);

          if (red != 130 || green != 130 || blue != 130)
          {
            // First difference found
            firstX = x;
            firstY = y;
            return true;
          }
        }
      }
    }

    // No difference found in this block. Count the number of 4x4 blocks scanned (that is the partIndex)
    partIndex++;
  }
  else
  {
    // Walk further into the hierarchy
    const int b2 = blockSize/2;
    if (hierarchicalPosition(x     , y     , b2, firstX, firstY, partIndex, diffImg))
      return true;
    if (hierarchicalPosition(x + b2, y     , b2, firstX, firstY, partIndex, diffImg))
      return true;
    if (hierarchicalPosition(x     , y + b2, b2, firstX, firstY, partIndex, diffImg))
      return true;
    if (hierarchicalPosition(x + b2, y + b2, b2, firstX, firstY, partIndex, diffImg))
      return true;
  }
  return false;
}

inline int getValueFromSource(const unsigned char * src, const int idx, const int bps, const bool bigEndian)
{
  if (bps > 8)
    // Read two bytes in the right order
    return (bigEndian) ? src[idx*2] << 8 | src[idx*2+1] : src[idx*2] | src[idx*2+1] << 8;
  else
    // Just read one byte
    return src[idx];
}


bool videoHandlerDifference::hierarchicalPositionYUV(int x, int y, int blockSize, int &firstX, int &firstY, int &partIndex, const QByteArray &diffYUV, const YUV_Internals::yuvPixelFormat &diffYUVFormat) const
{
  if (x >= frameSize.width() || y >= frameSize.height())
    // This block is entirely outside of the picture
    return false;

  // The items can be of different size (we then calculate the difference of the top left aligned part)
  const int w_in = frameSize.width();
  const int h_in = frameSize.height();

  // Get subsampling modes (they are identical for both inputs and the output)
  const int subH = diffYUVFormat.getSubsamplingHor();
  const int subV = diffYUVFormat.getSubsamplingVer();

  // Get the endianess of the inputs
  const bool bigEndian = diffYUVFormat.bigEndian;

  // Get/Set the bit depth of the input
  const int bps_in = diffYUVFormat.bitsPerSample;
  const int diffZero = 128 << (bps_in-8);

  // Get pointers to the inputs
  const int componentSizeLuma_In = w_in*h_in;
  const int componentSizeChroma_In = (w_in/subH)*(h_in/subV);
  const int nrBytesLumaPlane_In = bps_in > 8 ? 2 * componentSizeLuma_In : componentSizeLuma_In;
  const int nrBytesChromaPlane_In = bps_in > 8 ? 2 * componentSizeChroma_In : componentSizeChroma_In;
  // Current item

  // Calculate Luma sample difference
  const int stride_in = bps_in > 8 ? w_in*2 : w_in;  // How many bytes to the next y line?
  const int strideC_in = w_in / subH * (bps_in > 8 ? 2 : 1) ;  // How many bytes to the next U/V y line

  if (blockSize == 4)
  {
      const unsigned char * srcY1 = (unsigned char*)diffYUV.data();
      const unsigned char * srcU1 = (diffYUVFormat.planeOrder == YUV_Internals::Order_YUV || diffYUVFormat.planeOrder == YUV_Internals::Order_YUVA) ? srcY1 + nrBytesLumaPlane_In : srcY1 + nrBytesLumaPlane_In + nrBytesChromaPlane_In;
      const unsigned char * srcV1 = (diffYUVFormat.planeOrder == YUV_Internals::Order_YUV || diffYUVFormat.planeOrder == YUV_Internals::Order_YUVA) ? srcY1 + nrBytesLumaPlane_In + nrBytesChromaPlane_In: srcY1 + nrBytesLumaPlane_In;

    // adjust source pointer according to block position
    srcY1 += y*stride_in;
    srcU1 += y/subV*strideC_in;
    srcV1 += y/subV*strideC_in;

    // Check for a difference
    for (int subY=y; subY < y+4; subY++)
    {

      for (int subX=x; subX < x+4; subX++)
      {

          int val1 = getValueFromSource(srcY1, subX, bps_in, bigEndian);
          if(val1 != diffZero)
          {
              firstX = x;
              firstY = y;
              return true;
          }

          // is this a position at which we have a chroma sample?
          if(subX % subH == 0 && subY % subV == 0 && subX*subV < w_in)
          {
              int valU1 = getValueFromSource(srcU1, subX, bps_in, bigEndian);
              int valV1 = getValueFromSource(srcV1, subX, bps_in, bigEndian);
              if(valU1 != diffZero || valV1 != diffZero)
              {
                  firstX = x*subV;
                  firstY = y;
                  return true;
              }
          }


      }

      // Goto the next y line
      srcY1 += stride_in;

      // is this a position at which we have a chroma line?
      if(subY % subV == 0)
      {
          // Goto the next y line
          srcU1 += strideC_in;
          srcV1 += strideC_in;
      }
    }

    // No difference found in this block. Count the number of 4x4 blocks scanned (that is the partIndex)
    partIndex++;
  }
  else
  {
    // Walk further into the hierarchy
    const int b2 = blockSize/2;
    if (hierarchicalPositionYUV(x     , y     , b2, firstX, firstY, partIndex, diffYUV, diffYUVFormat))
      return true;
    if (hierarchicalPositionYUV(x + b2, y     , b2, firstX, firstY, partIndex, diffYUV, diffYUVFormat))
      return true;
    if (hierarchicalPositionYUV(x     , y + b2, b2, firstX, firstY, partIndex, diffYUV, diffYUVFormat))
      return true;
    if (hierarchicalPositionYUV(x + b2, y + b2, b2, firstX, firstY, partIndex, diffYUV, diffYUVFormat))
      return true;
  }
  return false;
}
