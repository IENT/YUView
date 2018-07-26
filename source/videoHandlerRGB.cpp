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

#include <QPainter>
#include "fileInfoWidget.h"

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERRGB_DEBUG_LOADING 0
#if VIDEOHANDLERRGB_DEBUG_LOADING && !NDEBUG
#include <QDebug>
#define DEBUG_RGB qDebug
#else
#define DEBUG_RGB(fmt,...) ((void)0)
#endif

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#    define restrict __restrict /* use implementation __ format */
#else
#    ifndef __STDC_VERSION__
#        define restrict __restrict /* use implementation __ format */
#    else
#        if __STDC_VERSION__ < 199901L
#            define restrict __restrict /* use implementation __ format */
#        else
#            /* all ok */
#        endif
#    endif
#endif

videoHandlerRGB_CustomFormatDialog::videoHandlerRGB_CustomFormatDialog(const QString &rgbFormat, int bitDepth, bool planar, bool alpha)
{
  setupUi(this);

  // Set the correct index
  rgbOrderComboBox->setCurrentIndex(0);  // Default is RGB
  for (int i = 0; i < rgbOrderComboBox->count(); i++)
  {
    if (rgbOrderComboBox->itemText(i).toLower() == rgbFormat.toLower())
    {
      rgbOrderComboBox->setCurrentIndex(i);
      break;
    }
  }

  if (bitDepth == 0)
    bitDepth = 8;
  bitDepthSpinBox->setValue(bitDepth);

  planarCheckBox->setChecked(planar);
  alphaChannelCheckBox->setChecked(alpha);
}

QString videoHandlerRGB::rgbPixelFormat::getName() const
{
  if (bitsPerValue == 0)
    return "Unknown Pixel Format";

  QString name = getRGBFormatString();
  if (alphaChannel)
    name.append("A");
  name.append(QString(" %1bit").arg(bitsPerValue));
  if (planar)
    name.append(" planar");

  return name;
}

void videoHandlerRGB::rgbPixelFormat::setFromName(const QString &name)
{
  if (name == "Unknown Pixel Format")
  {
    posR = 0;
    posG = 0;
    posB = 0;
    bitsPerValue = 0;
    planar = false;
    alphaChannel = false;
  }
  else
  {
    setRGBFormatFromString(name.left(3));
    alphaChannel = (name[3] == 'A');
    int bitIdx = name.indexOf("bit");
    bitsPerValue = name.mid(alphaChannel ? 5 : 4, (alphaChannel ? 5 : 4) - bitIdx).toInt();
    planar = name.contains("planar");
  }
}

QString videoHandlerRGB::rgbPixelFormat::getRGBFormatString() const
{
  QString name;
  for (int i = 0; i < 3; i++)
  {
    if (posR == i)
      name.append("R");
    if (posG == i)
      name.append("G");
    if (posB == i)
      name.append("B");
  }
  return name;
}

void videoHandlerRGB::rgbPixelFormat::setRGBFormatFromString(const QString &format)
{
  for (int i = 0; i < 3; i++)
  {
    if (format[i].toLower() == 'r')
      posR = i;
    else if (format[i].toLower() == 'g')
      posG = i;
    else if (format[i].toLower() == 'b')
      posB = i;
  }
}

/* The default constructor of the RGBFormatList will fill the list with all supported RGB file formats.
 * Don't forget to implement actual support for all of them in the conversion functions.
*/
videoHandlerRGB::RGBFormatList::RGBFormatList()
{
  append(rgbPixelFormat(8,  false, false, 0, 1, 2));  // RGB 8bit
  append(rgbPixelFormat(10, false, false, 0, 1, 2));  // RGB 10bit
  append(rgbPixelFormat(8,  false,  true, 0, 1, 2));  // RGBA 8bit
  append(rgbPixelFormat(8,  false, false, 1, 2, 0));  // BRG 8bit
  append(rgbPixelFormat(10, false, false, 1, 2, 0));  // BRG 10bit
  append(rgbPixelFormat(10, false, true , 0, 1, 2));  // RGB 10bit planar
}

/* Put all the names of the RGB formats into a list and return it
*/
QStringList videoHandlerRGB::RGBFormatList::getFormattedNames() const
{
  QStringList l;
  for (int i = 0; i < count(); i++)
  {
    l.append(at(i).getName());
  }
  return l;
}

videoHandlerRGB::rgbPixelFormat videoHandlerRGB::RGBFormatList::getFromName(const QString &name) const
{
  for (int i = 0; i < count(); i++)
  {
    if (at(i) == name)
      return at(i);
  }
  // If the format could not be found, we return the "Unknown Pixel Format" format
  return rgbPixelFormat();
}

// Initialize the static rgbPresetList
videoHandlerRGB::RGBFormatList videoHandlerRGB::rgbPresetList;

