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

#include "bitstreamAnalysisWidget.h"

#include "parser/parserAnnexBAVC.h"
#include "parser/parserAnnexBHEVC.h"
#include "parser/parserAnnexBVVC.h"
#include "parser/parserAnnexBMpeg2.h"
#include "parser/parserAVFormat.h"

#define BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT 0
#if BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_ANALYSIS qDebug
#else
#define DEBUG_ANALYSIS(fmt,...) ((void)0)
#endif

using namespace YUView;

BitstreamAnalysisWidget::BitstreamAnalysisWidget(QWidget *parent) :
  QWidget(parent)
{
  this->ui.setupUi(this);
  this->ui.streamInfoTreeWidget->setColumnWidth(0, 300);
  this->updateParsingStatusText(-1);

  this->connect(this->ui.showStreamComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BitstreamAnalysisWidget::showOnlyStreamComboBoxIndexChanged);
  this->connect(this->ui.colorCodeStreamsCheckBox, &QCheckBox::toggled, this, &BitstreamAnalysisWidget::colorCodeStreamsCheckBoxToggled);
  this->connect(this->ui.parseEntireFileCheckBox, &QCheckBox::toggled, this, &BitstreamAnalysisWidget::parseEntireBitstreamCheckBoxToggled);
  this->connect(this->ui.bitratePlotOrderComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BitstreamAnalysisWidget::bitratePlotOrderComboBoxIndexChanged);

  this->currentSelectedItemsChanged(nullptr, nullptr, false);
}

void BitstreamAnalysisWidget::updateParserItemModel()
{
  this->parser->updateNumberModelItems();
  this->updateParsingStatusText(this->parser->getParsingProgressPercent());
}

void BitstreamAnalysisWidget::updateStreamInfo()
{
  this->ui.streamInfoTreeWidget->clear();
  this->ui.streamInfoTreeWidget->addTopLevelItems(this->parser->getStreamInfo());
  this->ui.streamInfoTreeWidget->expandAll();

  if (this->ui.showStreamComboBox->count() + 1 != int(this->parser->getNrStreams()))
  {
    this->ui.showStreamComboBox->clear();
    this->ui.showStreamComboBox->addItem("Show all streams");
    for (unsigned int i = 0; i < this->parser->getNrStreams(); i++)
    {
      QString info = this->parser->getShortStreamDescription(i);
      this->ui.showStreamComboBox->addItem(QString("Stream %1 - ").arg(i) + info);
    }
  }
}

void BitstreamAnalysisWidget::backgroundParsingDone(QString error)
{
  if (error.isEmpty())
    this->ui.parsingStatusText->setText("Parsing done.");
  else
    this->ui.parsingStatusText->setText("Error parsing the file: " + error);
  this->updateParsingStatusText(100);
}

void BitstreamAnalysisWidget::showOnlyStreamComboBoxIndexChanged(int index)
{
  if (this->parser && this->showOnlyStream != index - 1)
  {
    this->showOnlyStream = index - 1;
    this->parser->setFilterStreamIndex(this->showOnlyStream);
  }
}

void BitstreamAnalysisWidget::bitratePlotOrderComboBoxIndexChanged(int index)
{
  if (this->parser)
  {
    this->parser->setBitrateSortingIndex(index);
    // Note: This was the only way I found to update the bar graph. None of the emit signal ways worked.
    this->ui.bitrateBarChart->setModel(nullptr);
    this->ui.bitrateBarChart->setModel(this->parser->getBitrateItemModel());
  }
}

void BitstreamAnalysisWidget::updateParsingStatusText(int progressValue)
{
  if (progressValue <= -1)
    this->ui.parsingStatusText->setText("No bitstream file selected - Select a bitstream file to start bitstream analysis.");
  else if (progressValue < 100)
    this->ui.parsingStatusText->setText(QString("Parsing file (%1%)").arg(progressValue));
  else
  {
    const bool parsingLimitSet = !this->ui.parseEntireFileCheckBox->isChecked();
    this->ui.parsingStatusText->setText(parsingLimitSet ? "Partial parsing done. Enable full parsing if needed." : "Parsing done.");
  }
}

