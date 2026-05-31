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

#include "parser/AVC/ParserAnnexBAVC.h"
#include "parser/AVFormat/ParserAVFormat.h"
#include "parser/HEVC/ParserAnnexBHEVC.h"
#include "parser/Mpeg2/ParserAnnexBMpeg2.h"
#include "parser/VVC/ParserAnnexBVVC.h"
#include <QSortFilterProxyModel>
#include <QTimer>

#define BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT 0
#if BITSTREAM_ANALYSIS_WIDGET_DEBUG_OUTPUT
#include <QDebug>
#define DEBUG_ANALYSIS(msg) qDebug() << msg
#else
#define DEBUG_ANALYSIS(msg) ((void)0)
#endif

BitstreamAnalysisWidget::BitstreamAnalysisWidget(QWidget *parent) : QWidget(parent)
{
  this->ui.setupUi(this);
  this->ui.streamInfoTreeWidget->setColumnWidth(0, 300);
  this->updateParsingStatusText(-1);

  this->connect(this->ui.showStreamComboBox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &BitstreamAnalysisWidget::showOnlyStreamComboBoxIndexChanged);
  this->connect(this->ui.colorCodeStreamsCheckBox,
                &QCheckBox::toggled,
                this,
                &BitstreamAnalysisWidget::colorCodeStreamsCheckBoxToggled);
  this->connect(this->ui.parseEntireFileCheckBox,
                &QCheckBox::toggled,
                this,
                &BitstreamAnalysisWidget::parseEntireBitstreamCheckBoxToggled);
this->connect(this->ui.bitratePlotOrderComboBox,
                 QOverload<int>::of(&QComboBox::currentIndexChanged),
                 this,
                 &BitstreamAnalysisWidget::bitratePlotOrderComboBoxIndexChanged);
  this->connect(this->ui.bitratePlotStreamComboBox,
                  QOverload<int>::of(&QComboBox::currentIndexChanged),
                  this,
                  &BitstreamAnalysisWidget::bitratePlotStreamComboBoxIndexChanged);

  this->connect(this->ui.showHexViewCheckBox,
                &QCheckBox::toggled,
                this,
                &BitstreamAnalysisWidget::onShowHexViewToggled);

  this->ui.dataTreeView->installEventFilter(this);

  this->ui.packetAnalysisSplitter->setStretchFactor(0, 3);  // TreeView
  this->ui.packetAnalysisSplitter->setStretchFactor(1, 2);  // HexView

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
  for (auto item : this->parser->getStreamInfo())
    this->ui.streamInfoTreeWidget->addTopLevelItem(item);
  this->ui.streamInfoTreeWidget->expandAll();

  DEBUG_ANALYSIS("BitstreamAnalysisWidget::updateStreamInfo comboBox entries "
                 << this->ui.showStreamComboBox->count() << " parser->getNrStreams "
                 << this->parser->getNrStreams());
  auto nrSelections = this->parser->getNrStreams();
  if (this->parser->getNrStreams() > 1)
    nrSelections += 1;
  if (this->ui.showStreamComboBox->count() != int(nrSelections))
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
      for (unsigned i = 0; i < this->parser->getNrStreams(); i++)
      {
        const auto info = this->parser->getShortStreamDescription(i);
this->ui.showStreamComboBox->addItem(QString("Stream %1 - ").arg(i) +
                                             QString::fromStdString(info));
       }
     }
   }

  auto nrStreams = this->parser->getNrStreams();
  auto nrBitrateSelections = nrStreams;
  if (nrStreams > 1)
    nrBitrateSelections += 1;
  if (this->ui.bitratePlotStreamComboBox->count() != int(nrBitrateSelections))
  {
    this->ui.bitratePlotStreamComboBox->clear();
    if (nrBitrateSelections == 1)
    {
      this->ui.bitratePlotStreamComboBox->addItem("Show stream 0");
      this->ui.bitratePlotStreamComboBox->setEnabled(false);
    }
    else
    {
      this->ui.bitratePlotStreamComboBox->setEnabled(true);
      this->ui.bitratePlotStreamComboBox->addItem("Show all streams");
      for (unsigned i = 0; i < nrStreams; i++)
      {
        const auto info = this->parser->getShortStreamDescription(i);
        this->ui.bitratePlotStreamComboBox->addItem(QString("Stream %1 - ").arg(i) +
                                                     QString::fromStdString(info));
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
    this->parser->setBitrateSortingIndex(index);
}

void BitstreamAnalysisWidget::bitratePlotStreamComboBoxIndexChanged(int index)
{
  const int selectedStream = index - 1;
  if (this->showOnlyBitrateStream == selectedStream)
    return;

  this->showOnlyBitrateStream = selectedStream;

  QList<unsigned int> showList;
  if (selectedStream == -1)
  {
    for (unsigned i = 0; i < this->parser->getNrStreams(); i++)
      showList.append(i);
  }
  else
  {
    showList.append(unsigned(selectedStream));
  }

  this->ui.plotViewWidget->setShowStreamList(showList);
}

void BitstreamAnalysisWidget::updateParsingStatusText(int progressValue)
{
  if (progressValue <= -1)
    this->ui.parsingStatusText->setText(
        "No bitstream file selected - Select a bitstream file to start bitstream analysis.");
  else if (progressValue < 100)
    this->ui.parsingStatusText->setText(QString("Parsing file (%1%)").arg(progressValue));
  else
  {
    const auto parsingLimitSet = !this->ui.parseEntireFileCheckBox->isChecked();
    this->ui.parsingStatusText->setText(
        parsingLimitSet ? "Partial parsing done. Enable full parsing if needed." : "Parsing done.");
  }
}

void BitstreamAnalysisWidget::stopAndDeleteParserBlocking()
{
  if (!this->parser)
    return;

  this->disconnect(this->parser.get(),
                   &parser::Parser::modelDataUpdated,
                   this,
                   &BitstreamAnalysisWidget::updateParserItemModel);
  this->disconnect(this->parser.get(),
                   &parser::Parser::streamInfoUpdated,
                   this,
                   &BitstreamAnalysisWidget::updateStreamInfo);
  this->disconnect(this->parser.get(),
                   &parser::Parser::backgroundParsingDone,
                   this,
                   &BitstreamAnalysisWidget::backgroundParsingDone);

  // Explicitly disconnect the selection model's currentChanged signal.
  // setModel(nullptr) uses deleteLater() on the old selection model, so the old
  // app-level connection may still fire if the selection model emits currentChanged
  // during teardown — use-after-free on the TreeItem* stored as internalPointer.
  if (this->ui.dataTreeView->selectionModel())
  {
    QObject::disconnect(this->ui.dataTreeView->selectionModel(),
                        &QItemSelectionModel::currentChanged,
                        this,
                        &BitstreamAnalysisWidget::onDataTreeViewSelectionChanged);
  }

  if (this->backgroundParserFuture.isRunning())
  {
    DEBUG_ANALYSIS("BitstreamAnalysisWidget::stopAndDeleteParser stopping parser");
    this->parser->setAbortParsing();
    this->backgroundParserFuture.waitForFinished();
  }

  this->ui.dataTreeView->setModel(nullptr);
  this->ui.plotViewWidget->setModel(nullptr);
  this->ui.hrdPlotWidget->setModel(nullptr);

  this->parser.reset();
  this->currentHighlightNalRoot.reset();
  DEBUG_ANALYSIS("BitstreamAnalysisWidget::stopAndDeleteParser parser stopped and deleted");
}

void BitstreamAnalysisWidget::backgroundParsingFunction()
{
  if (this->parser)
    this->parser->runParsingOfFile(this->currentCompressedVideo->properties().name.toStdString());
}

void BitstreamAnalysisWidget::currentSelectedItemsChanged(playlistItem *item1, playlistItem *, bool)
{
  this->currentCompressedVideo = dynamic_cast<playlistItemCompressedVideo *>(item1);
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
    DEBUG_ANALYSIS(
        "BitstreamAnalysisWidget::restartParsingOfCurrentItem no compressed video - abort");
    this->updateParsingStatusText(-1);
    this->ui.streamInfoTreeWidget->clear();
    this->ui.dataTreeView->setModel(nullptr);
    this->ui.plotViewWidget->setModel(nullptr);
    this->ui.hrdPlotWidget->setModel(nullptr);
    return;
  }

  this->createAndConnectNewParser(this->currentCompressedVideo->getInputFormat());

  this->ui.dataTreeView->setModel(this->parser->getPacketItemModel());
  this->ui.dataTreeView->setColumnWidth(0, 400);
  this->ui.dataTreeView->setColumnWidth(1, 100);
  this->ui.dataTreeView->setColumnWidth(2, 120);
  this->ui.plotViewWidget->setModel(this->parser->getBitratePlotModel());
  this->ui.hrdPlotWidget->setModel(this->parser->getHRDPlotModel());

  this->currentHighlightNalRoot.reset();
  this->ui.hexViewWidget->clear();
  this->ui.hexViewWidget->setVisible(this->ui.showHexViewCheckBox->isChecked());

  if (this->parser)
  {
    this->connect(this->ui.dataTreeView->selectionModel(),
                  &QItemSelectionModel::currentChanged,
                  this,
                  &BitstreamAnalysisWidget::onDataTreeViewSelectionChanged);
  }

  this->updateStreamInfo();

  this->updateParsingStatusText(0);
  this->backgroundParserFuture =
      QtConcurrent::run([=](BitstreamAnalysisWidget *b) { b->backgroundParsingFunction(); }, this);
  DEBUG_ANALYSIS(
      "BitstreamAnalysisWidget::restartParsingOfCurrentItem new parser created and started");
}

void BitstreamAnalysisWidget::createAndConnectNewParser(InputFormat inputFormat)
{
  Q_ASSERT_X(
      !this->parser, Q_FUNC_INFO, "Error reinitlaizing parser. The current parser is not null.");
  if (inputFormat == InputFormat::AnnexBHEVC)
    this->parser.reset(new parser::ParserAnnexBHEVC(this));
  if (inputFormat == InputFormat::AnnexBVVC)
    this->parser.reset(new parser::ParserAnnexBVVC(this));
  else if (inputFormat == InputFormat::AnnexBAVC)
    this->parser.reset(new parser::ParserAnnexBAVC(this));
  else if (inputFormat == InputFormat::Libav)
    this->parser.reset(new parser::ParserAVFormat(this));
  this->parser->enableModel();
  const bool parsingLimitSet = !this->ui.parseEntireFileCheckBox->isChecked();
  this->parser->setParsingLimitEnabled(parsingLimitSet);

  this->connect(this->parser.get(),
                &parser::Parser::modelDataUpdated,
                this,
                &BitstreamAnalysisWidget::updateParserItemModel);
  this->connect(this->parser.get(),
                &parser::Parser::streamInfoUpdated,
                this,
                &BitstreamAnalysisWidget::updateStreamInfo);
  this->connect(this->parser.get(),
                &parser::Parser::backgroundParsingDone,
                this,
                &BitstreamAnalysisWidget::backgroundParsingDone);
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

void BitstreamAnalysisWidget::onShowHexViewToggled(bool checked)
{
  this->ui.hexViewWidget->setVisible(checked);
}

bool BitstreamAnalysisWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == this->ui.dataTreeView && event->type() == QEvent::KeyPress)
  {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
    {
      auto idx = this->ui.dataTreeView->currentIndex();
      if (idx.isValid())
        this->ui.dataTreeView->setExpanded(idx, !this->ui.dataTreeView->isExpanded(idx));
      this->ui.dataTreeView->scrollTo(this->ui.dataTreeView->currentIndex(),
                                       QAbstractItemView::PositionAtCenter);
      return true;
    }
    if (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down ||
        keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right ||
        keyEvent->key() == Qt::Key_PageUp || keyEvent->key() == Qt::Key_PageDown ||
        keyEvent->key() == Qt::Key_Home || keyEvent->key() == Qt::Key_End)
    {
      event->accept();
      auto result = QWidget::eventFilter(watched, event);
      QTimer::singleShot(0, this, [this]() {
        auto idx = this->ui.dataTreeView->currentIndex();
        if (idx.isValid())
          this->ui.dataTreeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
      });
      return result;
    }
  }
  return QWidget::eventFilter(watched, event);
}

