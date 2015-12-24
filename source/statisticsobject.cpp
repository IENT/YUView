/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "statisticsobject.h"
#include "yuvfile.h"
#include "common.h"

StatisticsObject::StatisticsObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
  // Open statistics file
  if (srcFileName.endsWith(".csv")) {
    // Open file source statistic
    p_statisticSource = QSharedPointer<statisticSource>(new statisticSourceFile(srcFileName));

    // Connect the signal_statisticInformationChanged from the statisticSourceFile
    QSharedPointer<statisticSourceFile> f = qSharedPointerDynamicCast<statisticSourceFile>(p_statisticSource);
    QObject::connect(f.data(), SIGNAL(signal_statisticInformationChanged()), this, SLOT(statisticSourceInformationChanced()));
  }
  else {
    return;
  }

  // Try to get the width/height from the file name or the source
  int width, height, frameRate, bitDepth, subFormat;
  formatFromFilename(srcFileName, width, height, frameRate, bitDepth, subFormat);

  if (width == -1 || height == -1) {
    QSize size = p_statisticSource->getSize();
    width = size.width();
    height = size.height();
  }
  if (frameRate == -1) {
    frameRate = p_statisticSource->getFrameRate();
  }

  // Set display object things
  p_name = srcFileName;
  p_width = width;
  p_height = height;
}

StatisticsObject::StatisticsObject(QSharedPointer<statisticSource> statSrc, QObject* parent) : DisplayObject(parent)
{
  // The statistic source has already been created.
  // Just get the necessary data from it.
  p_statisticSource = statSrc;

  // Get width/height from the source
  p_name = statSrc->getName();
  QSize size = statSrc->getSize();
  p_width = size.width();
  p_height = size.height();

  // Connect signal_statisticInformationChanged if statSrc is a statisticSourceFile
  QSharedPointer<statisticSourceFile> statFile = qSharedPointerDynamicCast<statisticSourceFile>(p_statisticSource);
  if (!statFile.isNull())
    QObject::connect(statFile.data(), SIGNAL(signal_statisticInformationChanged()), this, SLOT(statisticSourceInformationChanced()));
}

StatisticsObject::~StatisticsObject() 
{
  if (p_statisticSource != NULL) {
    // Disconnect signal_statisticInformationChanged if statSrc is a statisticSourceFile
    QSharedPointer<statisticSourceFile> statFile = qSharedPointerDynamicCast<statisticSourceFile>(p_statisticSource);
    if (!statFile.isNull())
      QObject::disconnect(statFile.data(), SIGNAL(signal_statisticInformationChanged()), NULL, NULL);
  } 
}

void StatisticsObject::statisticSourceInformationChanced()
{
  if (p_statisticSource != NULL) {
    // Update status and errors
    p_status = p_statisticSource->getStatus();
    p_info = p_statisticSource->getInfo();

    emit signal_objectInformationChanged();
  }
}

void StatisticsObject::loadImage(int frameIdx)
{
  if (frameIdx == INT_INVALID || frameIdx >= numFrames() || p_statisticSource == NULL)
  {
    p_displayImage = QPixmap();
    return;
  }

  // create empty image
  QImage tmpImage(internalScaleFactor()*width(), internalScaleFactor()*height(), QImage::Format_ARGB32);
  tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
  p_displayImage.convertFromImage(tmpImage);

  // draw statistics
  p_statisticSource->drawStatistics(&p_displayImage, frameIdx);
  p_lastIdx = frameIdx;
}

// Get a complete list of all the info we want to show for this file.
QList<infoItem> StatisticsObject::getInfoList()
{
  QList<infoItem> infoList;

  if (p_statisticSource) {
    infoList.append(infoItem("Path", p_statisticSource->getPath()));
    infoList.append(infoItem("Time Created", p_statisticSource->getCreatedtime()));
    infoList.append(infoItem("Time Modified", p_statisticSource->getModifiedtime()));
    infoList.append(infoItem("Nr Bytes", QString::number(p_statisticSource->getNumberBytes())));
    infoList.append(infoItem("Num Frames", QString::number(numFrames())));
    infoList.append(infoItem("Status", getStatusAndInfo()));
  }

  return infoList;
}