/* Get the number of bytes for a frame with this RGB format and the given size
*/
qint64 videoHandlerRGB::rgbPixelFormat::bytesPerFrame(const QSize &frameSize) const
{
  if (bitsPerValue == 0 || !frameSize.isValid())
    return 0;

  qint64 numSamples = frameSize.height() * frameSize.width();

  int channels = (alphaChannel) ? 4 : 3;
  return numSamples * channels * ((bitsPerValue + 7) / 8);
}

// --------------------- videoHandlerRGB ----------------------------------

videoHandlerRGB::videoHandlerRGB() : videoHandler()
{
  // preset internal values
  setSrcPixelFormat(rgbPixelFormat());
  componentDisplayMode = DisplayAll;

  componentScale[0] = 1;
  componentScale[1] = 1;
  componentScale[2] = 1;

  componentInvert[0] = false;
  componentInvert[1] = false;
  componentInvert[2] = false;

  currentFrameRawRGBData_frameIdx = -1;
  rawRGBData_frameIdx = -1;

  // Set the order of the RGB formats in the combo box
  orderRGBList << "RGB" << "RBG" << "GRB" << "GBR" << "BRG" << "BGR";
  bitDepthList << "8" << "10" << "12" << "16";
}

videoHandlerRGB::~videoHandlerRGB()
{
  // This will cause a "QMutex: destroying locked mutex" warning by Qt.
  // However, here this is on purpose.
  rgbFormatMutex.lock();
}

ValuePairList videoHandlerRGB::getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2, const int frameIdx1)
{
  ValuePairList values;
  
  if (item2 != nullptr)
  {
    videoHandlerRGB *rgbItem2 = dynamic_cast<videoHandlerRGB*>(item2);
    if (rgbItem2 == nullptr)
      // The second item is not a videoHandlerRGB. Get the values from the frameHandler.
      return frameHandler::getPixelValues(pixelPos, frameIdx, item2, frameIdx1);

    if (currentFrameRawRGBData_frameIdx != frameIdx || rgbItem2->currentFrameRawRGBData_frameIdx != frameIdx1)
      return ValuePairList();

    int width  = qMin(frameSize.width(), rgbItem2->frameSize.width());
    int height = qMin(frameSize.height(), rgbItem2->frameSize.height());

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int R0,G0,B0, R1, G1, B1;
    getPixelValue(pixelPos, R0, G0, B0);
    rgbItem2->getPixelValue(pixelPos, R1, G1, B1);

    values.append(ValuePair("R", QString::number((int)R0-(int)R1)));
    values.append(ValuePair("G", QString::number((int)G0-(int)G1)));
    values.append(ValuePair("B", QString::number((int)B0-(int)B1)));
  }
  else
  {
    int width = frameSize.width();
    int height = frameSize.height();

    if (currentFrameRawRGBData_frameIdx != frameIdx)
      return ValuePairList();

    if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
      return ValuePairList();

    unsigned int R,G,B;
    getPixelValue(pixelPos, R, G, B);

    values.append(ValuePair("R", QString::number(R)));
    values.append(ValuePair("G", QString::number(G)));
    values.append(ValuePair("B", QString::number(B)));
  }

  return values;
}

QLayout *videoHandlerRGB::createRGBVideoHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  QVBoxLayout *newVBoxLayout = nullptr;
  if (!isSizeFixed)
  {
    // Our parent (frameHandler) also has controls to add. Create a new vBoxLayout and append the parent controls
    // and our controls into that layout, separated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout(frameHandler::createFrameHandlerControls(isSizeFixed));

    QFrame *line = new QFrame;
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }

  ui.setupUi();

  // Set all the values of the properties widget to the values of this class
  ui.rgbFormatComboBox->addItems(rgbPresetList.getFormattedNames());
  ui.rgbFormatComboBox->addItem("Custom...");
  int idx = rgbPresetList.indexOf(srcPixelFormat);
  if (idx == -1)
    ui.rgbFormatComboBox->setCurrentText("Unknown pixel format");
  else if (idx > 0)
    ui.rgbFormatComboBox->setCurrentIndex(idx);
  else
    // Custom pixel format (but a known pixel format)
    ui.rgbFormatComboBox->setCurrentText(srcPixelFormat.getName());
  ui.rgbFormatComboBox->setEnabled(!isSizeFixed);

  ui.colorComponentsComboBox->addItems(QStringList() << "RGB" << "Red Only" << "Green only" << "Blue only");
  ui.colorComponentsComboBox->setCurrentIndex((int)componentDisplayMode);

  ui.RScaleSpinBox->setValue(componentScale[0]);
  ui.RScaleSpinBox->setMaximum(1000);
  ui.GScaleSpinBox->setValue(componentScale[1]);
  ui.GScaleSpinBox->setMaximum(1000);
  ui.BScaleSpinBox->setValue(componentScale[2]);
  ui.BScaleSpinBox->setMaximum(1000);

  // Connect all the change signals from the controls
  connect(ui.rgbFormatComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerRGB::slotRGBFormatControlChanged);
  connect(ui.colorComponentsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.RScaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.GScaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.BScaleSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.RInvertCheckBox, &QCheckBox::stateChanged, this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.GInvertCheckBox, &QCheckBox::stateChanged, this, &videoHandlerRGB::slotDisplayOptionsChanged);
  connect(ui.BInvertCheckBox, &QCheckBox::stateChanged, this, &videoHandlerRGB::slotDisplayOptionsChanged);

  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui.topVerticalLayout);

  if (isSizeFixed)
    return ui.topVerticalLayout;
  else
    return newVBoxLayout;
}

