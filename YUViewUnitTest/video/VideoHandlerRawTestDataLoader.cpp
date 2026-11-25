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

#include "VideoHandlerRawTestDataLoader.h"

#include <video/videoHandler.h>

namespace video::test
{

videoHandlerDataLoadingTest::videoHandlerDataLoadingTest(video::videoHandler *video) : video(video)
{
  if (video == nullptr)
    throw std::invalid_argument("videoHandlerDataLoadingTest needs a non-null video handler");

  QObject::connect(video,
                   &videoHandler::signalRequestRawData,
                   this,
                   &videoHandlerDataLoadingTest::loadRawTestData,
                   Qt::DirectConnection);
}

void videoHandlerDataLoadingTest::addExpectedLoadingRequests(LoadingRequest expectedLoadingRequest)
{
  if (expectedLoadingRequest.frameIdx < 0)
    throw std::invalid_argument("frameIdx must be non-negative");
  if (expectedLoadingRequest.rawData.isEmpty())
    throw std::invalid_argument("rawData must not be empty");

  this->expectedLoadingRequests.push(expectedLoadingRequest);
}

void videoHandlerDataLoadingTest::loadRawTestData(int frameIdx, bool forceDecodingNow)
{
  (void)forceDecodingNow;

  if (expectedLoadingRequests.empty())
    throw std::runtime_error("Received unexpected loading request for frame index " +
                             std::to_string(frameIdx));

  const auto expectedRequest = expectedLoadingRequests.front();
  expectedLoadingRequests.pop();

  if (frameIdx != expectedRequest.frameIdx)
    throw std::runtime_error("Received loading request for unexpected frame index. Expected: " +
                             std::to_string(expectedRequest.frameIdx) +
                             ", actual: " + std::to_string(frameIdx));

  video->rawData            = expectedRequest.rawData;
  video->rawData_frameIndex = frameIdx;
}

} // namespace video::test
