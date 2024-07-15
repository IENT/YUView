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

#include "TemporaryFile.h"

#include <fstream>
#include <random>

namespace yuviewTest
{

namespace
{

char mapRandomNumberToAlphaChar(const int i)
{
  constexpr auto ASCII_OFFSET_TO_0 = 48;
  constexpr auto ASCII_OFFSET_TO_A = 65;
  constexpr auto ASCII_OFFSET_TO_a = 97;

  if (i < 10)
    return static_cast<char>(i + ASCII_OFFSET_TO_0);
  if (i < 36)
    return static_cast<char>(i - 10 + ASCII_OFFSET_TO_A);
  return static_cast<char>(i - 36 + ASCII_OFFSET_TO_a);
}

std::string generateRandomFileName()
{
  std::random_device randomDevice;
  std::mt19937       generator(randomDevice());

  std::uniform_int_distribution<int> distribution(0, 61);

  const auto value = distribution(generator);

  std::stringstream s;
  constexpr auto    NR_CHARACTERS = 40;
  for (int i = 0; i < NR_CHARACTERS; ++i)
    s << mapRandomNumberToAlphaChar(distribution(generator));
  return s.str();
}

} // namespace

TemporaryFile::TemporaryFile(const ByteVector &data)
{
  this->temporaryFilePath = std::filesystem::temp_directory_path() / generateRandomFileName();

  std::ofstream tempFileWriter(this->temporaryFilePath, std::iostream::out | std::iostream::binary);
  std::for_each(
      data.begin(), data.end(), [&tempFileWriter](const unsigned char c) { tempFileWriter << c; });
  tempFileWriter.close();
}

TemporaryFile::~TemporaryFile()
{
  std::filesystem::remove(this->temporaryFilePath);
}

std::filesystem::path TemporaryFile::getFilePath() const
{
  return this->temporaryFilePath;
}

} // namespace yuviewTest