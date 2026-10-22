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

#include <common/Testing.h>

#include <handler/update/VersionComparison.h>

namespace update::test
{

TEST(VersionComparison, TestVersionsThatShouldBeNewer)
{
  EXPECT_TRUE(isServerVersionNewer("v1", "v0"));
  EXPECT_TRUE(isServerVersionNewer("v5", "v1"));
  EXPECT_TRUE(isServerVersionNewer("v22", "v2"));
  EXPECT_TRUE(isServerVersionNewer("v23.1", "v22.3"));
  EXPECT_TRUE(isServerVersionNewer("v23.1", "v1.99"));
  EXPECT_TRUE(isServerVersionNewer("v23.1.2", "v1.5.8"));
  EXPECT_TRUE(isServerVersionNewer("v23.1.2", "v1.1.8"));
  EXPECT_TRUE(isServerVersionNewer("v23.1.2-gh0ffff", "v1.1.2-ghfffff"));

  EXPECT_TRUE(isServerVersionNewer("v2.1", "v2"));
  EXPECT_TRUE(isServerVersionNewer("v2.5", "v2"));
  EXPECT_TRUE(isServerVersionNewer("v2.5", "v2.0"));
  EXPECT_TRUE(isServerVersionNewer("v2.5", "v2.1"));
  EXPECT_TRUE(isServerVersionNewer("v2.55.1", "v2.1.54"));
  EXPECT_TRUE(isServerVersionNewer("v2.2.1", "v2.1.1"));
  EXPECT_TRUE(isServerVersionNewer("v2.2.1", "v2.1.54"));
  EXPECT_TRUE(isServerVersionNewer("v2.2.1-gh2345", "v2.1.54-gh1234"));
  EXPECT_TRUE(isServerVersionNewer("v2.2.1-gh2345", "v2.1.54"));
  EXPECT_TRUE(isServerVersionNewer("v2.2.1", "v2.1.54-gh23435"));

  EXPECT_TRUE(isServerVersionNewer("v2.1.1", "v2.1"));
  EXPECT_TRUE(isServerVersionNewer("v2.1.3", "v2.1"));
  EXPECT_TRUE(isServerVersionNewer("v2.1.3", "v2.1.1"));
  EXPECT_TRUE(isServerVersionNewer("v2.1.3", "v2.1.2"));
  EXPECT_TRUE(isServerVersionNewer("v2.1.3-tag", "v2.1.2"));
  EXPECT_TRUE(isServerVersionNewer("v2.1.3", "v2.1.2-tag"));

  EXPECT_TRUE(isServerVersionNewer("v2.1.3-tag", "v2.1.3"));

  // The "v" is not required or can also be capitalized
  EXPECT_TRUE(isServerVersionNewer("v2.1.1", "2.1"));
  EXPECT_TRUE(isServerVersionNewer("2.1.1", "v2.1"));
  EXPECT_TRUE(isServerVersionNewer("2.1.1", "2.1"));
  EXPECT_TRUE(isServerVersionNewer("V2.1.1", "2.1"));
  EXPECT_TRUE(isServerVersionNewer("2.1.1", "V2.1"));
}

TEST(VersionComparison, TestVersionsThatShouldNotParse)
{
  EXPECT_FALSE(isServerVersionNewer("version3", "2.1.1"));
  EXPECT_FALSE(isServerVersionNewer("a3", "2.1.1"));
  EXPECT_FALSE(isServerVersionNewer("vv3", "2.1.1"));
  EXPECT_FALSE(isServerVersionNewer("vv3.2.1-tag-something", "2.1.1"));
  EXPECT_FALSE(isServerVersionNewer("v", "2.1.1"));
  EXPECT_FALSE(isServerVersionNewer("", "2.1.1"));
}

TEST(VersionComparison, TestVersionsThatShouldBeSmallerOrEqual)
{
  EXPECT_FALSE(isServerVersionNewer("v1", "v1"));
  EXPECT_FALSE(isServerVersionNewer("v1", "v2"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v1.1"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v1.2"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v2.1"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v1.1.1"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v1.1.0"));
  EXPECT_FALSE(isServerVersionNewer("v1.1", "v1.1.0-tag"));
  EXPECT_FALSE(isServerVersionNewer("v2.1.3", "2.1.3"));
  EXPECT_FALSE(isServerVersionNewer("v2.1.3", "2.1.3-tag"));
  EXPECT_FALSE(isServerVersionNewer("v2.1.3", "2.1.4"));
  EXPECT_FALSE(isServerVersionNewer("v2.1.3-tag1", "2.1.4-tag2"));
  EXPECT_FALSE(isServerVersionNewer("", "v0.0.0"));
}

} // namespace update::test