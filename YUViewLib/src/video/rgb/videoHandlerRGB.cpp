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

#include "videoHandlerRGB.h"

#include <common/FileInfo.h>
#include <common/Formatting.h>
#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <common/EnumMapper.h>
#include <video/rgb/ConversionRGB.h>
#include <video/rgb/PixelFormatRGBGuess.h>
#include <video/rgb/videoHandlerRGBCustomFormatDialog.h>

#include <QPainter>
#include <QtGlobal>

namespace video::rgb
{

namespace
{

constexpr EnumMapper<ComponentDisplayMode, 6>
    ComponentShowMapper(std::make_pair(ComponentDisplayMode::RGBA, "RGBA"sv),
                        std::make_pair(ComponentDisplayMode::RGB, "RGB"sv),
                        std::make_pair(ComponentDisplayMode::R, "R"sv),
                        std::make_pair(ComponentDisplayMode::G, "G"sv),
                        std::make_pair(ComponentDisplayMode::B, "B"sv),
                        std::make_pair(ComponentDisplayMode::A, "A"sv));

constexpr EnumMapper<ComponentDisplayMode, 6>
    ComponentShowMapperToDisplayText(std::make_pair(ComponentDisplayMode::RGBA, "RGBA"sv),
                                     std::make_pair(ComponentDisplayMode::RGB, "RGB"sv),
                                     std::make_pair(ComponentDisplayMode::R, "Red Only"sv),
                                     std::make_pair(ComponentDisplayMode::G, "Green Only"sv),
                                     std::make_pair(ComponentDisplayMode::B, "Blue Only"sv),
                                     std::make_pair(ComponentDisplayMode::A, "Alpha Only"sv));

} // namespace

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERRGB_DEBUG_LOADING 0
#if VIDEOHANDLERRGB_DEBUG_LOADING && !NDEBUG
#include <QDebug>
#define DEBUG_RGB qDebug
#else
#define DEBUG_RGB(fmt, ...) ((void)0)
#endif

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of
// the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#define restrict __restrict /* use implementation __ format */
#else
#ifndef __STDC_VERSION__
#define restrict __restrict /* use implementation __ format */
#else
#if __STDC_VERSION__ < 199901L
#define restrict __restrict /* use implementation __ format */
#else
#/* all ok */
#endif
#endif
#endif

std::vector<rgb::PixelFormatRGB> videoHandlerRGB::formatPresetList = {
    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB),
    PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::RGB),
    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB, AlphaMode::First),
    PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::BRG),
    PixelFormatRGB(10, DataLayout::Packed, ChannelOrder::BRG),
    PixelFormatRGB(10, DataLayout::Planar, ChannelOrder::RGB)};

videoHandlerRGB::videoHandlerRGB() : videoHandler()
{
  // preset internal values
  this->setSrcPixelFormat(PixelFormatRGB(8, DataLayout::Packed, ChannelOrder::RGB));
}

videoHandlerRGB::~videoHandlerRGB()
{
  // This will cause a "QMutex: destroying locked mutex" warning by Qt.
  // However, here this is on purpose.
  rgbFormatMutex.lock();
}

unsigned videoHandlerRGB::getCachingFrameSize() const
{
  auto hasAlpha = this->srcPixelFormat.hasAlpha();
  auto bytes    = functionsGui::bytesPerPixel(functionsGui::platformImageFormat(hasAlpha));
  return this->frameSize.width * this->frameSize.height * bytes;
}

QStringPairList videoHandlerRGB::getPixelValues(const QPoint &pixelPos,
                                                int           frameIdx,
                                                FrameHandler *item2,
                                                const int     frameIdx1)
{
  QStringPairList values;

  const int formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
  if (item2 != nullptr)
  {
    auto rgbItem2 = dynamic_cast<videoHandlerRGB *>(item2);
    if (rgbItem2 == nullptr)
      // The second item is not a videoHandlerRGB. Get the values from the FrameHandler.
      return FrameHandler::getPixelValues(pixelPos, frameIdx, item2, frameIdx1);

    if (currentFrameRawData_frameIndex != frameIdx ||
        rgbItem2->currentFrameRawData_frameIndex != frameIdx1)
      return {};

    auto width  = std::min(frameSize.width, rgbItem2->frameSize.width);
    auto height = std::min(frameSize.height, rgbItem2->frameSize.height);

    if (pixelPos.x() < 0 || pixelPos.x() >= int(width) || pixelPos.y() < 0 ||
        pixelPos.y() >= int(height))
      return {};

    rgba_t valueThis  = getPixelValue(pixelPos);
    rgba_t valueOther = rgbItem2->getPixelValue(pixelPos);

    const int     R       = int(valueThis.R) - int(valueOther.R);
    const int     G       = int(valueThis.G) - int(valueOther.G);
    const int     B       = int(valueThis.B) - int(valueOther.B);
    const QString RString = ((R < 0) ? "-" : "") + QString::number(std::abs(R), formatBase);
    const QString GString = ((G < 0) ? "-" : "") + QString::number(std::abs(G), formatBase);
    const QString BString = ((B < 0) ? "-" : "") + QString::number(std::abs(B), formatBase);

    values.append(QStringPair("R", RString));
    values.append(QStringPair("G", GString));
    values.append(QStringPair("B", BString));
    if (srcPixelFormat.hasAlpha())
    {
      const int     A       = int(valueThis.A) - int(valueOther.A);
      const QString AString = ((A < 0) ? "-" : "") + QString::number(std::abs(A), formatBase);
      values.append(QStringPair("A", AString));
    }
  }
  else
  {
    int width  = frameSize.width;
    int height = frameSize.height;

    if (currentFrameRawData_frameIndex != frameIdx)
      return QStringPairList();

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return QStringPairList();

    rgba_t value = getPixelValue(pixelPos);

    values.append(QStringPair("R", QString::number(value.R, formatBase)));
    values.append(QStringPair("G", QString::number(value.G, formatBase)));
    values.append(QStringPair("B", QString::number(value.B, formatBase)));
    if (srcPixelFormat.hasAlpha())
      values.append(QStringPair("A", QString::number(value.A, formatBase)));
  }

  return values;
}