void videoHandlerRGB::slotDisplayOptionsChanged()
{
  componentDisplayMode = (ComponentDisplayMode)ui.colorComponentsComboBox->currentIndex();
  componentScale[0] = ui.RScaleSpinBox->value();
  componentScale[1] = ui.GScaleSpinBox->value();
  componentScale[2] = ui.BScaleSpinBox->value();
  componentInvert[0] = ui.RInvertCheckBox->isChecked();
  componentInvert[1] = ui.GInvertCheckBox->isChecked();
  componentInvert[2] = ui.BInvertCheckBox->isChecked();

  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  currentImageIdx = -1;
  setCacheInvalid();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerRGB::slotRGBFormatControlChanged()
{
  // What is the current selection?
  int idx = ui.rgbFormatComboBox->currentIndex();

  // The old format's number bytes per frame
  qint64 nrBytesOldFormat = getBytesPerFrame();

  if (idx == rgbPresetList.count())
  {
    // The user selected the "custom format..." option
    videoHandlerRGB_CustomFormatDialog dialog(srcPixelFormat.getRGBFormatString(), srcPixelFormat.bitsPerValue, srcPixelFormat.planar, srcPixelFormat.alphaChannel);
    if (dialog.exec() == QDialog::Accepted)
    {
      // Set the custom format
      srcPixelFormat.setRGBFormatFromString(dialog.getRGBFormat());
      srcPixelFormat.bitsPerValue = dialog.getBitDepth();
      srcPixelFormat.planar = dialog.getPlanar();
      srcPixelFormat.alphaChannel = dialog.getAlphaChannel();
    }

    // Check if the custom format it in the presets list. If not, add it
    int idx = rgbPresetList.indexOf(srcPixelFormat);
    if (idx == -1 && srcPixelFormat.isValid())
    {
      // Valid pixel format with is not in the list. Add it...
      rgbPresetList.append(srcPixelFormat);
      int nrItems = ui.rgbFormatComboBox->count();
      const QSignalBlocker blocker(ui.rgbFormatComboBox);
      ui.rgbFormatComboBox->insertItem(nrItems - 1, srcPixelFormat.getName());
      idx = rgbPresetList.indexOf(srcPixelFormat);
    }

    if (idx > 0)
    {
      // Format found. Set it without another call to this function.
      const QSignalBlocker blocker(ui.rgbFormatComboBox);
      ui.rgbFormatComboBox->setCurrentIndex(idx);
    }
  }
  else
  {
    // One of the preset formats was selected
    setSrcPixelFormat(rgbPresetList.at(idx));
  }

  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  currentImageIdx = -1;
  if (nrBytesOldFormat != getBytesPerFrame())
  {
    // The number of bytes per frame changed -> the number of frames in the sequence changed.
    emit signalUpdateFrameLimits();
    // The raw RGB data buffer also needs to be reloaded
    currentFrameRawRGBData_frameIdx = -1;
  }
  setCacheInvalid();
  emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerRGB::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
  DEBUG_RGB("videoHandlerRGB::loadFrame %d\n", frameIndex);

  if (!isFormatValid())
    // We cannot load a frame if the format is not known
    return;

  // Does the data in currentFrameRawRGBData need to be updated?
  if (!loadRawRGBData(frameIndex))
    // Loading failed or it is still being performed in the background
    return;

  // The data in currentFrameRawRGBData is now up to date. If necessary
  // convert the data to RGB.
  if (loadToDoubleBuffer)
  {
    QImage newImage;
    convertRGBToImage(currentFrameRawRGBData, newImage);
    doubleBufferImage = newImage;
    doubleBufferImageFrameIdx = frameIndex;
  }
  else if (currentImageIdx != frameIndex)
  {
    QImage newImage;
    convertRGBToImage(currentFrameRawRGBData, newImage);
    QMutexLocker writeLock(&currentImageSetMutex);    
    currentImage = newImage;
    currentImageIdx = frameIndex;
  }
}

void videoHandlerRGB::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
  DEBUG_RGB("videoHandlerRGB::loadFrameForCaching %d", frameIndex);

  // Lock the mutex for the rgbFormat. The main thread has to wait until caching is done
  // before the RGB format can change.
  rgbFormatMutex.lock();

  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, true);
  tmpBufferRawRGBDataCaching = rawRGBData;
  requestDataMutex.unlock();

  if (frameIndex != rawRGBData_frameIdx)
  {
    // Loading failed
    currentImageIdx = -1;
    rgbFormatMutex.unlock();
    return;
  }

  // Convert RGB to image. This can then be cached.
  convertRGBToImage(tmpBufferRawRGBDataCaching, frameToCache);

  rgbFormatMutex.unlock();
}

