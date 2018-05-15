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

#include "parserAnnexB.h"
#include <assert.h>

parserAnnexB::parserAnnexB()
{
  curFrameFileStartEndPos = QUint64Pair(-1, -1);
  curFramePOC = -1;
  curFrameIsRandomAccess = false;
}

void parserAnnexB::addFrameToList(int poc, QUint64Pair fileStartEndPos, bool randomAccessPoint)
{
  annexBFrame newFrame;
  newFrame.poc = poc;
  newFrame.fileStartEndPos = fileStartEndPos;
  newFrame.randomAccessPoint = randomAccessPoint;
  frameList.append(newFrame);

  assert(!POCList.contains(poc));
  POCList.append(poc);
}

int parserAnnexB::getClosestSeekableFrameNumberBefore(int frameIdx, int &codingOrderFrameIdx) const
{
  // Get the POC for the frame number
  int seekPOC = POCList[frameIdx];

  int bestSeekPOC = -1;
  for (int i=0; i<frameList.length(); i++)
  {
    annexBFrame f = frameList[i];
    if (f.randomAccessPoint)
    {
      if (bestSeekPOC == -1)
      {
        bestSeekPOC = f.poc;
        codingOrderFrameIdx = i;
      }
      else
      {
        if (f.poc <= seekPOC)
        {
          // We could seek here
          bestSeekPOC = f.poc;
          codingOrderFrameIdx = i;
        }
        else
          break;
      }
    }
  }

  // Get the frame index for the given POC
  return POCList.indexOf(bestSeekPOC);
}

QUint64Pair parserAnnexB::getFrameStartEndPos(int codingOrderFrameIdx)
{
  if (codingOrderFrameIdx >= frameList.length())
    return QUint64Pair(-1, -1);
  return frameList[codingOrderFrameIdx].fileStartEndPos;
}

void parserAnnexB::parseAndAddNALUnitNoThrow(int nalID, QByteArray data, TreeItem * parent, QUint64Pair nalStartEndPosFile)
{
  try
  {
    parseAndAddNALUnit(nalID, data, parent, nalStartEndPosFile);
  }
  catch (...)
  {
    // Catch exceptions and just return
  }
}