void videoHandlerRGB::setFormatFromCorrelation(const QByteArray &, int64_t)
{ /* TODO */
}

bool videoHandlerRGB::setFormatFromString(QString format)
{
  DEBUG_RGB("videoHandlerRGB::setFormatFromString " << format << "\n");

  auto split = format.split(";");
  if (split.length() != 4 || split[2] != "RGB")
    return false;

  if (!FrameHandler::setFormatFromString(split[0] + ";" + split[1]))
    return false;

  auto fmt = PixelFormatRGB(split[3].toStdString());
  if (!fmt.isValid())
    return false;

  this->setRGBPixelFormat(fmt, false);
  return true;
}

QLayout *videoHandlerRGB::createVideoHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  QVBoxLayout *newVBoxLayout = nullptr;
  if (!isSizeFixed)
  {
    // Our parent (FrameHandler) also has controls to add. Create a new vBoxLayout and append the
    // parent controls and our controls into that layout, separated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout(FrameHandler::createFrameHandlerControls(isSizeFixed));

    QFrame *line = new QFrame;
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }

  ui.setupUi();

  for (const auto &format : videoHandlerRGB::formatPresetList)
    ui.rgbFormatComboBox->addItem(QString::fromStdString(format.getName()));

  const auto currentFormatInPresetList =
      vectorContains(videoHandlerRGB::formatPresetList, this->srcPixelFormat);
  if (!currentFormatInPresetList && this->srcPixelFormat.isValid())
  {
    videoHandlerRGB::formatPresetList.push_back(this->srcPixelFormat);
    ui.rgbFormatComboBox->addItem(QString::fromStdString(this->srcPixelFormat.getName()));
  }
  ui.rgbFormatComboBox->addItem("Custom...");
  ui.rgbFormatComboBox->setEnabled(!isSizeFixed);

  if (const auto presetIndex =
          vectorIndexOf(videoHandlerRGB::formatPresetList, this->srcPixelFormat))
    ui.rgbFormatComboBox->setCurrentIndex(static_cast<int>(*presetIndex));

  ui.RScaleSpinBox->setValue(componentScale[0]);
  ui.RScaleSpinBox->setMaximum(1000);
  ui.GScaleSpinBox->setValue(componentScale[1]);
  ui.GScaleSpinBox->setMaximum(1000);
  ui.BScaleSpinBox->setValue(componentScale[2]);
  ui.BScaleSpinBox->setMaximum(1000);
  ui.AScaleSpinBox->setValue(componentScale[3]);
  ui.AScaleSpinBox->setMaximum(1000);

  ui.RInvertCheckBox->setChecked(this->componentInvert[0]);
  ui.GInvertCheckBox->setChecked(this->componentInvert[1]);
  ui.BInvertCheckBox->setChecked(this->componentInvert[2]);
  ui.AInvertCheckBox->setChecked(this->componentInvert[3]);

  ui.limitedRangeCheckBox->setChecked(this->limitedRange);

  connect(ui.rgbFormatComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &videoHandlerRGB::slotRGBFormatControlChanged);
  connect(ui.colorComponentsComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &videoHandlerRGB::slotDisplayOptionsChanged);
  for (auto spinBox : {ui.RScaleSpinBox, ui.GScaleSpinBox, ui.BScaleSpinBox, ui.AScaleSpinBox})
    connect(spinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &videoHandlerRGB::slotDisplayOptionsChanged);
  for (auto checkBox : {ui.RInvertCheckBox,
                        ui.GInvertCheckBox,
                        ui.BInvertCheckBox,
                        ui.AInvertCheckBox,
                        ui.limitedRangeCheckBox})
    connect(checkBox, &QCheckBox::stateChanged, this, &videoHandlerRGB::slotDisplayOptionsChanged);

  this->updateControlsForNewPixelFormat();

  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui.topVerticalLayout);

  if (isSizeFixed)
    return ui.topVerticalLayout;
  else
    return newVBoxLayout;
}