// Load the raw RGB data for the given frame index into currentFrameRawRGBData.
bool videoHandlerRGB::loadRawRGBData(int frameIndex)
{
  if (currentFrameRawRGBData_frameIdx == frameIndex)
    // Buffer already up to date
    return true;

  if (frameIndex == rawRGBData_frameIdx)
  {
    // The raw data was loaded in the background. Now we just have to move it to the current
    // buffer. No actual loading is needed.
    requestDataMutex.lock();
    currentFrameRawRGBData = rawRGBData;
    currentFrameRawRGBData_frameIdx = frameIndex;
    requestDataMutex.unlock();
    return true;
  }

  DEBUG_RGB("videoHandlerRGB::loadRawRGBData %d", frameIndex);

  // The function loadFrameForCaching also uses the signalRequestRawData to request raw data.
  // However, only one thread can use this at a time.
  requestDataMutex.lock();
  emit signalRequestRawData(frameIndex, false);
  if (frameIndex == rawRGBData_frameIdx)
  {
    currentFrameRawRGBData = rawRGBData;
    currentFrameRawRGBData_frameIdx = frameIndex;
  }
  requestDataMutex.unlock();

  DEBUG_RGB("videoHandlerRGB::loadRawRGBData %d %s", frameIndex, (frameIndex == rawRGBData_frameIdx) ? "NewDataSet" : "Waiting...");
  return (currentFrameRawRGBData_frameIdx == frameIndex);
}

// Convert the given raw RGB data in sourceBuffer (using srcPixelFormat) to image (RGB-888), using the
// buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerRGB::convertRGBToImage(const QByteArray &sourceBuffer, QImage &outputImage)
{
  DEBUG_RGB("videoHandlerRGB::convertRGBToImage");
  QSize curFrameSize = frameSize;

  // Create the output image in the right format.
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA (each 8 bit).
  // Internally, this is how QImage allocates the number of bytes per line (with depth = 32):
  // const int bytes_per_line = ((width * depth + 31) >> 5) << 2; // bytes per scanline (must be multiple of 4)
  if (is_Q_OS_WIN)
    outputImage = QImage(curFrameSize, QImage::Format_ARGB32_Premultiplied);
  else if (is_Q_OS_MAC)
    outputImage = QImage(curFrameSize, QImage::Format_RGB32);
  else if (is_Q_OS_LINUX)
  {
    QImage::Format f = platformImageFormat();
    if (f == QImage::Format_ARGB32_Premultiplied)
      outputImage = QImage(curFrameSize, QImage::Format_ARGB32_Premultiplied);
    if (f == QImage::Format_ARGB32)
      outputImage = QImage(curFrameSize, QImage::Format_ARGB32);
    else
      outputImage = QImage(curFrameSize, QImage::Format_RGB32);
  }

  // Check the image buffer size before we write to it
  assert(outputImage.byteCount() >= curFrameSize.width() * curFrameSize.height() * 4);

  convertSourceToRGBA32Bit(sourceBuffer, outputImage.bits());

  if (is_Q_OS_LINUX)
  {
    // On linux, we may have to convert the image to the platform image format if it is not one of the
    // RGBA formats.
    QImage::Format f = platformImageFormat();
    if (f != QImage::Format_ARGB32_Premultiplied && f != QImage::Format_ARGB32 && f != QImage::Format_RGB32)
      outputImage = outputImage.convertToFormat(f);
  }
}

