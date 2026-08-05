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

#include "Functions.h"

#ifdef Q_OS_MAC
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(Q_OS_UNIX)
#include <unistd.h>
#elif defined(Q_OS_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <charconv>
#include <string_view>

#include <QDir>
#include <QFileInfo>
#include <QThread>

namespace functions
{

unsigned int getOptimalThreadCount()
{
  int nrThreads = QThread::idealThreadCount() - 1;
  if (nrThreads > 0)
    return (unsigned int)nrThreads;
  else
    return 1;
}

unsigned int systemMemorySizeInMB()
{
  static unsigned int memorySizeInMB;
  if (!memorySizeInMB)
  {
    // Fetch size of main memory - assume 2 GB first.
    // Unfortunately there is no Qt API for doing this so this is platform dependent.
    memorySizeInMB = 2 << 10;
#ifdef Q_OS_MAC
    int      mib[2]  = {CTL_HW, HW_MEMSIZE};
    u_int    namelen = sizeof(mib) / sizeof(mib[0]);
    uint64_t size;
    size_t   len = sizeof(size);

    if (sysctl(mib, namelen, &size, &len, NULL, 0) == 0)
      memorySizeInMB = size >> 20;
#elif defined Q_OS_UNIX
    long pages     = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    memorySizeInMB = (pages * page_size) >> 20;
#elif defined Q_OS_WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    memorySizeInMB = status.ullTotalPhys >> 20;
#endif
  }
  return memorySizeInMB;
}

QStringList getThemeNameList()
{
  QStringList ret{};
  ret.append("Default");
  ret.append("Simple Dark/Blue");
  ret.append("Simple Dark/Orange");
  return ret;
}

QString getThemeFileName(QString themeName)
{
  if (themeName == "Simple Dark/Blue" || themeName == "Simple Dark/Orange")
    return ":YUViewSimple.qss";
  return "";
}

QStringList getThemeColors(QString themeName)
{
  if (themeName == "Simple Dark/Blue")
    return QStringList() << "#262626"
                         << "#E0E0E0"
                         << "#808080"
                         << "#3daee9";
  if (themeName == "Simple Dark/Orange")
    return QStringList() << "#262626"
                         << "#E0E0E0"
                         << "#808080"
                         << "#FFC300 ";
  return QStringList();
}

// If you are loading a playlist and you have an absolute path and a relative path, this function
// will return the absolute path (if a file with that absolute path exists) or convert the relative
// path to an absolute one and return that (if that file exists). If neither exists the empty string
// is returned.
QString getAbsPathFromAbsAndRel(const QString &currentPath,
                                const QString &absolutePath,
                                const QString &relativePath)
{
  QFileInfo checkAbsoluteFile(absolutePath);
  if (checkAbsoluteFile.exists())
    return absolutePath;

  QFileInfo plFileInfo(currentPath);
  auto      combinePath = QDir(plFileInfo.path()).filePath(relativePath);
  QFileInfo checkRelativeFile(combinePath);
  if (checkRelativeFile.exists() && checkRelativeFile.isFile())
  {
    return QDir::cleanPath(combinePath);
  }

  return {};
}

QString formatDataSize(double size, bool isBits)
{
  unsigned divCounter = 0;
  bool     isNegative = size < 0;
  if (isNegative)
    size = -size;
  while (divCounter < 5 && size >= 1000)
  {
    size = size / 1000;
    divCounter++;
  }
  QString valueString;
  if (isNegative)
    valueString += "-";
  valueString += QString("%1").arg(size, 0, 'f', 2);

  if (divCounter > 0 && divCounter < 5)
  {
    const auto bitsUnits = QStringList() << "kbit"
                                         << "Mbit"
                                         << "Gbit"
                                         << "Tbit";
    const auto bytesUnits = QStringList() << "kB"
                                          << "MB"
                                          << "GB"
                                          << "TB";
    if (isBits)
      return QString("%1 %2").arg(valueString).arg(bitsUnits[divCounter - 1]);
    else
      return QString("%1 %2").arg(valueString).arg(bytesUnits[divCounter - 1]);
  }

  return valueString;
}

QStringList toQStringList(const std::vector<std::string> &stringVec)
{
  QStringList list;
  for (const auto &s : stringVec)
    list.append(QString::fromStdString(s));
  return list;
}

std::string toLower(const std::string_view str)
{
  std::string lowercaseStr(str);
  std::transform(lowercaseStr.begin(),
                 lowercaseStr.end(),
                 lowercaseStr.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return lowercaseStr;
}

ByteVector readData(std::istream &istream, const size_t nrBytes)
{
  ByteVector data;
  data.resize(nrBytes);
  istream.read(reinterpret_cast<char *>(data.data()), nrBytes);
  const auto nrBytesActuallyRead = istream.gcount();
  data.resize(nrBytesActuallyRead);
  return data;
}

std::optional<unsigned> toUnsigned(const std::string_view text)
{
  unsigned   value{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

  if (result.ec != std::errc())
    return {};
  const auto allCharactersParsed = (result.ptr == &(*text.end()));
  if (!allCharactersParsed)
    return {};

  return value;
}

std::optional<int> toInt(const std::string_view text)
{
  int        value{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

  if (result.ec != std::errc())
    return {};
  const auto allCharactersParsed = (result.ptr == &(*text.end()));
  if (!allCharactersParsed)
    return {};

  return value;
}

} // namespace functions
