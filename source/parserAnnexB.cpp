/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik, RWTH Aachen University, GERMANY
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
#include "mainwindow.h"
#include <QProgressDialog>

#define PARSERANNEXB_DEBUG_OUTPUT 1
#if PARSERANNEXB_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB qDebug
#else
#define DEBUG_ANNEXB(fmt,...) ((void)0)
#endif

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

  if (POCList.contains(poc))
    throw std::logic_error(std::string("Error adding Frame POC already present - " + std::to_string(poc))); 
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
  if (codingOrderFrameIdx < 0 || codingOrderFrameIdx >= frameList.size())
    return QUint64Pair(-1, -1);
  return frameList[codingOrderFrameIdx].fileStartEndPos;
}

bool parserAnnexB::parseAnnexBFile(QScopedPointer<fileSourceAnnexBFile> &file)
{
  DEBUG_ANNEXB("playlistItemCompressedVideo::parseAnnexBFile");

  // Show a modal QProgressDialog while this operation is running.
  // If the user presses cancel, we will cancel and return false (opening the file failed).
  // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
  QWidget *mainWindow = MainWindow::getMainWindow();
  // Create the dialog
  int64_t maxPos = file->getFileSize();;
  // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
  int curPercentValue = 0;
  QProgressDialog progress("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow);
  progress.setMinimumDuration(1000);  // Show after 1s
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setWindowModality(Qt::WindowModal);

  // Just push all NAL units from the annexBFile into the annexBParser
  QByteArray nalData;
  int nalID = 0;
  QUint64Pair nalStartEndPosFile;
  while (!file->atEnd())
  {
    // Update the progress dialog
    if (progress.wasCanceled())
      return false;
    int64_t pos = file->pos();
    int newPercentValue = pos * 100 / maxPos;
    if (newPercentValue != curPercentValue)
    {
      progress.setValue(newPercentValue);
      curPercentValue = newPercentValue;
    }

    try
    {
      nalData = file->getNextNALUnit(false, &nalStartEndPosFile);
      parseAndAddNALUnit(nalID, nalData, nullptr, nalStartEndPosFile);
    }
    catch (const std::exception &exc)
    {
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_ANNEXB("parseAndAddNALUnit Exception thrown parsing NAL %d - %s", nalID, exc.what());
    }
    catch (...)
    {
      DEBUG_ANNEXB("parseAndAddNALUnit Exception thrown parsing NAL %d", nalID);
    }

    nalID++;
  }

  // We are done.
  parseAndAddNALUnit(-1, QByteArray());
  DEBUG_ANNEXB("parseAndAddNALUnit Parsing done. Found %d POCs.", POCList.length());
  return !progress.wasCanceled();
}