// Convert the data in "sourceBuffer" from the format "srcPixelFormat" to RGB 888. While doing so, apply the
// scaling factors, inversions and only convert the selected color components.
void videoHandlerRGB::convertSourceToRGBA32Bit(const QByteArray &sourceBuffer, unsigned char *targetBuffer)
{
  // Check if the source buffer is of the correct size
  Q_ASSERT_X(sourceBuffer.size() >= getBytesPerFrame(), "videoHandlerRGB::convertSourceToRGB888", "The source buffer does not hold enough data.");

  // Get the raw data pointer to the output array
  unsigned char * restrict dst = targetBuffer;

  // How many values do we have to skip in src to get to the next input value?
  // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
  int offsetToNextValue = (srcPixelFormat.alphaChannel) ? 4 : 3;
  if (srcPixelFormat.planar)
    offsetToNextValue = 1;

  if (componentDisplayMode != DisplayAll)
  {
    // Only convert one of the components to a gray-scale image.
    // Consider inversion and scale of that component

    // Which component of the source do we need?
    int displayComponentOffset = srcPixelFormat.posR;
    if (componentDisplayMode == DisplayG)
      displayComponentOffset = srcPixelFormat.posG;
    else if (componentDisplayMode == DisplayG)
      displayComponentOffset = srcPixelFormat.posB;

    // Get the scale/inversion for the displayed component
    int displayIndex = (componentDisplayMode == DisplayR) ? 0 : (componentDisplayMode == DisplayG) ? 1 : 2;
    int scale = componentScale[displayIndex];
    bool invert = componentInvert[displayIndex];

    if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
    {
      // 9 to 16 bits per component. We assume two bytes per value.

      // The source values have to be shifted left by this many bits to get 8 bit output
      const int rightShift = srcPixelFormat.bitsPerValue - 8;

      // First get the pointer to the first value that we will need.
      unsigned short* src = (unsigned short*)sourceBuffer.data();
      if (srcPixelFormat.planar)
        src += displayComponentOffset * frameSize.width() * frameSize.height();
      else
        src += displayComponentOffset;

      // Now we just have to iterate over all values and always skip "offsetToNextValue" values in src and write 3 values in dst.
      for (int i = 0; i < frameSize.width() * frameSize.height(); i++)
      {
        int val = (((int)src[0]) * scale) >> rightShift;
        val = clip(val, 0, 255);
        if (invert)
          val = 255 - val;
        dst[0] = val;
        dst[1] = val;
        dst[2] = val;
        dst[3] = 255;

        src += offsetToNextValue;
        dst += 4;
      }
    }
    else if (srcPixelFormat.bitsPerValue == 8)
    {
      // First get the pointer to the first value that we will need.
      unsigned char* src = (unsigned char*)sourceBuffer.data();
      if (srcPixelFormat.planar)
        src += displayComponentOffset * frameSize.width() * frameSize.height();
      else
        src += displayComponentOffset;

      // Now we just have to iterate over all values and always skip "offsetToNextValue" values in src and write 3 values in dst.
      for (int i = 0; i < frameSize.width() * frameSize.height(); i++)
      {
        int val = ((int)src[0]) * scale;
        val = clip(val, 0, 255);
        if (invert)
          val = 255 - val;
        dst[0] = val;
        dst[1] = val;
        dst[2] = val;
        dst[3] = 255;

        src += offsetToNextValue;
        dst += 4;
      }
    }
    else
      Q_ASSERT_X(false, "videoHandlerRGB::convertSourceToRGB888", "No RGB format with less than 8 or more than 16 bits supported yet.");
  }
  else if (componentDisplayMode == DisplayAll)
  {
    // Convert all components from the source RGB format to an RGB 888 array

    if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
    {
      // 9 to 16 bits per component. We assume two bytes per value.

      // The source values have to be shifted left by this many bits to get 8 bit output
      const int rightShift = srcPixelFormat.bitsPerValue - 8;

      // First get the pointer to the first value of each channel.
      unsigned short *srcR, *srcG, *srcB;
      if (srcPixelFormat.planar)
      {
        srcR = (unsigned short*)sourceBuffer.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG = (unsigned short*)sourceBuffer.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB = (unsigned short*)sourceBuffer.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR = (unsigned short*)sourceBuffer.data() + srcPixelFormat.posR;
        srcG = (unsigned short*)sourceBuffer.data() + srcPixelFormat.posG;
        srcB = (unsigned short*)sourceBuffer.data() + srcPixelFormat.posB;
      }

      // Now we just have to iterate over all values and always skip "offsetToNextValue" values in the sources and write 3 values in dst.
      for (int i = 0; i < frameSize.width() * frameSize.height(); i++)
      {
        // R
        int valR = (((int)srcR[0]) * componentScale[0]) >> rightShift;
        valR = clip(valR, 0, 255);
        if (componentInvert[0])
          valR = 255 - valR;
        
        // G
        int valG = (((int)srcG[0]) * componentScale[1]) >> rightShift;
        valG = clip(valG, 0, 255);
        if (componentInvert[1])
          valG = 255 - valG;

        // B
        int valB = (((int)srcB[0]) * componentScale[2]) >> rightShift;
        valB = clip(valB, 0, 255);
        if (componentInvert[2])
          valB = 255 - valB;

        srcR += offsetToNextValue;
        srcG += offsetToNextValue;
        srcB += offsetToNextValue;

        dst[0] = valB;
        dst[1] = valG;
        dst[2] = valR;
        dst[3] = 255;
        dst += 4;
      }
    }
    else if (srcPixelFormat.bitsPerValue == 8)
    {
      // First get the pointer to the first value of each channel.
      unsigned char *srcR, *srcG, *srcB;
      if (srcPixelFormat.planar)
      {
        srcR = (unsigned char*)sourceBuffer.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG = (unsigned char*)sourceBuffer.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB = (unsigned char*)sourceBuffer.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR = (unsigned char*)sourceBuffer.data() + srcPixelFormat.posR;
        srcG = (unsigned char*)sourceBuffer.data() + srcPixelFormat.posG;
        srcB = (unsigned char*)sourceBuffer.data() + srcPixelFormat.posB;
      }

      // Now we just have to iterate over all values and always skip "offsetToNextValue" values in the sources and write 3 values in dst.
      for (int i = 0; i < frameSize.width() * frameSize.height(); i++)
      {
        // R
        int valR = ((int)srcR[0]) * componentScale[0];
        valR = clip(valR, 0, 255);
        if (componentInvert[0])
          valR = 255 - valR;

        // G
        int valG = ((int)srcG[0]) * componentScale[1];
        valG = clip(valG, 0, 255);
        if (componentInvert[1])
          valG = 255 - valG;

        // B
        int valB = ((int)srcB[0]) * componentScale[2];
        valB = clip(valB, 0, 255);
        if (componentInvert[2])
          valB = 255 - valB;

        srcR += offsetToNextValue;
        srcG += offsetToNextValue;
        srcB += offsetToNextValue;

        dst[0] = valB;
        dst[1] = valG;
        dst[2] = valR;
        dst[3] = 255;
        dst += 4;
      }
    }
    else
      Q_ASSERT_X(false, "videoHandlerRGB::convertSourceToRGB888", "No RGB format with less than 8 or more than 16 bits supported yet.");
  }
  else
    Q_ASSERT_X(false, "videoHandlerRGB::convertSourceToRGB888", "Unsupported display mode.");
}

