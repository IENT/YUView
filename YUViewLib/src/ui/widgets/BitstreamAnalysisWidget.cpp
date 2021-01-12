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

#include "BitstreamAnalysisWidget.h"

#include "parser/AVC/AnnexBAVC.h"
#include "parser/AnnexBHEVC.h"
#include "parser/VVC/AnnexBVVC.h"
#include "parser/Mpeg2/AnnexBMpeg2.h"
#include "parser/AVFormat.h"

#define BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT 0
#if BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_ANALYSIS(msg) qDebug() << msg
#else
#define DEBUG_ANALYSIS(msg) ((void)0)
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

MoveAndZoomableView *BitstreamAnalysisWidget::getCurrentActiveView()
{
  const auto idx = this->ui.analysisTab->currentIndex();
  if (idx == 2)
    return this->ui.plotViewWidget;
  if (idx == 3)
    return this->ui.hrdPlotWidget;
  return {};
}

void BitstreamAnalysisWidget::updateParserItemModel()
{
  if (this->parser)
  {
    this->parser->updateNumberModelItems();
    this->updateParsingStatusText(this->parser->getParsingProgressPercent());
  }
}

void BitstreamAnalysisWidget::updateStreamInfo()
{
  this->ui.streamInfoTreeWidget->clear();
  this->ui.streamInfoTreeWidget->addTopLevelItems(this->parser->getStreamInfo());
  this->ui.streamInfoTreeWidget->expandAll();

  DEBUG_ANALYSIS("BitstreamAnalysisWidget::updateStreamInfo comboBox entries " << this->ui.showStreamComboBox->count() << 
                 " parser->getNrStreams " << this->parser->getNrStreams());
  int nrSelections = this->parser->getNrStreams();
  if (this->parser->getNrStreams() > 1)
    nrSelections += 1;
  if (this->ui.showStreamComboBox->count() != nrSelections)
  {
    this->ui.showStreamComboBox->clear();
    if (nrSelections == 1)
    {
      this->ui.showStreamComboBox->addItem("Show stream 0");
      this->ui.showStreamComboBox->setEnabled(false);
    }
    else
    {
      this->ui.showStreamComboBox->setEnabled(true);
      this->ui.showStreamComboBox->addItem("Show all streams");
      for (unsigned int i = 0; i < this->parser->getNrStreams(); i++)
      {
        QString info = this->parser->getShortStreamDescription(i);
        this->ui.showStreamComboBox->addItem(QString("Stream %1 - ").arg(i) + info);
      }
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
  if (this->parser.isNull())
    return;

  this->disconnect(this->parser.data(), &parser::Base::modelDataUpdated, this, &BitstreamAnalysisWidget::updateParserItemModel);
  this->disconnect(this->parser.data(), &parser::Base::streamInfoUpdated, this, &BitstreamAnalysisWidget::updateStreamInfo);
  this->disconnect(this->parser.data(), &parser::Base::backgroundParsingDone, this, &BitstreamAnalysisWidget::backgroundParsingDone);

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
    this->parser->runParsingOfFile(this->currentCompressedVideo->properties().name);
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

void BitstreamAnalysisWidget::updateSettings()
{
  this->ui.plotViewWidget->updateSettings();
  this->ui.hrdPlotWidget->updateSettings();
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
    this->ui.plotViewWidget->setModel(nullptr);
    this->ui.hrdPlotWidget->setModel(nullptr);
    return;
  }

  this->createAndConnectNewParser(this->currentCompressedVideo->getInputFormat());

  this->ui.dataTreeView->setModel(this->parser->getPacketItemModel());
  this->ui.dataTreeView->setColumnWidth(0, 600);
  this->ui.dataTreeView->setColumnWidth(1, 100);
  this->ui.dataTreeView->setColumnWidth(2, 120);
  this->ui.plotViewWidget->setModel(this->parser->getBitratePlotModel());
  this->ui.hrdPlotWidget->setModel(this->parser->getHRDPlotModel());

  this->updateStreamInfo();

  this->updateParsingStatusText(0);
  this->backgroundParserFuture = QtConcurrent::run(this, &BitstreamAnalysisWidget::backgroundParsingFunction);
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::restartParsingOfCurrentItem new parser created and started");
}

void BitstreamAnalysisWidget::createAndConnectNewParser(inputFormat inputFormatType)
{
  Q_ASSERT_X(!this->parser, Q_FUNC_INFO, "Error reinitlaizing parser. The current parser is not null.");
  if (inputFormatType == inputAnnexBHEVC)
    this->parser.reset(new parser::AnnexBHEVC(this));
  if (inputFormatType == inputAnnexBVVC)
    this->parser.reset(new parser::AnnexBVVC(this));
  else if (inputFormatType == inputAnnexBAVC)
    this->parser.reset(new parser::AnnexBAVC(this));
  else if (inputFormatType == inputLibavformat)
    this->parser.reset(new parser::AVFormat(this));
  this->parser->enableModel();
  const bool parsingLimitSet = !this->ui.parseEntireFileCheckBox->isChecked();
  this->parser->setParsingLimitEnabled(parsingLimitSet);

  this->connect(this->parser.data(), &parser::Base::modelDataUpdated, this, &BitstreamAnalysisWidget::updateParserItemModel);
  this->connect(this->parser.data(), &parser::Base::streamInfoUpdated, this, &BitstreamAnalysisWidget::updateStreamInfo);
  this->connect(this->parser.data(), &parser::Base::backgroundParsingDone, this, &BitstreamAnalysisWidget::backgroundParsingDone);
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