void BitstreamAnalysisWidget::stopAndDeleteParserBlocking()
{
  this->disconnect(this->parser.data(), &parserBase::modelDataUpdated, this, &BitstreamAnalysisWidget::updateParserItemModel);
  this->disconnect(this->parser.data(), &parserBase::streamInfoUpdated, this, &BitstreamAnalysisWidget::updateStreamInfo);
  this->disconnect(this->parser.data(), &parserBase::backgroundParsingDone, this, &BitstreamAnalysisWidget::backgroundParsingDone);

  if (this->backgroundParserFuture.isRunning())
  {
    DEBUG_ANALYSIS("BitstreamAnalysisWidget::stopAndDeleteParser stopping parser");
    this->parser->setAbortParsing();
    this->backgroundParserFuture.waitForFinished();
  }
  this->parser.reset();
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::stopAndDeleteParser parser stopped and deleted");
}

void BitstreamAnalysisWidget::backgroundParsingFunction()
{
  if (this->parser)
    this->parser->runParsingOfFile(this->currentCompressedVideo->getName());
}

void BitstreamAnalysisWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2, bool chageByPlayback)
{
  Q_UNUSED(item2);
  Q_UNUSED(chageByPlayback);

  this->currentCompressedVideo = dynamic_cast<playlistItemCompressedVideo*>(item1);
  this->ui.streamInfoTreeWidget->clear();

  const bool isBitstream = !this->currentCompressedVideo.isNull();
  this->ui.tabStreamInfo->setEnabled(isBitstream);
  this->ui.tabPacketAnalysis->setEnabled(isBitstream);
  this->ui.tabBitrateGraphicsView->setEnabled(isBitstream);

  this->restartParsingOfCurrentItem();
}

void BitstreamAnalysisWidget::restartParsingOfCurrentItem()
{
  if (!this->isVisible())
  {
    DEBUG_ANALYSIS("BitstreamAnalysisWidget::restartParsingOfCurrentItem not visible - abort");
    return;
  }

  this->stopAndDeleteParserBlocking();
  
  if (this->currentCompressedVideo.isNull())
  {
    DEBUG_ANALYSIS("BitstreamAnalysisWidget::restartParsingOfCurrentItem no compressed video - abort");
    this->updateParsingStatusText(-1);
    this->ui.streamInfoTreeWidget->clear();
    this->ui.dataTreeView->setModel(nullptr);
    this->ui.bitrateBarChart->setModel(nullptr);
    this->parser.reset();
    return;
  }

  this->createAndConnectNewParser(this->currentCompressedVideo->getInputFormat());

  this->ui.dataTreeView->setModel(this->parser->getPacketItemModel());
  this->ui.dataTreeView->setColumnWidth(0, 600);
  this->ui.dataTreeView->setColumnWidth(1, 100);
  this->ui.dataTreeView->setColumnWidth(2, 120);
  this->ui.bitrateBarChart->setModel(this->parser->getBitrateItemModel());

  this->updateStreamInfo();

  this->updateParsingStatusText(0);
  this->backgroundParserFuture = QtConcurrent::run(this, &BitstreamAnalysisWidget::backgroundParsingFunction);
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::restartParsingOfCurrentItem new parser created and started");
}

void BitstreamAnalysisWidget::createAndConnectNewParser(inputFormat inputFormatType)
{
  Q_ASSERT_X(!this->parser, "BitstreamAnalysisWidget::restartParsingOfCurrentItem", "Error reinitlaizing parser. The current parser is not null.");
  if (inputFormatType == inputAnnexBHEVC)
    this->parser.reset(new parserAnnexBHEVC(this));
  if (inputFormatType == inputAnnexBVVC)
    this->parser.reset(new parserAnnexBVVC(this));
  else if (inputFormatType == inputAnnexBAVC)
    this->parser.reset(new parserAnnexBAVC(this));
  else if (inputFormatType == inputLibavformat)
    this->parser.reset(new parserAVFormat(this));
  this->parser->enableModel();
  const bool parsingLimitSet = !this->ui.parseEntireFileCheckBox->isChecked();
  this->parser->setParsingLimitEnabled(parsingLimitSet);

  this->connect(this->parser.data(), &parserBase::modelDataUpdated, this, &BitstreamAnalysisWidget::updateParserItemModel);
  this->connect(this->parser.data(), &parserBase::streamInfoUpdated, this, &BitstreamAnalysisWidget::updateStreamInfo);
  this->connect(this->parser.data(), &parserBase::backgroundParsingDone, this, &BitstreamAnalysisWidget::backgroundParsingDone);
}

void BitstreamAnalysisWidget::hideEvent(QHideEvent *event)
{
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::hideEvent");
  this->stopAndDeleteParserBlocking();
  QWidget::hideEvent(event);
}

void BitstreamAnalysisWidget::showEvent(QShowEvent *event)
{
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::showEvent");
  this->restartParsingOfCurrentItem();
  QWidget::showEvent(event);
}