void videoHandlerRGB::getPixelValue(const QPoint &pixelPos, unsigned int &R, unsigned int &G, unsigned int &B)
{
  const unsigned int offsetCoordinate = frameSize.width() * pixelPos.y() + pixelPos.x();

  // How many values do we have to skip in src to get to the next input value?
  // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
  int offsetToNextValue = (srcPixelFormat.alphaChannel) ? 4 : 3;
  if (srcPixelFormat.planar)
    offsetToNextValue = 1;

  if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
  {
    // First get the pointer to the first value of each channel.
    unsigned short *srcR, *srcG, *srcB;
    if (srcPixelFormat.planar)
    {
      srcR = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
      srcG = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
      srcB = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
    }
    else
    {
      srcR = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
      srcG = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
      srcB = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
    }

    R = (unsigned int)(*(srcR + offsetToNextValue * offsetCoordinate));
    G = (unsigned int)(*(srcG + offsetToNextValue * offsetCoordinate));
    B = (unsigned int)(*(srcB + offsetToNextValue * offsetCoordinate));
  }
  else if (srcPixelFormat.bitsPerValue == 8)
  {
    // First get the pointer to the first value of each channel.
    unsigned char *srcR, *srcG, *srcB;
    if (srcPixelFormat.planar)
    {
      srcR = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
      srcG = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
      srcB = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
    }
    else
    {
      srcR = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
      srcG = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
      srcB = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
    }

    R = (unsigned int)(*(srcR + offsetToNextValue * offsetCoordinate));
    G = (unsigned int)(*(srcG + offsetToNextValue * offsetCoordinate));
    B = (unsigned int)(*(srcB + offsetToNextValue * offsetCoordinate));
  }
  else
    Q_ASSERT_X(false, "videoHandlerRGB::getPixelValue", "No RGB format with less than 8 or more than 16 bits supported yet.");
}

void videoHandlerRGB::setFrameSize(const QSize &size)
{
  if (size != frameSize)
  {
    currentFrameRawRGBData_frameIdx = -1;
    currentImageIdx = -1;
  }

  videoHandler::setFrameSize(size);
}

