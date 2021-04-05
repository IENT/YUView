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

#include "playlistItemResample.h"

#include <QPainter>

#include "common/functions.h"

// Activate this if you want to know when which difference is loaded
#define PLAYLISTITEMRESAMPLE_DEBUG_LOADING 0
#if PLAYLISTITEMRESAMPLE_DEBUG_LOADING && !NDEBUG
#define DEBUG_RESAMPLE qDebug
#else
#define DEBUG_RESAMPLE(fmt, ...) ((void)0)
#endif

#define RESAMPLE_INFO_TEXT "Please drop an item onto this item to show a resampled version of it."

playlistItemResample::playlistItemResample() : playlistItemContainer("Resample Item")
{
  this->setIcon(0, functions::convertIcon(":img_resample.png"));
  this->setFlags(flags() | Qt::ItemIsDropEnabled);

  this->prop.propertiesWidgetTitle = "Resample Properties";

  this->maxItemCount   = 1;
  this->frameLimitsMax = false;
  this->infoText       = RESAMPLE_INFO_TEXT;

  this->connect(&this->video,
                &frameHandler::signalHandlerChanged,
                this,
                &playlistItemResample::signalItemChanged);
}

/* For a resample item, the info list is just the name of the child item
 */
infoData playlistItemResample::getInfo() const
{
  infoData info("Resample Info");

  if (this->childCount() >= 1)
    info.items.append(
        infoItem(QString("File 1"), this->getChildPlaylistItem(0)->properties().name));

  return info;
}

void playlistItemResample::drawItem(QPainter *painter,
                                    int       frameIdx,
                                    double    zoomFactor,
                                    bool      drawRawData)
{
  DEBUG_RESAMPLE("playlistItemResample::drawItem frameIdx %d %s",
                 frameIdx,
                 childLlistUpdateRequired ? "childLlistUpdateRequired" : "");
  if (this->childLlistUpdateRequired)
  {
    this->updateChildList();

    if (this->childCount() == 1)
    {
      auto child        = this->getChildPlaylistItem(0);
      auto frameHandler = child->getFrameHandler();

      this->prop.isFileSource = child->properties().isFileSource;
      this->prop.name         = child->properties().name + " resampled";

      this->video.setInputVideo(frameHandler);

      if (this->video.inputValid())
      {
        if (this->useLoadedValues)
        {
          // Values are already set from loading
          this->useLoadedValues = false;
        }
        else
        {
          this->scaledSize = frameHandler->getFrameSize();
          this->cutRange   = child->properties().startEndRange;
          this->sampling   = 1;
        }

        if (ui.created())
        {
          QSignalBlocker blockerWidth(ui.spinBoxWidth);
          QSignalBlocker blockerHeight(ui.spinBoxHeight);
          ui.spinBoxWidth->setValue(this->scaledSize.width());
          ui.spinBoxHeight->setValue(this->scaledSize.height());

          auto nrFramesInput = this->cutRange.second - this->cutRange.first + 1;

          QSignalBlocker blockerSampling(ui.spinBoxSampling);
          ui.spinBoxSampling->setMinimum(1);
          ui.spinBoxSampling->setMaximum(nrFramesInput);
          ui.spinBoxSampling->setValue(this->sampling);

          QSignalBlocker blockerStart(ui.spinBoxStart);
          ui.spinBoxStart->setMinimum(this->cutRange.first);
          ui.spinBoxStart->setMaximum(this->cutRange.second);
          ui.spinBoxStart->setValue(this->cutRange.first);

          QSignalBlocker blockerEnd(ui.spinBoxEnd);
          ui.spinBoxEnd->setMinimum(this->cutRange.first);
          ui.spinBoxEnd->setMaximum(this->cutRange.second);
          ui.spinBoxEnd->setValue(this->cutRange.second);

          auto sampleAspectRatio = child->properties().sampleAspectRatio;
          auto sarEnabled        = sampleAspectRatio.num != sampleAspectRatio.den;
          ui.labelSAR->setEnabled(sarEnabled);
          ui.pushButtonSARWidth->setEnabled(sarEnabled);
          ui.pushButtonSARHeight->setEnabled(sarEnabled);
        }

        this->video.setScaledSize(this->scaledSize);
        auto interpolation = (this->interpolationIndex == 0)
                                 ? videoHandlerResample::Interpolation::Bilinear
                                 : videoHandlerResample::Interpolation::Fast;
        this->video.setInterpolation(interpolation);
        this->video.setCutAndSample(this->cutRange, this->sampling);
        auto nrFrames            = (this->cutRange.second - this->cutRange.first) / this->sampling;
        this->prop.startEndRange = indexRange(0, nrFrames);
      }
      else
      {
        ui.labelSAR->setEnabled(false);
        ui.pushButtonSARWidth->setEnabled(false);
        ui.pushButtonSARHeight->setEnabled(false);
      }
    }
  }

  if (this->childCount() != 1 || !this->video.inputValid())
    // Draw the emptyText
    playlistItem::drawItem(painter, -1, zoomFactor, drawRawData);
  else
  {
    // draw the videoHandler
    this->video.drawFrame(painter, frameIdx, zoomFactor, drawRawData);
  }
}

