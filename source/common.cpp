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

#include "common.h"
#include <QRegExp>

void formatFromFilename(QString name, int &width, int &height, int &frameRate, int &bitDepth, int &subFormat)
{
  // preset return values first
  width = -1;
  height = -1;
  frameRate = -1;
  bitDepth = -1;
  subFormat = -1;

  if (name.isEmpty())
    return;

  // parse filename and extract width, height and framerate
  // default format is: sequenceName_widthxheight_framerate.yuv
  QRegExp rxExtendedFormat("([0-9]+)x([0-9]+)_([0-9]+)_([0-9]+)_([0-9]+)");
  if (rxExtendedFormat.indexIn(name) > -1)
  {
    QString widthString = rxExtendedFormat.cap(1);
    width = widthString.toInt();

    QString heightString = rxExtendedFormat.cap(2);
    height = heightString.toInt();

    QString rateString = rxExtendedFormat.cap(3);
    frameRate = rateString.toDouble();

    QString bitDepthString = rxExtendedFormat.cap(4);
    bitDepth = bitDepthString.toInt();

    QString subSampling = rxExtendedFormat.cap(5);
    subFormat = subSampling.toInt();
    return;
  }
  
  QRegExp rxExtended("([0-9]+)x([0-9]+)_([0-9]+)_([0-9]+)");
  if (rxExtended.indexIn(name) > -1)
  {
    QString widthString = rxExtended.cap(1);
    width = widthString.toInt();

    QString heightString = rxExtended.cap(2);
    height = heightString.toInt();

    QString rateString = rxExtended.cap(3);
    frameRate = rateString.toDouble();

    QString bitDepthString = rxExtended.cap(4);
    bitDepth = bitDepthString.toInt();
    return;
  }

  QRegExp rxDefault("([0-9]+)x([0-9]+)_([0-9]+)");
  if (rxDefault.indexIn(name) > -1) {
    QString widthString = rxDefault.cap(1);
    width = widthString.toInt();

    QString heightString = rxDefault.cap(2);
    height = heightString.toInt();

    QString rateString = rxDefault.cap(3);
    frameRate = rateString.toDouble();

    bitDepth = 8; // assume 8 bit
    return;
  }

  QRegExp rxSizeOnly("([0-9]+)x([0-9]+)");
  if (rxSizeOnly.indexIn(name) > -1) {
    QString widthString = rxSizeOnly.cap(1);
    width = widthString.toInt();

    QString heightString = rxSizeOnly.cap(2);
    height = heightString.toInt();

    bitDepth = 8; // assume 8 bit
    return;
  }
  
  // try to find resolution indicators (e.g. 'cif', 'hd') in file name
  if (name.contains("_cif", Qt::CaseInsensitive))
  {
    width = 352;
    height = 288;
    return;
  }
  else if (name.contains("_qcif", Qt::CaseInsensitive))
  {
    width = 176;
    height = 144;
    return;
  }
  else if (name.contains("_4cif", Qt::CaseInsensitive))
  {
    width = 704;
    height = 576;
    return;
  }

  // try other resolution indicators with framerate: "1080p50", "720p24" ...
  QRegExp rx1080p("1080p([0-9]+)");
  if (rx1080p.indexIn(name) > -1) {
    width = 1920;
    height = 1080;
    
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  QRegExp rx720p("720p([0-9]+)");
  if (rx720p.indexIn(name) > -1) {
    width = 1280;
    height = 720;
    
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  // try without framerate indication: "1080p", "720p"
  if (name.contains("1080p", Qt::CaseSensitive)) {
    width = 1920;
    height = 1080;
    return;
  }
  if (name.contains("720p", Qt::CaseSensitive)) {
    width = 1280;
    height = 720;
    return;
  }

  // Maybe there is at least something about the frame rate in the filename
  // like: 24fps, 50Hz
  QRegExp rxFPS("([0-9]+)fps");
  if (rxFPS.indexIn(name) > -1) {
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }

  QRegExp rxHZ("([0-9]+)HZ");
  if (rxFPS.indexIn(name) > -1) {
    QString frameRateString = rxSizeOnly.cap(1);
    frameRate = frameRateString.toInt();
    return;
  }
}