void videoHandlerRGB::setFormatFromSizeAndName(const QSize size, int bitDepth, qint64 fileSize, const QFileInfo &fileInfo)
{
  // Get the file extension
  QString ext = fileInfo.suffix().toLower();
  QString subFormat = "rgb";
  if (ext == "rgb" || ext == "bgr" || ext == "gbr")
    subFormat = ext;

  // If the bit depth could not be determined, check 8 and 10 bit
  int testBitDepths = (bitDepth > 0) ? 1 : 2;

  for (int i = 0; i < testBitDepths; i++)
  {
    if (testBitDepths == 2)
      bitDepth = (i == 0) ? 8 : 10;

    if (bitDepth==8)
    {
      // assume RGB if subFormat does not indicate anything else
      rgbPixelFormat cFormat;
      cFormat.setRGBFormatFromString(subFormat);
      cFormat.bitsPerValue = 8;

      // Check if the file size and the assumed format match
      int bpf = cFormat.bytesPerFrame(size);
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        setFrameSize(size);
        setSrcPixelFormat(cFormat);
        return;
      }
    }
    else if (bitDepth==10)
    {
      // Assume 444 format if subFormat is set. Otherwise assume 420
      rgbPixelFormat cFormat;
      cFormat.setRGBFormatFromString(subFormat);
      cFormat.bitsPerValue = 10;

      // Check if the file size and the assumed format match
      int bpf = cFormat.bytesPerFrame(size);
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        setFrameSize(size);
        setSrcPixelFormat(cFormat);
        return;
      }
    }
    else
    {
        // do other stuff
    }
  }
}

