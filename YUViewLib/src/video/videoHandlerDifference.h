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

#include "ui_videoHandlerDifference.h"

namespace video
{

class videoHandlerDifference : public videoHandler
{
  Q_OBJECT

public:
  explicit videoHandlerDifference();

  // Draw the frame with the given frame index and zoom factor. If onLoadShowLasFrame is set, show
  // the last frame if the frame with the current frame index is loaded in the background.
  void drawDifferenceFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawValues);

  void loadFrameDifference(int frameIndex, bool loadToDoubleBuffer = false);

  // Are both inputs valid and can be used?
  bool inputsValid() const;

  // Create the YUV controls and return a pointer to the layout.
  virtual QLayout *createDifferenceHandlerControls();

  // Set the two video inputs. This will also update the number frames, the controls and the frame
  // size. The signal signalHandlerChanged will be emitted if a redraw is required.
  void setInputVideos(FrameHandler *childVideo0, FrameHandler *childVideo1);

  QList<InfoItem> differenceInfoList;

  // The difference overloads this and returns the difference values (A-B)
  virtual QStringPairList getPixelValues(const QPoint &pixelPos,
                                         int           frameIdx,
                                         FrameHandler *item2     = nullptr,
                                         const int     frameIdx1 = 0) override;

  void
  guessAndSetPixelFormat(const filesource::frameFormatGuess::GuessedFrameFormat &frameFormat,
                         const filesource::frameFormatGuess::FileInfoForGuess   &fileInfo) override;

  // Calculate the position of the first difference and add the info to the list
  void reportFirstDifferencePosition(QList<InfoItem> &infoList) const;

  virtual void savePlaylist(YUViewDomElement &root) const override;
  virtual void loadPlaylist(const YUViewDomElement &root) override;

private slots:
  void slotDifferenceControlChanged();

protected:
  ItemLoadingState needsLoadingRawValues(int frameIndex) override;

  bool markDifference{}; // Mark differences?
  int  amplificationFactor{1};

private:
  enum class CodingOrder
  {
    HEVC
  };
  CodingOrder codingOrder{CodingOrder::HEVC};

  // The two videos that the difference will be calculated from
  QPointer<FrameHandler> inputVideo[2];

  // Recursively scan the LCU
  bool hierarchicalPosition(int           x,
                            int           y,
                            int           blockSize,
                            int          &firstX,
                            int          &firstY,
                            int          &partIndex,
                            const QImage &diffImg) const;
  bool hierarchicalPositionYUV(int                        x,
                               int                        y,
                               int                        blockSize,
                               int                       &firstX,
                               int                       &firstY,
                               int                       &partIndex,
                               const QByteArray          &diffYUV,
                               const yuv::PixelFormatYUV &diffYUVFormat) const;

  SafeUi<Ui::videoHandlerDifference> ui;
};

} // namespace video
