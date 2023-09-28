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

#include "FFMpegLibrariesTypes.h"

namespace FFmpeg
{

QString timestampToString(int64_t timestamp, AVRational timebase)
{
  auto d_seconds = (double)timestamp * timebase.num / timebase.den;
  auto hours     = (int)(d_seconds / 60 / 60);
  d_seconds -= hours * 60 * 60;
  auto minutes = (int)(d_seconds / 60);
  d_seconds -= minutes * 60;
  auto seconds = (int)d_seconds;
  d_seconds -= seconds;
  auto milliseconds = (int)(d_seconds * 1000);

  return QString("%1:%2:%3.%4")
      .arg(hours, 2, 10, QChar('0'))
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'))
      .arg(milliseconds, 3, 10, QChar('0'));
}

Version Version::fromFFmpegVersion(const unsigned ffmpegVersion)
{
  Version v;
  v.major = AV_VERSION_MAJOR(ffmpegVersion);
  v.minor = AV_VERSION_MINOR(ffmpegVersion);
  v.micro = AV_VERSION_MICRO(ffmpegVersion);
  return v;
}

std::string to_string(const Version &version)
{
  std::ostringstream stream;
  stream << "v" << version.major;
  if (version.minor)
  {
    stream << "." << version.minor.value();
    if (version.micro)
      stream << "." << version.micro.value();
  }
  return stream.str();
}

std::ostream &operator<<(std::ostream &stream, const Version &version)
{
  stream << to_string(version);
  return stream;
}

} // namespace FFmpeg