QSize playlistItemResample::getSize() const
{
  if (!this->video.inputValid())
    return playlistItemContainer::getSize();

  return this->video.getFrameSize();
}

void playlistItemResample::createPropertiesWidget()
{
  Q_ASSERT_X(!this->propertiesWidget, "createPropertiesWidget", "Properties widget already exists");

  this->preparePropertiesWidget(QStringLiteral("playlistItemResample"));

  // On the top level everything is layout vertically
  auto vAllLaout = new QVBoxLayout(propertiesWidget.data());

  ui.setupUi();

  ui.comboBoxInterpolation->addItems(QStringList() << "Bilinear"
                                                   << "Linear");
  ui.comboBoxInterpolation->setCurrentIndex(0);

  ui.labelSAR->setEnabled(false);
  ui.pushButtonSARWidth->setEnabled(false);
  ui.pushButtonSARHeight->setEnabled(false);

  this->connect(ui.spinBoxWidth,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &playlistItemResample::slotResampleControlChanged);
  this->connect(ui.spinBoxHeight,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &playlistItemResample::slotResampleControlChanged);
  this->connect(ui.comboBoxInterpolation,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &playlistItemResample::slotInterpolationModeChanged);
  this->connect(ui.spinBoxStart,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &playlistItemResample::slotCutAndSampleControlChanged);
  this->connect(ui.spinBoxEnd,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &playlistItemResample::slotCutAndSampleControlChanged);
  this->connect(ui.spinBoxSampling,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &playlistItemResample::slotCutAndSampleControlChanged);
  this->connect(ui.pushButtonSARWidth,
                &QPushButton::clicked,
                this,
                &playlistItemResample::slotButtonSARWidth);
  this->connect(ui.pushButtonSARHeight,
                &QPushButton::clicked,
                this,
                &playlistItemResample::slotButtonSARHeight);

  vAllLaout->addLayout(ui.topVBoxLayout);
}

void playlistItemResample::savePlaylist(QDomElement &root, const QDir &playlistDir) const
{
  YUViewDomElement d = root.ownerDocument().createElement("playlistItemResample");

  // Append the indexed item's properties
  playlistItem::appendPropertiesToPlaylist(d);

  if (ui.created())
  {
    // Append the video handler properties
    d.appendProperiteChild("width", QString::number(this->scaledSize.width()));
    d.appendProperiteChild("height", QString::number(this->scaledSize.height()));
    d.appendProperiteChild("interpolation", QString::number(this->interpolationIndex));
    d.appendProperiteChild("cutStart", QString::number(this->cutRange.first));
    d.appendProperiteChild("cutEnd", QString::number(this->cutRange.second));
    d.appendProperiteChild("sampling", QString::number(this->sampling));
  }

  playlistItemContainer::savePlaylistChildren(d, playlistDir);

  root.appendChild(d);
}

playlistItemResample *playlistItemResample::newPlaylistItemResample(const YUViewDomElement &root)
{
  auto newItemResample = new playlistItemResample();

  // Load properties from the parent classes
  playlistItem::loadPropertiesFromPlaylist(root, newItemResample);

  newItemResample->scaledSize =
      QSize(root.findChildValueInt("width", 0), root.findChildValueInt("height", 0));
  newItemResample->interpolationIndex = root.findChildValueInt("interpolation", 0);
  newItemResample->cutRange =
      indexRange({root.findChildValueInt("cutStart", 0), root.findChildValueInt("cutEnd", 0)});
  newItemResample->sampling = root.findChildValueInt("sampling", 0);

  newItemResample->useLoadedValues = true;

  return newItemResample;
}

ValuePairListSets playlistItemResample::getPixelValues(const QPoint &pixelPos, int frameIdx)
{
  return ValuePairListSets("RGB", this->video.getPixelValues(pixelPos, frameIdx));
}