void videoHandlerRGB::slotDisplayOptionsChanged()
{
  {
    const auto index = ui.colorComponentsComboBox->currentIndex();
    if (index >= 0 && static_cast<std::size_t>(index) < ComponentShowMapper.size())
      if (const auto mode = ComponentShowMapper.at(static_cast<std::size_t>(index)))
        this->componentDisplayMode = *mode;
  }

  componentScale[0]  = ui.RScaleSpinBox->value();
  componentScale[1]  = ui.GScaleSpinBox->value();
  componentScale[2]  = ui.BScaleSpinBox->value();
  componentScale[3]  = ui.AScaleSpinBox->value();
  componentInvert[0] = ui.RInvertCheckBox->isChecked();
  componentInvert[1] = ui.GInvertCheckBox->isChecked();
  componentInvert[2] = ui.BInvertCheckBox->isChecked();
  componentInvert[3] = ui.AInvertCheckBox->isChecked();
  limitedRange       = ui.limitedRangeCheckBox->isChecked();

  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  currentImageIndex = -1;
  setCacheInvalid();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerRGB::updateControlsForNewPixelFormat()
{
  if (!ui.created())
    return;

  auto valid         = this->srcPixelFormat.isValid();
  auto hasAlpha      = this->srcPixelFormat.hasAlpha();
  auto validAndAlpha = valid & hasAlpha;

  ui.RScaleSpinBox->setEnabled(valid);
  ui.GScaleSpinBox->setEnabled(valid);
  ui.BScaleSpinBox->setEnabled(valid);
  ui.AScaleSpinBox->setEnabled(validAndAlpha);
  ui.RInvertCheckBox->setEnabled(valid);
  ui.GInvertCheckBox->setEnabled(valid);
  ui.BInvertCheckBox->setEnabled(valid);
  ui.AInvertCheckBox->setEnabled(validAndAlpha);

  QSignalBlocker block(ui.colorComponentsComboBox);
  ui.colorComponentsComboBox->setEnabled(valid);
  ui.colorComponentsComboBox->clear();
  if (valid)
  {
    std::vector<ComponentDisplayMode> listItems;
    if (hasAlpha)
      listItems = {ComponentDisplayMode::RGBA,
                   ComponentDisplayMode::RGB,
                   ComponentDisplayMode::R,
                   ComponentDisplayMode::G,
                   ComponentDisplayMode::B,
                   ComponentDisplayMode::A};
    else
      listItems = {ComponentDisplayMode::RGB,
                   ComponentDisplayMode::R,
                   ComponentDisplayMode::G,
                   ComponentDisplayMode::B};

    if (!hasAlpha && (this->componentDisplayMode == ComponentDisplayMode::A ||
                      this->componentDisplayMode == ComponentDisplayMode::RGBA))
      this->componentDisplayMode = ComponentDisplayMode::RGB;
    if (hasAlpha && this->componentDisplayMode == ComponentDisplayMode::RGB)
      this->componentDisplayMode = ComponentDisplayMode::RGBA;

    ui.colorComponentsComboBox->addItems(
        functions::toQStringList(ComponentShowMapperToDisplayText.getNames()));
    ui.colorComponentsComboBox->setCurrentText(QString::fromStdString(
        std::string(ComponentShowMapperToDisplayText.getName(this->componentDisplayMode))));
  }
}

void videoHandlerRGB::slotRGBFormatControlChanged(int selectionIndex)
{
  const auto nrBytesOldFormat = getBytesPerFrame();

  const auto customFormatSelected = (selectionIndex == videoHandlerRGB::formatPresetList.size());
  if (customFormatSelected)
  {
    DEBUG_RGB("videoHandlerRGB::slotRGBFormatControlChanged custom format");

    videoHandlerRGBCustomFormatDialog dialog(this->srcPixelFormat);
    if (dialog.exec() == QDialog::Accepted && dialog.getSelectedRGBFormat().isValid())
      this->srcPixelFormat = dialog.getSelectedRGBFormat();

    const auto isInPresetList =
        vectorContains(videoHandlerRGB::formatPresetList, this->srcPixelFormat);
    if (!isInPresetList && this->srcPixelFormat.isValid())
    {
      videoHandlerRGB::formatPresetList.push_back(this->srcPixelFormat);
      const QSignalBlocker blocker(this->ui.rgbFormatComboBox);
      const auto           insertPositionBeforeCustom = (this->ui.rgbFormatComboBox->count() - 1);
      ui.rgbFormatComboBox->insertItem(insertPositionBeforeCustom,
                                       QString::fromStdString(this->srcPixelFormat.getName()));
    }

    if (const auto presetIndex =
            vectorIndexOf(videoHandlerRGB::formatPresetList, this->srcPixelFormat))
    {
      const QSignalBlocker blocker(this->ui.rgbFormatComboBox);
      selectionIndex = static_cast<int>(*presetIndex);
      ui.rgbFormatComboBox->setCurrentIndex(selectionIndex);
    }
  }

  this->setSrcPixelFormat(videoHandlerRGB::formatPresetList.at(selectionIndex));

  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  this->currentImageIndex = -1;
  if (nrBytesOldFormat != getBytesPerFrame())
  {
    DEBUG_RGB("videoHandlerRGB::slotRGBFormatControlChanged nr bytes per frame changed");
    this->invalidateAllBuffers();
  }
  this->setCacheInvalid();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerRGB::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
  DEBUG_RGB("videoHandlerRGB::loadFrame %d", frameIndex);

  if (!isFormatValid())
  {
    DEBUG_RGB("videoHandlerRGB::loadFrame invalid pixel format");
    return;
  }

  // Does the data in currentFrameRawData need to be updated?
  if (!loadRawRGBData(frameIndex) || currentFrameRawData.isEmpty())
  {
    DEBUG_RGB("videoHandlerRGB::loadFrame Loading failed or is still running in the background");
    return;
  }

  // The data in currentFrameRawData is now up to date. If necessary
  // convert the data to RGB.
  if (loadToDoubleBuffer)
  {
    QImage newImage;
    convertRGBToImage(currentFrameRawData, newImage);
    doubleBufferImage           = newImage;
    doubleBufferImageFrameIndex = frameIndex;
  }
  else if (currentImageIndex != frameIndex)
  {
    QImage newImage;
    convertRGBToImage(currentFrameRawData, newImage);
    QMutexLocker writeLock(&currentImageSetMutex);
    currentImage      = newImage;
    currentImageIndex = frameIndex;
  }
}

void videoHandlerRGB::savePlaylist(YUViewDomElement &element) const
{
  FrameHandler::savePlaylist(element);
  element.appendProperiteChild("pixelFormat", this->getRawRGBPixelFormatName());

  element.appendProperiteChild("componentShow",
                               ComponentShowMapper.getName(this->componentDisplayMode));

  element.appendProperiteChild("scale.R", QString::number(this->componentScale[0]));
  element.appendProperiteChild("scale.G", QString::number(this->componentScale[1]));
  element.appendProperiteChild("scale.B", QString::number(this->componentScale[2]));
  element.appendProperiteChild("scale.A", QString::number(this->componentScale[3]));

  element.appendProperiteChild("invert.R", to_string(this->componentInvert[0]));
  element.appendProperiteChild("invert.G", to_string(this->componentInvert[1]));
  element.appendProperiteChild("invert.B", to_string(this->componentInvert[2]));
  element.appendProperiteChild("invert.A", to_string(this->componentInvert[3]));

  element.appendProperiteChild("limitedRange", to_string(this->limitedRange));
}

void videoHandlerRGB::loadPlaylist(const YUViewDomElement &element)
{
  FrameHandler::loadPlaylist(element);
  QString sourcePixelFormat = element.findChildValue("pixelFormat");
  this->setRGBPixelFormatByName(sourcePixelFormat);

  auto showVal = element.findChildValue("componentShow");
  if (const auto c = ComponentShowMapper.getValue(showVal.toStdString()))
    this->componentDisplayMode = *c;

  auto scaleR = element.findChildValue("scale.R");
  if (!scaleR.isEmpty())
    this->componentScale[0] = scaleR.toInt();
  auto scaleG = element.findChildValue("scale.G");
  if (!scaleG.isEmpty())
    this->componentScale[1] = scaleG.toInt();
  auto scaleB = element.findChildValue("scale.B");
  if (!scaleB.isEmpty())
    this->componentScale[2] = scaleB.toInt();
  auto scaleA = element.findChildValue("scale.A");
  if (!scaleA.isEmpty())
    this->componentScale[3] = scaleA.toInt();

  this->componentInvert[0] = (element.findChildValue("invert.R") == "True");
  this->componentInvert[1] = (element.findChildValue("invert.G") == "True");
  this->componentInvert[2] = (element.findChildValue("invert.B") == "True");
  this->componentInvert[3] = (element.findChildValue("invert.A") == "True");

  this->limitedRange = (element.findChildValue("limitedRange") == "True");
}

void videoHandlerRGB::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
  DEBUG_RGB("videoHandlerRGB::loadFrameForCaching %d", frameIndex);

  // Lock the mutex for the rgbFormat. The main thread has to wait until caching is done
  // before the RGB format can change.
  rgbFormatMutex.lock();

  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, true);
  tmpBufferRawRGBDataCaching = rawData;
  requestDataMutex.unlock();

  if (frameIndex != rawData_frameIndex)
  {
    // Loading failed
    currentImageIndex = -1;
    rgbFormatMutex.unlock();
    return;
  }

  // Convert RGB to image. This can then be cached.
  convertRGBToImage(tmpBufferRawRGBDataCaching, frameToCache);

  rgbFormatMutex.unlock();
}

