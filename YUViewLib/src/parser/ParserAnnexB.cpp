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

#include "ParserAnnexB.h"

#include <common/Formatting.h>
#include <parser/common/SubByteReaderLogging.h>

#include <QElapsedTimer>
#include <QProgressDialog>
#include <assert.h>

#define PARSERANNEXB_DEBUG_OUTPUT 0
#if PARSERANNEXB_DEBUG_OUTPUT && !NDEBUG
#include <QDebug>
#define DEBUG_ANNEXB(msg) qDebug() << msg
#else
#define DEBUG_ANNEXB(msg) ((void)0)
#endif

namespace parser
{

std::string ParserAnnexB::getShortStreamDescription(const int) const
{
  std::ostringstream info;
  info << "Video";
  if (const auto frameSize = this->getSequenceSizeSamples())
    info << " " << frameSize;
  return info.str();
}

bool ParserAnnexB::addFrameToList(int                       poc,
                                  std::optional<pairUint64> fileStartEndPos,
                                  bool                      randomAccessPoint,
                                  unsigned                  layerID)
{
  for (const auto &f : this->frameListCodingOrder)
    if (f.poc == poc && f.layerID == layerID)
      return false;

  if (!this->pocOfFirstRandomAccessFrame && randomAccessPoint)
    this->pocOfFirstRandomAccessFrame = poc;
  if (poc >= this->pocOfFirstRandomAccessFrame.value())
  {
    // We don't add frames which we can not decode because they are before the first RA (I) frame
    AnnexBFrame newFrame;
    newFrame.poc               = poc;
    newFrame.fileStartEndPos   = fileStartEndPos;
    newFrame.randomAccessPoint = randomAccessPoint;
    newFrame.layerID           = layerID;
    this->frameListCodingOrder.push_back(newFrame);
    this->frameListDisplayOder.clear();
  }
  return true;
}

void ParserAnnexB::logNALSize(const ByteVector         &data,
                              std::shared_ptr<TreeItem> root,
                              std::optional<pairUint64> nalStartEndPos)
{
  size_t startCodeSize = 0;
  if (data[0] == char(0) && data[1] == char(0) && data[2] == char(0) && data[3] == char(1))
    startCodeSize = 4;
  if (data[0] == char(0) && data[1] == char(0) && data[2] == char(1))
    startCodeSize = 3;

  if (startCodeSize > 0)
    root->createChildItem("Start code size", startCodeSize);

  root->createChildItem("Payload size", data.size() - startCodeSize);
  if (nalStartEndPos)
    root->createChildItem("Start/End pos", to_string(*nalStartEndPos));
}

auto ParserAnnexB::getClosestSeekPoint(FrameIndexDisplayOrder targetFrame,
                                       FrameIndexDisplayOrder currentFrame) -> SeekPointInfo
{
  if (targetFrame >= this->frameListCodingOrder.size())
    return {};

  this->updateFrameListDisplayOrder();
  auto frameTarget  = this->frameListDisplayOder[targetFrame];
  auto frameCurrent = this->frameListDisplayOder[currentFrame];

  auto bestSeekFrame = this->frameListCodingOrder.begin();
  for (auto it = this->frameListCodingOrder.begin(); it != this->frameListCodingOrder.end(); it++)
  {
    if (it->randomAccessPoint && it->poc < frameTarget.poc)
      bestSeekFrame = it;
    if (it->poc == frameTarget.poc)
      break;
  }

  SeekPointInfo seekPointInfo;
  {
    auto itInDisplayOrder = std::find(
        this->frameListDisplayOder.begin(), this->frameListDisplayOder.end(), *bestSeekFrame);
    seekPointInfo.frameIndex = std::distance(this->frameListDisplayOder.begin(), itInDisplayOrder);
  }
  {
    auto itCurrentFrameCodingOrder = std::find(
        this->frameListCodingOrder.begin(), this->frameListCodingOrder.end(), frameCurrent);
    seekPointInfo.frameDistanceInCodingOrder =
        std::distance(itCurrentFrameCodingOrder, bestSeekFrame);
  }

  DEBUG_ANNEXB("ParserAnnexB::getClosestSeekPoint targetFrame "
               << targetFrame << "(POC " << frameTarget.poc << " seek to "
               << seekPointInfo.frameIndex << " (POC " << bestSeekFrame->poc
               << ") distance in coding order " << seekPointInfo.frameDistanceInCodingOrder);
  return seekPointInfo;
}

std::optional<pairUint64> ParserAnnexB::getFrameStartEndPos(FrameIndexCodingOrder idx)
{
  if (idx >= this->frameListCodingOrder.size())
    return {};
  this->updateFrameListDisplayOrder();
  return this->frameListCodingOrder[idx].fileStartEndPos;
}

bool ParserAnnexB::parseAnnexBFile(std::unique_ptr<FileSourceAnnexBFile> &file, QWidget *mainWindow)
{
  DEBUG_ANNEXB("ParserAnnexB::parseAnnexBFile");

  const auto                       fileSize = file->getFileSize();
  std::unique_ptr<QProgressDialog> progressDialog;
  int                              curPercentValue = 0;
  if (mainWindow)
  {
    // Show a modal QProgressDialog while this operation is running.
    // If the user presses cancel, we will cancel and return false (opening the file failed).
    // First, get a pointer to the main window to use as a parent for the modal parsing progress
    // dialog.
    progressDialog.reset(
        new QProgressDialog("Parsing AnnexB bitstream...", "Cancel", 0, 100, mainWindow));
    progressDialog->setMinimumDuration(1000); // Show after 1s
    progressDialog->setAutoClose(false);
    progressDialog->setAutoReset(false);
    progressDialog->setWindowModality(Qt::WindowModal);
  }

  this->streamInfo.file_size = file->getFileSize().value_or(0);
  this->streamInfo.parsing   = true;
  emit streamInfoUpdated();

  // Just push all NAL units from the annexBFile into the annexBParser
  int           nalID = 0;
  pairUint64    nalStartEndPosFile;
  bool          abortParsing = false;
  QElapsedTimer signalEmitTimer;
  signalEmitTimer.start();
  while (!file->atEnd() && !abortParsing)
  {
    // Update the progress dialog
    int64_t pos = file->pos();
    if (this->streamInfo.file_size > 0)
      progressPercentValue = functions::clip((int)(pos * 100 / this->streamInfo.file_size), 0, 100);

    try
    {
      auto nalData = reader::SubByteReaderLogging::convertToByteVector(
          file->getNextNALUnit(false, &nalStartEndPosFile));
      auto parsingResult =
          this->parseAndAddNALUnit(nalID, nalData, {}, nalStartEndPosFile, nullptr);
      if (!parsingResult.success)
      {
        DEBUG_ANNEXB("ParserAnnexB::parseAndAddNALUnit Error parsing NAL " << nalID);
      }
      else if (parsingResult.bitrateEntry)
      {
        this->bitratePlotModel->addBitratePoint(0, *parsingResult.bitrateEntry);
      }
    }
    catch (const std::exception &exc)
    {
      (void)exc;
      // Reading a NAL unit failed at some point.
      // This is not too bad. Just don't use this NAL unit and continue with the next one.
      DEBUG_ANNEXB("ParserAnnexB::parseAndAddNALUnit Exception thrown parsing NAL "
                   << nalID << " - " << exc.what());
    }
    catch (...)
    {
      DEBUG_ANNEXB("ParserAnnexB::parseAndAddNALUnit Exception thrown parsing NAL " << nalID);
    }

    nalID++;

    if (progressDialog)
    {
      // Updating the dialog (setValue) is quite slow. Only do this if the percent value changes.
      if (progressDialog->wasCanceled())
        return false;

      int newPercentValue = 0;
      if (fileSize)
        newPercentValue = functions::clip(int((file->pos() - *fileSize) * 100 / *fileSize), 0, 100);
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
      DEBUG_ANNEXB("ParserAnnexB::parseAndAddNALUnit Abort parsing by user request.");
      abortParsing = true;
    }
    if (this->parsingLimitEnabled && this->frameListCodingOrder.size() > PARSER_FILE_FRAME_NR_LIMIT)
    {
      DEBUG_ANNEXB(
          "ParserAnnexB::parseAndAddNALUnit Abort parsing because frame limit was reached.");
      abortParsing = true;
    }
  }

  try
  {
    auto parseResult = this->parseAndAddNALUnit(-1, {}, {}, {});
    if (!parseResult.success)
      DEBUG_ANNEXB(
          "ParserAnnexB::parseAndAddNALUnit Error finalizing parsing. This should not happen.");
  }
  catch (...)
  {
    DEBUG_ANNEXB(
        "ParserAnnexB::parseAndAddNALUnit Error finalizing parsing. This should not happen.");
  }

  DEBUG_ANNEXB("ParserAnnexB::parseAndAddNALUnit Parsing done. Found "
               << this->frameListCodingOrder.size() << " POCs");

  if (packetModel)
    emit modelDataUpdated();

  this->streamInfo.parsing    = false;
  this->streamInfo.nrNalUnits = nalID;
  this->streamInfo.nrFrames   = unsigned(this->frameListCodingOrder.size());
  emit streamInfoUpdated();
  emit backgroundParsingDone("");

  return !cancelBackgroundParser;
}

bool ParserAnnexB::runParsingOfFile(const std::filesystem::path &compressedFilePath)
{
  DEBUG_ANNEXB("playlistItemCompressedVideo::runParsingOfFile");
  auto file = std::make_unique<FileSourceAnnexBFile>(compressedFilePath);
  return this->parseAnnexBFile(file);
}

vector<QTreeWidgetItem *> ParserAnnexB::createTreeItemsFromStreamInfo() const
{
  vector<QTreeWidgetItem *> infoList;
  infoList.push_back(new QTreeWidgetItem(
      QStringList() << "File size" << QString::number(this->streamInfo.file_size)));
  if (this->streamInfo.parsing)
  {
    infoList.push_back(new QTreeWidgetItem(QStringList() << "Number NAL units"
                                                         << "Parsing..."));
    infoList.push_back(new QTreeWidgetItem(QStringList() << "Number Frames"
                                                         << "Parsing..."));
  }
  else
  {
    infoList.push_back(new QTreeWidgetItem(
        QStringList() << "Number NAL units" << QString::number(this->streamInfo.nrNalUnits)));
    infoList.push_back(new QTreeWidgetItem(
        QStringList() << "Number Frames" << QString::number(this->streamInfo.nrFrames)));
  }

  return infoList;
}

int ParserAnnexB::getFramePOC(FrameIndexDisplayOrder frameIdx)
{
  this->updateFrameListDisplayOrder();
  return this->frameListDisplayOder[frameIdx].poc;
}

void ParserAnnexB::updateFrameListDisplayOrder()
{
  if (this->frameListCodingOrder.size() == 0 || this->frameListDisplayOder.size() > 0)
    return;

  this->frameListDisplayOder = this->frameListCodingOrder;
  std::sort(frameListDisplayOder.begin(), frameListDisplayOder.end());
}

} // namespace parser