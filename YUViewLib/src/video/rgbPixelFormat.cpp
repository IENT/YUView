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

#include "rgbPixelFormat.h"

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define RGBPIXELFORMAT_DEBUG 0
#if RGBPIXELFORMAT_DEBUG && !NDEBUG
#include <QDebug>
#define DEBUG_RGB_FORMAT qDebug
#else
#define DEBUG_RGB_FORMAT(fmt,...) ((void)0)
#endif

namespace RGB_Internals
{

rgbPixelFormat::rgbPixelFormat(int bitsPerValue, bool planar, int posR, int posG, int posB, int posA)
  : posR(posR), posG(posG), posB(posB), posA(posA), bitsPerValue(bitsPerValue), planar(planar)
{
}

rgbPixelFormat::rgbPixelFormat(const QString &name)
{
  if (name == "Unknown Pixel Format")
  {
    posR = 0;
    posG = 0;
    posB = 0;
    posA = -1;
    bitsPerValue = 0;
    planar = false;
  }
  else
  {
    setRGBFormatFromString(name.left(4));
    int bitIdx = name.indexOf("bit");
    bitsPerValue = name.mid(bitIdx-2, 2).toInt();
    planar = name.contains("planar");
  }
}

bool rgbPixelFormat::isValid() const
{ 
  if (bitsPerValue < 8 || bitsPerValue > 16)
    return false;

  if (posR == posG || posR == posB || posG == posB)
    return false;

  const auto hasAlpha = (posA != -1);

  if (hasAlpha && (posA == posR || posA == posG || posA == posB))
    return false;

  const auto valueLimit = hasAlpha ? 4 : 3;

  if (posR < 0 || posR >= valueLimit || 
      posG < 0 || posG >= valueLimit ||
      posB < 0 || posB >= valueLimit)
    return false;
  
  if (hasAlpha && (posA < 0 || posA >= valueLimit))
    return false;

  return true;
}

QString rgbPixelFormat::getName() const
{
  if (!this->isValid())
    return "Unknown Pixel Format";

  QString name = getRGBFormatString();
  name.append(QString(" %1bit").arg(bitsPerValue));
  if (planar)
    name.append(" planar");

  return name;
}

QString rgbPixelFormat::getRGBFormatString() const
{
  QString name;
  for (int i = 0; i < nrChannels(); i++)
  {
    if (posR == i)
      name.append("R");
    if (posG == i)
      name.append("G");
    if (posB == i)
      name.append("B");
    if (posA == i)
      name.append("A");
  }
  return name;
}

void rgbPixelFormat::setRGBFormatFromString(const QString &format)
{
  int n = format.length();
  if (n < 3)
    return;
  if (n > 4)
    n = 4;

  for (int i = 0; i < n; i++)
  {
    if (format[i].toLower() == 'r')
      posR = i;
    else if (format[i].toLower() == 'g')
      posG = i;
    else if (format[i].toLower() == 'b')
      posB = i;
    else if (format[i].toLower() == 'a')
      posA = i;
  }
}

/* Get the number of bytes for a frame with this RGB format and the given size
*/
std::optional<size_t> rgbPixelFormat::bytesPerFrame(const QSize &frameSize) const
{
  if (this->bitsPerValue == 0 || !frameSize.isValid())
    return {};
  
  auto channels = this->nrChannels();
  if (!channels)
    return {};

  auto numSamples = size_t(frameSize.height()) * size_t(frameSize.width());
  auto nrBytes = numSamples * (*channels) * ((this->bitsPerValue + 7) / 8);
  DEBUG_RGB_FORMAT("rgbPixelFormat::bytesPerFrame samples %d channels %d bytes %d", int(numSamples), *channels, nrBytes);
  return nrBytes;
}

} // namespace RGB_Internals