// Load the raw RGB data for the given frame index into currentFrameRawData.
bool videoHandlerRGB::loadRawRGBData(int frameIndex)
{
  DEBUG_RGB("videoHandlerRGB::loadRawRGBData frame %d", frameIndex);

  if (currentFrameRawData_frameIndex == frameIndex && cacheValid)
  {
    DEBUG_RGB("videoHandlerRGB::loadRawRGBData frame %d already in the current buffer - Done",
              frameIndex);
    return true;
  }

  if (frameIndex == rawData_frameIndex)
  {
    // The raw data was loaded in the background. Now we just have to move it to the current
    // buffer. No actual loading is needed.
    requestDataMutex.lock();
    currentFrameRawData            = rawData;
    currentFrameRawData_frameIndex = frameIndex;
    requestDataMutex.unlock();
    return true;
  }

  DEBUG_RGB("videoHandlerRGB::loadRawRGBData %d", frameIndex);

  // The function loadFrameForCaching also uses the signalRequestRawData to request raw data.
  // However, only one thread can use this at a time.
  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, false);
  if (frameIndex == rawData_frameIndex)
  {
    currentFrameRawData            = rawData;
    currentFrameRawData_frameIndex = frameIndex;
  }
  requestDataMutex.unlock();

  DEBUG_RGB("videoHandlerRGB::loadRawRGBData %d %s",
            frameIndex,
            (frameIndex == rawData_frameIndex) ? "NewDataSet" : "Waiting...");
  return (currentFrameRawData_frameIndex == frameIndex);
}

