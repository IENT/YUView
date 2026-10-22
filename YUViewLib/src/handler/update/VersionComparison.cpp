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

#include "VersionComparison.h"

#include <common/Functions.h>

#include <QRegularExpression>

#define VERSIONCOMPARE_DEBUG_OUTPUT 0
#if UPDATER_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_VERSION(msg) qDebug() << msg
#else
#define DEBUG_VERSION(msg) ((void)0)
#endif

namespace update
{

namespace
{

struct Version
{
  int  major{};
  int  minor{};
  int  min{};
  bool hasAdditionalTag{};

  bool operator>(const Version &other) const
  {
    if (this->major > other.major)
      return true;
    if (this->major == other.major)
    {
      if (this->minor > other.minor)
        return true;
      if (this->minor == other.minor)
      {
        if (this->min > other.min)
          return true;
        if (this->min == other.min)
          return this->hasAdditionalTag && !other.hasAdditionalTag;
      }
    }

    return false;
  }
};

std::optional<Version> parseVersionFromString(QString str)
{
  QRegularExpression versionRegexp(
    "^[vV]?(?<major>\\d+)(.(?<minor>\\d+))?(.(?<min>\\d+))?(?<tag>-[a-zA-Z0-9]+)?$");
  auto versionMatch = versionRegexp.match(str);
  if (!versionMatch.hasMatch())
    return {};

  Version version;
  if (const auto major = functions::toInt(versionMatch.captured("major")))
    version.major = *major;
  else
    return {};

  version.minor            = functions::toInt(versionMatch.captured("minor")).value_or(0);
  version.min              = functions::toInt(versionMatch.captured("min")).value_or(0);
  version.hasAdditionalTag = !versionMatch.captured("tag").isEmpty();

  return version;
}

} // namespace

bool isServerVersionNewer(const QString &serverVersionString, const QString &currentVersionString)
{
  const auto currentVersion = parseVersionFromString(currentVersionString);
  if (!currentVersion)
  {
    DEBUG_VERSION("isServerVersionNewer Error parsing current version from string "
                  << YUVIEW_VERSION);
    return false;
  }

  const auto serverVersion = parseVersionFromString(serverVersionString);
  if (!serverVersion)
  {
    DEBUG_VERSION("isServerVersionNewer Error parsing server version from string "
                  << serverVersionString);
    return false;
  }

  DEBUG_VERSION("isServerVersionNewer serverVersion v"
                << serverVersion.major << "." << serverVersion.minor << "." << serverVersion.min
                << (serverVersion.hasAdditionalTag ? "-tag" : "") << " currentVersion "
                << currentVersion.major << "." << currentVersion.minor << "." << currentVersion.min
                << (currentVersion.hasAdditionalTag ? "-tag" : ""));

  // This is the current logic
  return *serverVersion > *currentVersion;
}

} // namespace update
