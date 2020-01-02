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
#include <QProgressDialog>
#include <QElapsedTimer>

#define PARSERANNEXB_DEBUG_OUTPUT 0
#if PARSERANNEXB_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB qDebug
#else
#define DEBUG_ANNEXB(fmt,...) ((void)0)
#endif

QString parserAnnexB::getShortStreamDescription(int streamIndex) const
{
  Q_UNUSED(streamIndex);

  QString info = "Video";
  QSize frameSize = this->getSequenceSizeSamples();
  if (frameSize.isValid())
    info += QString(" (%1x%2)").arg(frameSize.width()).arg(frameSize.height());
  return info;
}

bool parserAnnexB::addFrameToList(int poc, QUint64Pair fileStartEndPos, bool randomAccessPoint)
{
  if (POCList.contains(poc))
    return false;

  if (pocOfFirstRandomAccessFrame == -1 && randomAccessPoint)
    pocOfFirstRandomAccessFrame = poc;
  if (poc >= pocOfFirstRandomAccessFrame)
  {
    // We don't add frames which we can not decode because they are before the first RA (I) frame
    annexBFrame newFrame;
    newFrame.poc = poc;
    newFrame.fileStartEndPos = fileStartEndPos;
    newFrame.randomAccessPoint = randomAccessPoint;
    frameList.append(newFrame);

    POCList.append(poc);
  }
  return true;
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

bool parserAnnexB::parseAnnexBFile(QScopedPointer<fileSourceAnnexBFile> &file, QWidget *mainWindow)
{
  DEBUG_ANNEXB("parserAnnexB::parseAnnexBFile");

  int64_t maxPos = file->getFileSize();
  QScopedPointer<QProgressDialog> progressDialog;
  int curPercentValue = 0;
  if (mainWindow)
  {
    // Show a modal QProgressDialog while this operation is running.
    // If the user presses cancel, we will cancel and return false (opening the file failed).
    // First, get a pointer to the main window to use as a parent for the modal parsing progress dialog.
    progressDialog.reset(new QProgressDialog("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow));
    progressDialog->setMinimumDuration(1000);  // Show after 1s
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->setWindowModality(Qt::WindowModal);
  }

  stream_info.file_size = file->getFileSize();
  stream_info.parsing = true;
  emit streamInfoUpdated();

  // Just push all NAL units from the annexBFile into the annexBParser
  QByteArray nalData;
  int nalID = 0;
  QUint64Pair nalStartEndPosFile;
  bool abortParsing = false;
  QElapsedTimer signalEmitTimer;
  signalEmitTimer.start();
  while (!file->atEnd() && !abortParsing)
  {
    // Update the progress dialog
    int64_t pos = file->pos();
    if (stream_info.file_size > 0)
      progressPercentValue = clip((int)(pos * 100 / stream_info.file_size), 0, 100);

    try
    {
      nalData = file->getNextNALUnit(false, &nalStartEndPosFile);
      if (!parseAndAddNALUnit(nalID, nalData, this->bitrateItemModel.data(), nullptr, nalStartEndPosFile))
      {
        DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Error parsing NAL %d", nalID);
      }
    }
    catch (const std::exception &exc)
    {
      Q_UNUSED(exc);
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Exception thrown parsing NAL %d - %s", nalID, exc.what());
    }
    catch (...)
    {
      DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Exception thrown parsing NAL %d", nalID);
    }

    nalID++;

    if (progressDialog)
    {
      // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
      if (progressDialog->wasCanceled())
        return false;

      int newPercentValue = 0;
      if (maxPos > 0)
        newPercentValue = clip(int((file->pos() - maxPos) * 100 / maxPos), 0, 100);
      if (newPercentValue != curPercentValue)
      {
        progressDialog->setValue(newPercentValue);
        curPercentValue = newPercentValue;
      }
    }

    if (signalEmitTimer.elapsed() > 1000 && packetModel)
    {
      signalEmitTimer.start();
      emit modelDataUpdated();
    }

    if (cancelBackgroundParser)
    {
      DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Abort parsing by user request.");
      abortParsing = true;
    }
    if (parsingLimitEnabled && frameList.size() > PARSER_FILE_FRAME_NR_LIMIT)
    {
      DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Abort parsing because frame limit was reached.");
      abortParsing = true;
    }
  }

  // We are done.
  parseAndAddNALUnit(-1, QByteArray(), this->bitrateItemModel.data());
  DEBUG_ANNEXB("parserAnnexB::parseAndAddNALUnit Parsing done. Found %d POCs.", POCList.length());

  if (packetModel)
    emit modelDataUpdated();

  stream_info.parsing = false;
  stream_info.nr_nal_units = nalID;
  stream_info.nr_frames = frameList.size();
  emit streamInfoUpdated();
  emit backgroundParsingDone("");

  return !cancelBackgroundParser;
}

bool parserAnnexB::runParsingOfFile(QString compressedFilePath)
{
  DEBUG_ANNEXB("playlistItemCompressedVideo::runParsingOfFile");
  QScopedPointer<fileSourceAnnexBFile> file(new fileSourceAnnexBFile(compressedFilePath));
  return parseAnnexBFile(file);
}

QList<QTreeWidgetItem*> parserAnnexB::stream_info_type::getStreamInfo()
{
  QList<QTreeWidgetItem*> infoList;
  infoList.append(new QTreeWidgetItem(QStringList() << "File size" << QString::number(file_size)));
  if (parsing)
  {
    infoList.append(new QTreeWidgetItem(QStringList() << "Number NAL units" << "Parsing..."));
    infoList.append(new QTreeWidgetItem(QStringList() << "Number Frames" << "Parsing..."));
  }
  else
  {
    infoList.append(new QTreeWidgetItem(QStringList() << "Number NAL units" << QString::number(nr_nal_units)));
    infoList.append(new QTreeWidgetItem(QStringList() << "Number Frames" << QString::number(nr_frames)));
  }

  return infoList;
}