// Convert the given raw RGB data in sourceBuffer (using srcPixelFormat) to image (RGB-888), using
// the buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerRGB::convertRGBToImage(const QByteArray &sourceBuffer, QImage &outputImage)
{
  DEBUG_RGB("videoHandlerRGB::convertRGBToImage");
  auto curFrameSize = QSize(this->frameSize.width, this->frameSize.height);

  // Create the output image in the right format.
  // Check if we have to premultiply alpha. The format of the raw buffer is: BGRA
  // (each 8 bit). Internally, this is how QImage allocates the number of bytes per line (with depth
  // = 32): const int bytes_per_line = ((width * depth + 31) >> 5) << 2; // bytes per scanline (must
  // be multiple of 4)
  const auto hasAlpha = this->srcPixelFormat.hasAlpha();
  const auto format   = functionsGui::platformImageFormat(hasAlpha);

  if (format != QImage::Format_RGB32 && format != QImage::Format_ARGB32 &&
      format != QImage::Format_ARGB32_Premultiplied)
  {
    DEBUG_RGB("Unsupported format. Can only convert to RGB32 formats.");
    return;
  }

  const auto bps = this->srcPixelFormat.getBitsPerSample();
  if (bps < 8 || bps > 16)
  {
    DEBUG_RGB("Unsupported bit depth. 8-16 bit are supported.");
    return;
  }

  outputImage = QImage(curFrameSize, format);

  // Check the image buffer size before we write to it
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
  assert(functions::clipToUnsigned(outputImage.byteCount()) >=
         frameSize.width * frameSize.height * 4);
#else
  assert(functions::clipToUnsigned(outputImage.sizeInBytes()) >=
         frameSize.width * frameSize.height * 4);
#endif

  this->convertSourceToRGBA32Bit(sourceBuffer, outputImage.bits(), format);
}

void videoHandlerRGB::setSrcPixelFormat(const PixelFormatRGB &newFormat)
{
  this->rgbFormatMutex.lock();
  this->srcPixelFormat = newFormat;
  this->updateControlsForNewPixelFormat();
  this->rgbFormatMutex.unlock();
}

// Convert the data in "sourceBuffer" from the format "srcPixelFormat" to RGB 888. While doing so,
// apply the scaling factors, inversions and only convert the selected color components.
void videoHandlerRGB::convertSourceToRGBA32Bit(const QByteArray &sourceBuffer,
                                               unsigned char    *targetBuffer,
                                               QImage::Format    imageFormat)
{
  Q_ASSERT_X(sourceBuffer.size() >= getBytesPerFrame(),
             Q_FUNC_INFO,
             "The source buffer does not hold enough data.");

  const auto outputSupportsAlpha =
      imageFormat == QImage::Format_ARGB32 || imageFormat == QImage::Format_ARGB32_Premultiplied;
  const auto premultiplyAlpha = imageFormat == QImage::Format_ARGB32_Premultiplied;
  const auto inputHasAlpha    = srcPixelFormat.hasAlpha();

  if (this->componentDisplayMode == ComponentDisplayMode::RGB ||
      this->componentDisplayMode == ComponentDisplayMode::RGBA)
  {
    const auto convertAlpha = this->componentDisplayMode == ComponentDisplayMode::RGBA &&
                              outputSupportsAlpha && inputHasAlpha;

    convertInputRGBToARGB(sourceBuffer,
                          this->srcPixelFormat,
                          targetBuffer,
                          this->frameSize,
                          this->componentInvert,
                          this->componentScale,
                          this->limitedRange,
                          convertAlpha,
                          premultiplyAlpha);
  }
  else // Single component
  {
    auto displayIndexMap = std::map<ComponentDisplayMode, unsigned>({{ComponentDisplayMode::R, 0},
                                                                     {ComponentDisplayMode::G, 1},
                                                                     {ComponentDisplayMode::B, 2},
                                                                     {ComponentDisplayMode::A, 3}});
    const auto displayIndex = displayIndexMap[this->componentDisplayMode];

    const auto scale  = this->componentScale[displayIndex];
    const auto invert = this->componentInvert[displayIndex];

    auto componentToChannel = std::map<ComponentDisplayMode, rgb::Channel>(
        {{ComponentDisplayMode::R, rgb::Channel::Red},
         {ComponentDisplayMode::G, rgb::Channel::Green},
         {ComponentDisplayMode::B, rgb::Channel::Blue},
         {ComponentDisplayMode::A, rgb::Channel::Alpha}});
    const auto displayChannel = componentToChannel[this->componentDisplayMode];

    convertSinglePlaneOfRGBToGreyscaleARGB(sourceBuffer,
                                           this->srcPixelFormat,
                                           targetBuffer,
                                           this->frameSize,
                                           displayChannel,
                                           scale,
                                           invert,
                                           this->limitedRange);
  }
}