itemLoadingState playlistItemResample::needsLoading(int frameIdx, bool loadRawData)
{

  return this->video.needsLoading(frameIdx, loadRawData);
}

void playlistItemResample::loadFrame(int frameIdx, bool playing, bool loadRawData, bool emitSignals)
{
  (void)playing;
  if (this->childCount() != 1 || !this->video.inputValid())
    return;

  DEBUG_RESAMPLE(
      "playlistItemResample::loadFrame frameIdx %d %s", frameIdx, playing ? "(playing)" : "");

  auto state = this->video.needsLoading(frameIdx, loadRawData);
  if (state == LoadingNeeded)
  {
    // Load the requested current frame
    DEBUG_RESAMPLE("playlistItemResample::loadFrame loading resampled frame %d", frameIdx);
    this->isFrameLoading = true;
    this->video.loadResampledFrame(frameIdx);
    this->isFrameLoading = false;
    if (emitSignals)
      emit signalItemChanged(true, RECACHE_NONE);
  }

  if (playing && (state == LoadingNeeded || state == LoadingNeededDoubleBuffer))
  {
    // Load the next frame into the double buffer
    int nextFrameIdx = frameIdx + 1;
    if (nextFrameIdx <= this->properties().startEndRange.second)
    {
      DEBUG_RESAMPLE(
          "playlistItemResample::loadFrame loading resampled frame into double buffer %d %s",
          nextFrameIdx,
          playing ? "(playing)" : "");
      this->isFrameLoadingDoubleBuffer = true;
      this->video.loadResampledFrame(nextFrameIdx, true);
      this->isFrameLoadingDoubleBuffer = false;
      if (emitSignals)
        emit signalItemDoubleBufferLoaded();
    }
  }
}

void playlistItemResample::childChanged(bool redraw, recacheIndicator recache)
{
  // The child item changed and needs to redraw. This means that the resampled frame is out of date
  // and has to be recalculated.

  auto nrFrames            = (this->cutRange.second - this->cutRange.first) / this->sampling;
  this->prop.startEndRange = indexRange(0, nrFrames);

  this->video.invalidateAllBuffers();
  playlistItemContainer::childChanged(redraw, recache);
}

void playlistItemResample::slotResampleControlChanged(int)
{
  this->scaledSize = QSize(ui.spinBoxWidth->value(), ui.spinBoxHeight->value());
  this->video.setScaledSize(this->scaledSize);
}

void playlistItemResample::slotInterpolationModeChanged(int)
{
  this->interpolationIndex = ui.comboBoxInterpolation->currentIndex();
  auto interpolation       = (this->interpolationIndex == 0)
                                 ? videoHandlerResample::Interpolation::Bilinear
                                 : videoHandlerResample::Interpolation::Fast;
  this->video.setInterpolation(interpolation);
}

void playlistItemResample::slotCutAndSampleControlChanged(int)
{
  this->cutRange = indexRange(ui.spinBoxStart->value(), ui.spinBoxEnd->value());
  this->sampling = std::max(ui.spinBoxSampling->value(), 1);

  auto nrFrames            = (this->cutRange.second - this->cutRange.first) / this->sampling;
  this->prop.startEndRange = indexRange(0, nrFrames);

  this->video.setCutAndSample(this->cutRange, this->sampling);
}

void playlistItemResample::slotButtonSARWidth(bool)
{
  if (this->childCount() <= 0)
    return;

  auto childSize = this->getChildPlaylistItem(0)->getFrameHandler()->getFrameSize();
  auto childSAR  = this->getChildPlaylistItem(0)->properties().sampleAspectRatio;

  auto newHeight = childSize.height();
  auto newWidth  = newHeight * childSAR.den / childSAR.num;

  this->scaledSize = QSize(newWidth, newHeight);
  this->video.setScaledSize(this->scaledSize);
}

void playlistItemResample::slotButtonSARHeight(bool)
{
  if (this->childCount() <= 0)
    return;

  auto childSize = this->getChildPlaylistItem(0)->getFrameHandler()->getFrameSize();
  auto childSAR  = this->getChildPlaylistItem(0)->properties().sampleAspectRatio;

  auto newWidth  = childSize.width();
  auto newHeight = newWidth * childSAR.num / childSAR.den;

  this->scaledSize = QSize(newWidth, newHeight);
  this->video.setScaledSize(this->scaledSize);
}
