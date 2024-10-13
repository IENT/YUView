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

#pragma once

#include <common/InfoItemAndData.h>

#include <video/videoHandler.h>
#include <video/yuv/videoHandlerYUV.h>

#include <QPointer>

namespace video
{

class videoHandlerResample : public videoHandler
{
  Q_OBJECT

public:
  enum class Interpolation
  {
    Bilinear,
    Fast
  };

  explicit videoHandlerResample();

  // We need to override these videoHandler functions in order to map the frameIndex
  void drawFrame(QPainter *painter, int frameIndex, double zoomFactor, bool drawRawValues) override;
  QImage           calculateDifference(FrameHandler    *item2,
                                       const int        frameIndex0,
                                       const int        frameIndex1,
                                       QList<InfoItem> &differenceInfoList,
                                       const int        amplificationFactor,
                                       const bool       markDifference) override;
  ItemLoadingState needsLoading(int frameIndex, bool loadRawValues) override;

  void loadResampledFrame(int frameIndex, bool loadToDoubleBuffer = false);
  bool inputValid() const;

  // Set the video input. This will also update the number frames, the controls and the frame size.
  // The signal signalHandlerChanged will be emitted if a redraw is required.
  void setInputVideo(FrameHandler *childVideo);

  void setScaledSize(Size scaledSize);
  void setInterpolation(Interpolation interpolation);
  void setCutAndSample(indexRange startEnd, int sampling);

  virtual void setFormatFromSizeAndName(const Size       frameSize,
                                        int              bitDepth,
                                        DataLayout       dataLayout,
                                        int64_t          fileSize,
                                        const QFileInfo &fileInfo) override;

  QList<InfoItem> resampleInfoList;

private:
  int mapFrameIndex(int frameIndex);

  // The input video we will resample
  QPointer<FrameHandler> inputVideo;

  Size          scaledSize{0, 0};
  Interpolation interpolation{Interpolation::Bilinear};
  indexRange    cutRange{0, 0};
  int           sampling{1};
};

} // namespace video