rgba_t videoHandlerRGB::getPixelValue(const QPoint &pixelPos) const
{
  return getPixelValueFromBuffer(
      this->currentFrameRawData, this->srcPixelFormat, this->frameSize, pixelPos);
}

void videoHandlerRGB::setFormatFromSizeAndName(
    const Size frameSize, int, DataLayout, int64_t fileSize, const QFileInfo &fileInfo)
{
  this->setSrcPixelFormat(guessFormatFromSizeAndName(fileInfo, frameSize, fileSize));
}

void videoHandlerRGB::drawPixelValues(QPainter     *painter,
                                      const int     frameIdx,
                                      const QRect  &videoRect,
                                      const double  zoomFactor,
                                      FrameHandler *item2,
                                      const bool    markDifference,
                                      const int     frameIdxItem1)
{
  // First determine which pixels from this item are actually visible, because we only have to draw
  // the pixel values of the pixels that are actually visible
  auto viewport       = painter->viewport();
  auto worldTransform = painter->worldTransform();

  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

  // functions::clip the min/max visible pixel values to the size of the item (no pixels outside of
  // the item have to be labeled)
  xMin = functions::clip(xMin, 0, int(frameSize.width) - 1);
  yMin = functions::clip(yMin, 0, int(frameSize.height) - 1);
  xMax = functions::clip(xMax, 0, int(frameSize.width) - 1);
  yMax = functions::clip(yMax, 0, int(frameSize.height) - 1);

  // Get the other RGB item (if any)
  auto rgbItem2 = dynamic_cast<videoHandlerRGB *>(item2);
  if (item2 != nullptr && rgbItem2 == nullptr)
  {
    // The second item is not a videoHandlerRGB item
    FrameHandler::drawPixelValues(
        painter, frameIdx, videoRect, zoomFactor, item2, markDifference, frameIdxItem1);
    return;
  }

  // Check if the raw RGB values are up to date. If not, do not draw them. Do not trigger loading of
  // data here. The needsLoadingRawValues function will return that loading is needed. The caching
  // in the background should then trigger loading of them.
  if (currentFrameRawData_frameIndex != frameIdx)
    return;
  if (rgbItem2 && rgbItem2->currentFrameRawData_frameIndex != frameIdxItem1)
    return;

  // The center point of the pixel (0,0).
  auto centerPointZero = (QPoint(-(int(frameSize.width)), -(int(frameSize.height))) * zoomFactor +
                          QPoint(zoomFactor, zoomFactor)) /
                         2;
  // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize(QSize(zoomFactor, zoomFactor));
  const unsigned drawWhitLevel = 1 << (srcPixelFormat.getBitsPerSample() - 1);
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor))
      // and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);

      // Get the text to show
      QString   valText;
      const int formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
      if (rgbItem2 != nullptr)
      {
        rgba_t valueThis  = getPixelValue(QPoint(x, y));
        rgba_t valueOther = rgbItem2->getPixelValue(QPoint(x, y));

        const int     R       = int(valueThis.R) - int(valueOther.R);
        const int     G       = int(valueThis.G) - int(valueOther.G);
        const int     B       = int(valueThis.B) - int(valueOther.B);
        const int     A       = int(valueThis.A) - int(valueOther.A);
        const QString RString = ((R < 0) ? "-" : "") + QString::number(std::abs(R), formatBase);
        const QString GString = ((G < 0) ? "-" : "") + QString::number(std::abs(G), formatBase);
        const QString BString = ((B < 0) ? "-" : "") + QString::number(std::abs(B), formatBase);

        if (markDifference)
          painter->setPen((R == 0 && G == 0 && B == 0 && (!srcPixelFormat.hasAlpha() || A == 0))
                              ? Qt::white
                              : Qt::black);
        else
          painter->setPen((R < 0 && G < 0 && B < 0) ? Qt::white : Qt::black);

        if (srcPixelFormat.hasAlpha())
        {
          const QString AString = ((A < 0) ? "-" : "") + QString::number(std::abs(A), formatBase);
          valText = QString("R%1\nG%2\nB%3\nA%4").arg(RString, GString, BString, AString);
        }
        else
          valText = QString("R%1\nG%2\nB%3").arg(RString, GString, BString);
      }
      else
      {
        rgba_t value = getPixelValue(QPoint(x, y));
        if (srcPixelFormat.hasAlpha())
          valText = QString("R%1\nG%2\nB%3\nA%4")
                        .arg(value.R, 0, formatBase)
                        .arg(value.G, 0, formatBase)
                        .arg(value.B, 0, formatBase)
                        .arg(value.A, 0, formatBase);
        else
          valText = QString("R%1\nG%2\nB%3")
                        .arg(value.R, 0, formatBase)
                        .arg(value.G, 0, formatBase)
                        .arg(value.B, 0, formatBase);
        painter->setPen(
            (value.R < drawWhitLevel && value.G < drawWhitLevel && value.B < drawWhitLevel)
                ? Qt::white
                : Qt::black);
      }

      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QImage videoHandlerRGB::calculateDifference(FrameHandler    *item2,
                                            const int        frameIdxItem0,
                                            const int        frameIdxItem1,
                                            QList<InfoItem> &differenceInfoList,
                                            const int        amplificationFactor,
                                            const bool       markDifference)
{
  videoHandlerRGB *rgbItem2 = dynamic_cast<videoHandlerRGB *>(item2);
  if (rgbItem2 == nullptr)
    // The given item is not a RGB source. We cannot compare raw RGB values to non raw RGB values.
    // Call the base class comparison function to compare the items using the RGB 888 values.
    return videoHandler::calculateDifference(item2,
                                             frameIdxItem0,
                                             frameIdxItem1,
                                             differenceInfoList,
                                             amplificationFactor,
                                             markDifference);

  if (srcPixelFormat.getBitsPerSample() != rgbItem2->srcPixelFormat.getBitsPerSample())
    // The two items have different bit depths. Compare RGB 888 values instead.
    return videoHandler::calculateDifference(item2,
                                             frameIdxItem0,
                                             frameIdxItem1,
                                             differenceInfoList,
                                             amplificationFactor,
                                             markDifference);

  const int width  = std::min(frameSize.width, rgbItem2->frameSize.width);
  const int height = std::min(frameSize.height, rgbItem2->frameSize.height);

  // Load the right raw RGB data (if not already loaded).
  // This will just update the raw RGB data. No conversion to image (RGB) is performed. This is
  // either done on request if the frame is actually shown or has already been done by the caching
  // process.
  if (!loadRawRGBData(frameIdxItem0))
    return QImage(); // Loading failed
  if (!rgbItem2->loadRawRGBData(frameIdxItem1))
    return QImage(); // Loading failed

  // Also calculate the MSE while we're at it (R,G,B)
  int64_t mseAdd[3] = {0, 0, 0};

  // Create the output image in the right format
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA
  // (each 8 bit).
  auto qFrameSize = QSize(this->frameSize.width, this->frameSize.height);
  auto outputImage =
      QImage(qFrameSize, functionsGui::platformImageFormat(this->srcPixelFormat.hasAlpha()));

  // We directly write the difference values into the QImage buffer in the right format (ABGR).
  unsigned char *restrict dst = outputImage.bits();

  const auto bitDepth = srcPixelFormat.getBitsPerSample();
  const auto posR     = srcPixelFormat.getChannelPosition(Channel::Red);
  const auto posG     = srcPixelFormat.getChannelPosition(Channel::Green);
  const auto posB     = srcPixelFormat.getChannelPosition(Channel::Blue);

  if (bitDepth >= 8 && bitDepth <= 16)
  {
    // How many values do we have to skip in src to get to the next input value?
    // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
    int offsetToNextValue = srcPixelFormat.nrChannels();
    if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      offsetToNextValue = 1;

    if (bitDepth > 8 && bitDepth <= 16)
    {
      // 9 to 16 bits per component. We assume two bytes per value.
      // First get the pointer to the first value of each channel. (this item)
      unsigned short *srcR0, *srcG0, *srcB0;
      if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      {
        srcR0 = (unsigned short *)currentFrameRawData.data() +
                (posR * frameSize.width * frameSize.height);
        srcG0 = (unsigned short *)currentFrameRawData.data() +
                (posG * frameSize.width * frameSize.height);
        srcB0 = (unsigned short *)currentFrameRawData.data() +
                (posB * frameSize.width * frameSize.height);
      }
      else
      {
        srcR0 = (unsigned short *)currentFrameRawData.data() + posR;
        srcG0 = (unsigned short *)currentFrameRawData.data() + posG;
        srcB0 = (unsigned short *)currentFrameRawData.data() + posB;
      }

      // Next get the pointer to the first value of each channel. (the other item)
      unsigned short *srcR1, *srcG1, *srcB1;
      if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      {
        srcR1 = (unsigned short *)rgbItem2->currentFrameRawData.data() +
                (posR * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
        srcG1 = (unsigned short *)rgbItem2->currentFrameRawData.data() +
                (posG * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
        srcB1 = (unsigned short *)rgbItem2->currentFrameRawData.data() +
                (posB * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
      }
      else
      {
        srcR1 = (unsigned short *)rgbItem2->currentFrameRawData.data() + posR;
        srcG1 = (unsigned short *)rgbItem2->currentFrameRawData.data() + posG;
        srcB1 = (unsigned short *)rgbItem2->currentFrameRawData.data() + posB;
      }

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          unsigned int offsetCoordinate = frameSize.width * y + x;

          unsigned int R0 = (unsigned int)(*(srcR0 + offsetToNextValue * offsetCoordinate));
          unsigned int G0 = (unsigned int)(*(srcG0 + offsetToNextValue * offsetCoordinate));
          unsigned int B0 = (unsigned int)(*(srcB0 + offsetToNextValue * offsetCoordinate));

          unsigned int R1 = (unsigned int)(*(srcR1 + offsetToNextValue * offsetCoordinate));
          unsigned int G1 = (unsigned int)(*(srcG1 + offsetToNextValue * offsetCoordinate));
          unsigned int B1 = (unsigned int)(*(srcB1 + offsetToNextValue * offsetCoordinate));

          int deltaR = R0 - R1;
          int deltaG = G0 - G1;
          int deltaB = B0 - B1;

          mseAdd[0] += deltaR * deltaR;
          mseAdd[1] += deltaG * deltaG;
          mseAdd[2] += deltaB * deltaB;

          if (markDifference)
          {
            // Just mark if there is a difference
            dst[0] = (deltaB == 0) ? 0 : 255;
            dst[1] = (deltaG == 0) ? 0 : 255;
            dst[2] = (deltaR == 0) ? 0 : 255;
          }
          else
          {
            // We want to see the difference
            dst[0] = functions::clip(128 + deltaB * amplificationFactor, 0, 255);
            dst[1] = functions::clip(128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = functions::clip(128 + deltaR * amplificationFactor, 0, 255);
          }
          dst[3] = 255;
          dst += 4;
        }
      }
    }
    else if (bitDepth == 8)
    {
      // First get the pointer to the first value of each channel. (this item)
      unsigned char *srcR0, *srcG0, *srcB0;
      if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      {
        srcR0 = (unsigned char *)currentFrameRawData.data() +
                (posR * frameSize.width * frameSize.height);
        srcG0 = (unsigned char *)currentFrameRawData.data() +
                (posG * frameSize.width * frameSize.height);
        srcB0 = (unsigned char *)currentFrameRawData.data() +
                (posB * frameSize.width * frameSize.height);
      }
      else
      {
        srcR0 = (unsigned char *)currentFrameRawData.data() + posR;
        srcG0 = (unsigned char *)currentFrameRawData.data() + posG;
        srcB0 = (unsigned char *)currentFrameRawData.data() + posB;
      }

      // First get the pointer to the first value of each channel. (other item)
      unsigned char *srcR1, *srcG1, *srcB1;
      if (srcPixelFormat.getDataLayout() == DataLayout::Planar)
      {
        srcR1 = (unsigned char *)rgbItem2->currentFrameRawData.data() +
                (posR * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
        srcG1 = (unsigned char *)rgbItem2->currentFrameRawData.data() +
                (posG * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
        srcB1 = (unsigned char *)rgbItem2->currentFrameRawData.data() +
                (posB * rgbItem2->frameSize.width * rgbItem2->frameSize.height);
      }
      else
      {
        srcR1 = (unsigned char *)rgbItem2->currentFrameRawData.data() + posR;
        srcG1 = (unsigned char *)rgbItem2->currentFrameRawData.data() + posG;
        srcB1 = (unsigned char *)rgbItem2->currentFrameRawData.data() + posB;
      }

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          unsigned int offsetCoordinate = frameSize.width * y + x;

          unsigned int R0 = (unsigned int)(*(srcR0 + offsetToNextValue * offsetCoordinate));
          unsigned int G0 = (unsigned int)(*(srcG0 + offsetToNextValue * offsetCoordinate));
          unsigned int B0 = (unsigned int)(*(srcB0 + offsetToNextValue * offsetCoordinate));

          unsigned int R1 = (unsigned int)(*(srcR1 + offsetToNextValue * offsetCoordinate));
          unsigned int G1 = (unsigned int)(*(srcG1 + offsetToNextValue * offsetCoordinate));
          unsigned int B1 = (unsigned int)(*(srcB1 + offsetToNextValue * offsetCoordinate));

          int deltaR = R0 - R1;
          int deltaG = G0 - G1;
          int deltaB = B0 - B1;

          mseAdd[0] += deltaR * deltaR;
          mseAdd[1] += deltaG * deltaG;
          mseAdd[2] += deltaB * deltaB;

          if (markDifference)
          {
            // Just mark if there is a difference
            dst[0] = (deltaB == 0) ? 0 : 255;
            dst[1] = (deltaG == 0) ? 0 : 255;
            dst[2] = (deltaR == 0) ? 0 : 255;
          }
          else
          {
            // We want to see the difference
            dst[0] = functions::clip(128 + deltaB * amplificationFactor, 0, 255);
            dst[1] = functions::clip(128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = functions::clip(128 + deltaR * amplificationFactor, 0, 255);
          }
          dst[3] = 255;
          dst += 4;
        }
      }
    }
    else
      Q_ASSERT_X(
          false, Q_FUNC_INFO, "No RGB format with less than 8 or more than 16 bits supported yet.");
  }

  // Append the conversion information that will be returned
  differenceInfoList.append(InfoItem("Difference Type", QString("RGB %1bit").arg(bitDepth)));
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append(InfoItem("MSE R", QString("%1").arg(mse[0])));
  differenceInfoList.append(InfoItem("MSE G", QString("%1").arg(mse[1])));
  differenceInfoList.append(InfoItem("MSE B", QString("%1").arg(mse[2])));
  differenceInfoList.append(InfoItem("MSE All", QString("%1").arg(mse[3])));

  return outputImage;
}

} // namespace video::rgb