struct NalRootResult
{
  TreeItem *nalRoot{};
  TreeItem *selectedItem{};
};

static TreeItem *findFirstDescendantWithRawData(TreeItem *item, int maxDepth)
{
  if (!item || maxDepth <= 0)
    return {};
  for (unsigned i = 0; i < item->getNrChildItems(); i++)
  {
    auto child = item->getChild(i);
    if (child && child->getRawData().has_value())
      return child.get();
    if (auto *found = findFirstDescendantWithRawData(child.get(), maxDepth - 1))
      return found;
  }
  return {};
}

static NalRootResult findNalRootItemByModelIndex(QModelIndex idx,
                                                 const QAbstractItemModel *model)
{
  NalRootResult result;
  auto *firstItem = static_cast<TreeItem *>(idx.internalPointer());
  if (!firstItem)
    return result;
  result.selectedItem = firstItem;

  constexpr int maxDepth = 100;
  for (int depth = 0; depth < maxDepth && idx.isValid(); depth++)
  {
    auto *item = static_cast<TreeItem *>(idx.internalPointer());
    if (item && item->getRawData().has_value())
    {
      result.nalRoot = item;
      return result;
    }
    idx = model->parent(idx);
  }

  if (auto *child = findFirstDescendantWithRawData(firstItem, maxDepth))
  {
    result.nalRoot = child;
    if (!result.selectedItem->getRawData().has_value())
      result.selectedItem = child;
  }

  return result;
}