void videoHandlerRGB::drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2, const bool markDifference, const int frameIdxItem1)
{
  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();

  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  xMin = clip(xMin, 0, frameSize.width()-1);
  yMin = clip(yMin, 0, frameSize.height()-1);
  xMax = clip(xMax, 0, frameSize.width()-1);
  yMax = clip(yMax, 0, frameSize.height()-1);

  // Get the other RGB item (if any)
  videoHandlerRGB *rgbItem2 = (item2 == nullptr) ? nullptr : dynamic_cast<videoHandlerRGB*>(item2);
  if (item2 != nullptr && rgbItem2 == nullptr)
  {
    // The second item is not a videoHandlerRGB item
    frameHandler::drawPixelValues(painter, frameIdx, videoRect, zoomFactor, item2, markDifference, frameIdxItem1);
    return;
  }

  // Check if the raw RGB values are up to date. If not, do not draw them. Do not trigger loading of data here. The needsLoadingRawValues 
  // function will return that loading is needed. The caching in the background should then trigger loading of them.
  if (currentFrameRawRGBData_frameIdx != frameIdx)
    return;
  if (rgbItem2 && rgbItem2->currentFrameRawRGBData_frameIdx != frameIdxItem1)
    return;

  // The center point of the pixel (0,0).
  QPoint centerPointZero = (QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor)) / 2;
  // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize(QSize(zoomFactor, zoomFactor));
  const unsigned int drawWhitLevel = 1 << (srcPixelFormat.bitsPerValue - 1);
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);

      // Get the text to show
      QString valText;
      if (rgbItem2 != nullptr)
      {
        unsigned int R0, G0, B0, R1, G1, B1;
        getPixelValue(QPoint(x,y), R0, G0, B0);
        rgbItem2->getPixelValue(QPoint(x,y), R1, G1, B1);

        int DR = (int)R0-R1;
        int DG = (int)G0-G1;
        int DB = (int)B0-B1;
        if (markDifference)
          painter->setPen((DR == 0 && DG == 0 && DB == 0) ? Qt::white : Qt::black);
        else
          painter->setPen((DR < 0 && DG < 0 && DB < 0) ? Qt::white : Qt::black);
        valText = QString("R%1\nG%2\nB%3").arg(DR).arg(DG).arg(DB);
      }
      else
      {
        unsigned int R, G, B;
        getPixelValue(QPoint(x, y), R, G, B);
        valText = QString("R%1\nG%2\nB%3").arg(R).arg(G).arg(B);
        painter->setPen((R < drawWhitLevel && G < drawWhitLevel && B < drawWhitLevel) ? Qt::white : Qt::black);
      }

      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QImage videoHandlerRGB::calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  videoHandlerRGB *rgbItem2 = dynamic_cast<videoHandlerRGB*>(item2);
  if (rgbItem2 == nullptr)
    // The given item is not a RGB source. We cannot compare raw RGB values to non raw RGB values.
    // Call the base class comparison function to compare the items using the RGB 888 values.
    return videoHandler::calculateDifference(item2, frameIdxItem0, frameIdxItem1, differenceInfoList, amplificationFactor, markDifference);

  if (srcPixelFormat.bitsPerValue != rgbItem2->srcPixelFormat.bitsPerValue)
    // The two items have different bit depths. Compare RGB 888 values instead.
    return videoHandler::calculateDifference(item2, frameIdxItem0, frameIdxItem1, differenceInfoList, amplificationFactor, markDifference);

  const int width  = qMin(frameSize.width(), rgbItem2->frameSize.width());
  const int height = qMin(frameSize.height(), rgbItem2->frameSize.height());

  // Load the right raw RGB data (if not already loaded).
  // This will just update the raw RGB data. No conversion to image (RGB) is performed. This is either
  // done on request if the frame is actually shown or has already been done by the caching process.
  if (!loadRawRGBData(frameIdxItem0))
    return QImage();  // Loading failed
  if (!rgbItem2->loadRawRGBData(frameIdxItem1))
    return QImage();  // Loading failed

  // Also calculate the MSE while we're at it (R,G,B)
  qint64 mseAdd[3] = {0, 0, 0};

  // Create the output image in the right format
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA (each 8 bit).
  QImage outputImage;
  if (is_Q_OS_WIN)
    outputImage = QImage(frameSize, QImage::Format_ARGB32_Premultiplied);
  else if (is_Q_OS_MAC)
    outputImage = QImage(frameSize, QImage::Format_RGB32);
  else if (is_Q_OS_LINUX)
  {
    QImage::Format f = platformImageFormat();
    if (f == QImage::Format_ARGB32_Premultiplied)
      outputImage = QImage(frameSize, QImage::Format_ARGB32_Premultiplied);
    if (f == QImage::Format_ARGB32)
      outputImage = QImage(frameSize, QImage::Format_ARGB32);
    else
      outputImage = QImage(frameSize, QImage::Format_RGB32);
  }

  // We directly write the difference values into the QImage buffer in the right format (ABGR).
  unsigned char * restrict dst = outputImage.bits();

  if (srcPixelFormat.bitsPerValue >= 8 && srcPixelFormat.bitsPerValue <= 16)
  {
    // How many values do we have to skip in src to get to the next input value?
    // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
    int offsetToNextValue = (srcPixelFormat.alphaChannel) ? 4 : 3;
    if (srcPixelFormat.planar)
      offsetToNextValue = 1;

    if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
    {
      // 9 to 16 bits per component. We assume two bytes per value.
      // First get the pointer to the first value of each channel. (this item)
      unsigned short *srcR0, *srcG0, *srcB0;
      if (srcPixelFormat.planar)
      {
        srcR0 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG0 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB0 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR0 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG0 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB0 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
      }

      // Next get the pointer to the first value of each channel. (the other item)
      unsigned short *srcR1, *srcG1, *srcB1;
      if (srcPixelFormat.planar)
      {
        srcR1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB1 = (unsigned short*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posB;
      }

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          unsigned int offsetCoordinate = frameSize.width() * y + x;

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
            dst[0] = clip(128 + deltaB * amplificationFactor, 0, 255);
            dst[1] = clip(128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = clip(128 + deltaR * amplificationFactor, 0, 255);
          }
          dst[3] = 255;
          dst += 4;
        }
      }
    }
    else if (srcPixelFormat.bitsPerValue == 8)
    {
      // First get the pointer to the first value of each channel. (this item)
      unsigned char *srcR0, *srcG0, *srcB0;
      if (srcPixelFormat.planar)
      {
        srcR0 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG0 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB0 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR0 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG0 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB0 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
      }

      // First get the pointer to the first value of each channel. (other item)
      unsigned char *srcR1, *srcG1, *srcB1;
      if (srcPixelFormat.planar)
      {
        srcR1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height());
      }
      else
      {
        srcR1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB1 = (unsigned char*)rgbItem2->currentFrameRawRGBData.data() + srcPixelFormat.posB;
      }

      for (int y = 0; y < height; y++)
      {
        for (int x = 0; x < width; x++)
        {
          unsigned int offsetCoordinate = frameSize.width() * y + x;

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
            dst[0] = clip(128 + deltaB * amplificationFactor, 0, 255);
            dst[1] = clip(128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = clip(128 + deltaR * amplificationFactor, 0, 255);
          }
          dst[3] = 255;
          dst += 4;
        }
      }
    }
    else
      Q_ASSERT_X(false, "videoHandlerRGB::getPixelValue", "No RGB format with less than 8 or more than 16 bits supported yet.");
  }

  // Append the conversion information that will be returned
  differenceInfoList.append(infoItem("Difference Type", QString("RGB %1bit").arg(srcPixelFormat.bitsPerValue)));
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append(infoItem("MSE R",QString("%1").arg(mse[0])));
  differenceInfoList.append(infoItem("MSE G",QString("%1").arg(mse[1])));
  differenceInfoList.append(infoItem("MSE B",QString("%1").arg(mse[2])));
  differenceInfoList.append(infoItem("MSE All",QString("%1").arg(mse[3])));

  if (is_Q_OS_LINUX)
  {
    // On linux, we may have to convert the image to the platform image format if it is not one of the
    // RGBA formats.
    QImage::Format f = platformImageFormat();
    if (f != QImage::Format_ARGB32_Premultiplied && f != QImage::Format_ARGB32 && f != QImage::Format_RGB32)
      return outputImage.convertToFormat(f);
  }
  return outputImage;
}

void videoHandlerRGB::invalidateAllBuffers()
{
  currentFrameRawRGBData_frameIdx = -1;
  videoHandler::invalidateAllBuffers();
}