static size_t computeBitLength(TreeItem *item, size_t totalBits)
{
  auto code = item->getData(3);
  const size_t codeLen = code.empty() ? 0 : code.size();

  auto parent = item->getParentItem().lock();
  if (parent)
  {
    auto self = item->shared_from_this();
    auto idx  = parent->getIndexOfChildItem(self);
    if (idx)
    {
      auto nextSibling = parent->getChild(unsigned(*idx + 1));
      if (nextSibling)
      {
        size_t bitLen = nextSibling->getBitOffset() - item->getBitOffset();
        if (bitLen > 0 && (codeLen == 0 || bitLen <= codeLen))
          return bitLen;
      }
    }
  }

  if (codeLen > 0)
    return codeLen;

  return totalBits - item->getBitOffset();
}

void BitstreamAnalysisWidget::onDataTreeViewSelectionChanged(const QModelIndex &current,
                                                               const QModelIndex &)
{
  if (!current.isValid() || !this->parser)
  {
    this->currentHighlightNalRoot.reset();
    this->ui.hexViewWidget->clear();
    return;
  }

  auto *proxyModel = qobject_cast<QSortFilterProxyModel *>(this->ui.dataTreeView->model());
  QModelIndex sourceIdx = proxyModel ? proxyModel->mapToSource(current) : current;
  if (!sourceIdx.isValid())
  {
    this->currentHighlightNalRoot.reset();
    this->ui.hexViewWidget->clear();
    return;
  }

  auto *sourceModel = proxyModel ? proxyModel->sourceModel() : this->ui.dataTreeView->model();
  auto result = findNalRootItemByModelIndex(sourceIdx, sourceModel);
  if (!result.nalRoot)
  {
    this->currentHighlightNalRoot.reset();
    this->ui.hexViewWidget->clear();
    return;
  }

  std::shared_ptr<TreeItem> nalRoot;
  try
  {
    nalRoot = result.nalRoot->shared_from_this();
  }
  catch (const std::bad_weak_ptr &)
  {
    this->currentHighlightNalRoot.reset();
    this->ui.hexViewWidget->clear();
    return;
  }

  if (nalRoot.get() != this->currentHighlightNalRoot.get())
    this->currentHighlightNalRoot = nalRoot;

  const auto &rawData = nalRoot->getRawData().value();
  QByteArray byteData(reinterpret_cast<const char *>(rawData.data()), int(rawData.size()));
  this->ui.hexViewWidget->setData(byteData);

  if (result.selectedItem)
  {
    const auto coding = result.selectedItem->getData(2);
    const auto code   = result.selectedItem->getData(3);
    if (!code.empty() && coding != "Calc")
    {
      const size_t totalBits = rawData.size() * 8;
      const size_t bitStart  = result.selectedItem->getBitOffset();
      if (bitStart < totalBits)
      {
        const size_t bitLen    = computeBitLength(result.selectedItem, totalBits);
        const int    byteOff   = int(bitStart / 8);
        const int    byteLen   = int((bitStart + bitLen + 7) / 8) - byteOff;
        this->ui.hexViewWidget->setHighlight(byteOff, byteLen);
      }
    }
  }

}
