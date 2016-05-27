/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "videoHandlerRGB.h"
#include <QtEndian>
#include <QTime>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include "stdio.h"
#include <QPainter>
#include <xmmintrin.h>
#include <QDebug>

// Activate this if you want to know when wich buffer is loaded/converted to pixmap and so on.
#define VIDEOHANDLERRGB_DEBUG_LOADING 0
#if VIDEOHANDLERRGB_DEBUG_LOADING
#define DEBUG_RGB qDebug
#else
#define DEBUG_RGB(fmt,...) ((void)0)
#endif

videoHandlerRGB_CustomFormatDialog::videoHandlerRGB_CustomFormatDialog(QString rgbFormat, int bitDepth, bool planar, bool alpha)
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
  name.append( QString(" %1bit").arg(bitsPerValue) );
  if (planar)
    name.append(" planar");

  return name;
}

void videoHandlerRGB::rgbPixelFormat::setFromName(QString name)
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
    bitsPerValue = name.mid( alphaChannel ? 5 : 4, (alphaChannel ? 5 : 4) - bitIdx).toInt();
    planar = name.contains("planar");
  }
}

QString videoHandlerRGB::rgbPixelFormat::getRGBFormatString() const
{
  QString name = "";
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

void videoHandlerRGB::rgbPixelFormat::setRGBFormatFromString(QString format)
{
  format = format.toLower();
  for (int i = 0; i < 3; i++)
  {
    if (format[i] == 'r')
      posR = i;
    else if (format[i] == 'g')
      posG = i;
    else if (format[i] == 'b')
      posB = i;
  }
}

/* The default constructor of the RGBFormatList will fill the list with all supported YUV file formats.
 * Don't forget to implement actual support for all of them in the conversion functions.
*/
videoHandlerRGB::RGBFormatList::RGBFormatList()
{
  append( rgbPixelFormat(8,  false, false, 0, 1, 2) );  // RGB 8bit
  append( rgbPixelFormat(10, false, false, 0, 1, 2) );  // RGB 10bit
  append( rgbPixelFormat(8,  false,  true, 0, 1, 2) );  // RGBA 8bit
  append( rgbPixelFormat(8,  false, false, 1, 2, 0) );  // BRG 8bit
  append( rgbPixelFormat(10, false, false, 1, 2, 0) );  // BRG 10bit
  append( rgbPixelFormat(10, false, true , 0, 1, 2) );  // RGB 10bit planar
}

/* Put all the names of the yuvPixelFormats into a list and return it
*/
QStringList videoHandlerRGB::RGBFormatList::getFormatedNames()
{
  QStringList l;
  for (int i = 0; i < count(); i++)
  {
    l.append( at(i).getName() );
  }
  return l;
}

videoHandlerRGB::rgbPixelFormat videoHandlerRGB::RGBFormatList::getFromName(QString name)
{
  for (int i = 0; i < count(); i++)
  {
    if ( at(i) == name )
      return at(i);
  }
  // If the format could not be found, we return the "Unknown Pixel Format" format
  return rgbPixelFormat();
}

// Initialize the static yuvFormatList
videoHandlerRGB::RGBFormatList videoHandlerRGB::rgbPresetList;

/* Get the number of bytes for a frame with this yuvPixelFormat and the given size
*/
qint64 videoHandlerRGB::rgbPixelFormat::bytesPerFrame(QSize frameSize)
{
  if (bitsPerValue == 0 || !frameSize.isValid())
    return 0;

  qint64 numSamples = frameSize.height() * frameSize.width();
  
  int channels = (alphaChannel) ? 4 : 3;
  return numSamples * channels * ((bitsPerValue + 7) / 8);
}

videoHandlerRGB::videoHandlerRGB() : videoHandler(),
  ui(new Ui::videoHandlerRGB)
{
  // preset internal values
  setSrcPixelFormat( rgbPixelFormat() );
  componentDisplayMode = DisplayAll;
  
  componentScale[0] = 1;
  componentScale[1] = 1;
  componentScale[2] = 1;

  componentInvert[0] = false;
  componentInvert[1] = false;
  componentInvert[2] = false;

  controlsCreated = false;
  currentFrameRawRGBData_frameIdx = -1;

  // Set the order of the rgb formats in the combo box
  orderRGBList << "RGB" << "RBG" << "GRB" << "GBR" << "BRG" << "BGR";
  bitDepthList << "8" << "10" << "12" << "16";
}

videoHandlerRGB::~videoHandlerRGB()
{
  // This will cause a "QMutex: destroying locked mutex" warning by Qt.
  // However, here this is on purpose.
  rgbFormatMutex.lock();
  delete ui;
}

ValuePairList videoHandlerRGB::getPixelValues(QPoint pixelPos)
{
  unsigned int Y,U,V;
  getPixelValue(pixelPos, Y, U, V);

  ValuePairList values;

  values.append( ValuePair("R", QString::number(Y)) );
  values.append( ValuePair("G", QString::number(U)) );
  values.append( ValuePair("B", QString::number(V)) );

  return values;
}

QLayout *videoHandlerRGB::createVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!controlsCreated);
  controlsCreated = true;

  QVBoxLayout *newVBoxLayout = NULL;
  if (!isSizeFixed)
  {
    // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the parent controls
    // and our controls into that layout, seperated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout( videoHandler::createVideoHandlerControls(parentWidget, isSizeFixed) );
  
    QFrame *line = new QFrame(parentWidget);
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }

  ui->setupUi(parentWidget);

  // Set all the values of the properties widget to the values of this class
  ui->rgbFormatComboBox->addItems( rgbPresetList.getFormatedNames() );
  ui->rgbFormatComboBox->addItem( "Custom..." );
  int idx = rgbPresetList.indexOf( srcPixelFormat );
  if (idx == -1)
    ui->rgbFormatComboBox->setCurrentText("Unknown pixel format");
  else if (idx > 0)
    ui->rgbFormatComboBox->setCurrentIndex( idx );  
  else
    // Custom pixel format (but a known pixel format)
    ui->rgbFormatComboBox->setCurrentText( srcPixelFormat.getName() );
  ui->rgbFormatComboBox->setEnabled(!isSizeFixed);

  ui->colorComponentsComboBox->addItems( QStringList() << "RGB" << "Red Only" << "Green only" << "Blue only" );
  ui->colorComponentsComboBox->setCurrentIndex( (int)componentDisplayMode );
  
  ui->RScaleSpinBox->setValue(componentScale[0]);
  ui->RScaleSpinBox->setMaximum(1000);
  ui->GScaleSpinBox->setValue(componentScale[1]);
  ui->GScaleSpinBox->setMaximum(1000);
  ui->BScaleSpinBox->setValue(componentScale[2]);
  ui->BScaleSpinBox->setMaximum(1000);

  // Connect all the change signals from the controls
  connect(ui->rgbFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotRGBFormatControlChanged()));
  connect(ui->colorComponentsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->RScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->GScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->BScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->RInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->GInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  connect(ui->BInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotDisplayOptionsChanged()));
  
  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui->topVerticalLayout);

  if (isSizeFixed)
    return ui->topVerticalLayout;
  else
    return newVBoxLayout;
}

void videoHandlerRGB::slotDisplayOptionsChanged()
{
  componentDisplayMode = (ComponentDisplayMode)ui->colorComponentsComboBox->currentIndex();
  componentScale[0] = ui->RScaleSpinBox->value();
  componentScale[1] = ui->GScaleSpinBox->value();
  componentScale[2] = ui->BScaleSpinBox->value();
  componentInvert[0] = ui->RInvertCheckBox->isChecked();
  componentInvert[1] = ui->GInvertCheckBox->isChecked();
  componentInvert[2] = ui->BInvertCheckBox->isChecked();
    
  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  currentFrameIdx = -1;
  if (pixmapCache.count() > 0)
    pixmapCache.clear();
  emit signalHandlerChanged(true, true);
}

void videoHandlerRGB::slotRGBFormatControlChanged()
{
  // What is the current selection?
  int idx = ui->rgbFormatComboBox->currentIndex();

  // The old format's nr bytes per frame
  qint64 nrBytesOldFormat = getBytesPerFrame();

  if (idx == rgbPresetList.count())
  {
    // The user selected the "cutom format..." option
    videoHandlerRGB_CustomFormatDialog dialog( srcPixelFormat.getRGBFormatString(), srcPixelFormat.bitsPerValue, srcPixelFormat.planar, srcPixelFormat.alphaChannel );
    if (dialog.exec() == QDialog::Accepted)
    {
      // Set the custom format
      srcPixelFormat.setRGBFormatFromString( dialog.getRGBFormat() );
      srcPixelFormat.bitsPerValue = dialog.getBitDepth();
      srcPixelFormat.planar = dialog.getPlanar();
      srcPixelFormat.alphaChannel = dialog.getAlphaChannel();
    }

    // Check if the custom format it in the presets list. If not, add it
    int idx = rgbPresetList.indexOf( srcPixelFormat );
    if (idx == -1 && srcPixelFormat.isValid())
    {
      // Valid pixel format with is not in the list. Add it...
      rgbPresetList.append( srcPixelFormat );
      int nrItems = ui->rgbFormatComboBox->count();
      disconnect(ui->rgbFormatComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
      ui->rgbFormatComboBox->insertItem( nrItems - 1, srcPixelFormat.getName() );
      connect(ui->rgbFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotRGBFormatControlChanged()));
      idx = rgbPresetList.indexOf( srcPixelFormat );
    }
    
    if (idx > 0)
      // Format found. Set it without another call to this function.
      disconnect(ui->rgbFormatComboBox, SIGNAL(currentIndexChanged(int)));
      ui->rgbFormatComboBox->setCurrentIndex( idx );
      connect(ui->rgbFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotRGBFormatControlChanged()));
  }
  else
  {
    // One of the preset formats was selected
    setSrcPixelFormat( rgbPresetList.at(idx) );
  }

  // Set the current frame in the buffer to be invalid and clear the cache.
  // Emit that this item needs redraw and the cache needs updating.
  currentFrameIdx = -1;
  if (nrBytesOldFormat != getBytesPerFrame())
  {
    // The number of bytes per frame changed -> the number of frames in the sequence changed.
    emit signalUpdateFrameLimits();
    // The raw rgb data buffer also needs to be reloaded
    currentFrameRawRGBData_frameIdx = -1;
  }
  if (pixmapCache.count() > 0)
    pixmapCache.clear();
  emit signalHandlerChanged(true, true);
}

void videoHandlerRGB::loadFrame(int frameIndex)
{
  DEBUG_RGB( "videoHandlerRGB::loadFrame %d\n", frameIndex );

  if (!isFormatValid())
    // We cannot load a frame if the format is not known
    return;

  // Does the data in currentFrameRawYUVData need to be updated?
  if (!loadRawRGBData(frameIndex))
    // Loading failed 
    return;

  // The data in currentFrameRawYUVData is now up to date. If necessary
  // convert the data to RGB.
  if (currentFrameIdx != frameIndex)
  {
    convertRGBToPixmap(currentFrameRawRGBData, currentFrame, tmpBufferRGB);
    currentFrameIdx = frameIndex;
  }
}

void videoHandlerRGB::loadFrameForCaching(int frameIndex, QPixmap &frameToCache)
{
  DEBUG_RGB( "videoHandlerRGB::loadFrameForCaching %d", frameIndex );

  // Lock the mutex for the yuvFormat. The main thread has to wait until caching is done
  // before the yuv format can change.
  rgbFormatMutex.lock();

  rawDataMutex.lock();
  emit signalRequesRawData(frameIndex);
  tmpBufferRawRGBDataCaching = rawData;
  rawDataMutex.unlock();

  if (frameIndex != rawData_frameIdx)
  {
    // Loading failed
    currentFrameIdx = -1;
    rgbFormatMutex.unlock();
    return;
  }

  // Convert YUV to pixmap. This can then be cached.
  convertRGBToPixmap(tmpBufferRawRGBDataCaching, frameToCache, tmpBufferRGBCaching);

  rgbFormatMutex.unlock();
}

// Load the raw YUV data for the given frame index into currentFrameRawYUVData.
bool videoHandlerRGB::loadRawRGBData(int frameIndex)
{
  if (currentFrameRawRGBData_frameIdx == frameIndex)
    // Buffer already up to date
    return true;

  DEBUG_RGB( "videoHandlerRGB::loadRawRGBData %d", frameIndex );

  // The function loadFrameForCaching also uses the signalRequesRawYUVData to request raw data.
  // However, only one thread can use this at a time.
  rawDataMutex.lock();
  emit signalRequesRawData(frameIndex);

  if (frameIndex != rawData_frameIdx)
  {
    // Loading failed
    currentFrameRawRGBData_frameIdx = -1;
  }
  else
  {
    currentFrameRawRGBData = rawData;
    currentFrameRawRGBData_frameIdx = frameIndex;
  }

  rawDataMutex.unlock();
  return (currentFrameRawRGBData_frameIdx == frameIndex);
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to pixmap (RGB-888), using the
// buffer tmpRGBBuffer for intermediate RGB values.
void videoHandlerRGB::convertRGBToPixmap(QByteArray sourceBuffer, QPixmap &outputPixmap, QByteArray &tmpRGBBuffer)
{
  DEBUG_RGB( "videoHandlerRGB::convertRGBToPixmap" );

  convertSourceToRGB888(sourceBuffer, tmpRGBBuffer);

  // Convert the image in tmpRGBBuffer to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  QImage tmpImage((unsigned char*)tmpRGBBuffer.data(), frameSize.width(), frameSize.height(), QImage::Format_RGB888);

  // Set the videoHanlder pixmap and image so the videoHandler can draw the item
  outputPixmap.convertFromImage(tmpImage);
}

// Convert the data in "sourceBuffer" from the format "srcPixelFormat" to RGB 888. While doing so, apply the 
// scaling factors, inversions and only convert the selected color components.
void videoHandlerRGB::convertSourceToRGB888(QByteArray &sourceBuffer, QByteArray &targetBuffer)
{
  // Check if the source buffer is of the correct size
  Q_ASSERT_X(sourceBuffer.size() >= getBytesPerFrame(), "videoHandlerRGB::convertSourceToRGB888", "The source buffer does not hold enough data.");
  
  // Adjust the targetsBuffer size. The output is RGB888
  int targetSize = frameSize.width() * frameSize.height() * 3;
  if (targetBuffer.size() < targetSize)
    targetBuffer.resize(targetSize);

  // Get the raw data pointer to the output array
  unsigned char* dst = (unsigned char*)targetBuffer.data();

  // How many values do we have to skip in src to get to the next input value?
  // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
  int offsetToNextValue = (srcPixelFormat.alphaChannel) ? 4 : 3;
  if (srcPixelFormat.planar)
    offsetToNextValue = 1;

  if (componentDisplayMode != DisplayAll)
  {
    // Only convert one of the components to a grayscale image.
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

        src += offsetToNextValue;
        dst += 3;
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

        src += offsetToNextValue;
        dst += 3;
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
        dst[0] = valR;
        
        // G
        int valG = (((int)srcG[0]) * componentScale[1]) >> rightShift;
        valG = clip(valG, 0, 255);
        if (componentInvert[1])
          valG = 255 - valG;
        dst[1] = valG;

        // B
        int valB = (((int)srcB[0]) * componentScale[2]) >> rightShift;
        valB = clip(valB, 0, 255);
        if (componentInvert[2])
          valB = 255 - valB;
        dst[2] = valB;

        srcR += offsetToNextValue;
        srcG += offsetToNextValue;
        srcB += offsetToNextValue;
        dst += 3;
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
        dst[0] = valR;
        
        // G
        int valG = ((int)srcG[0]) * componentScale[1];
        valG = clip(valG, 0, 255);
        if (componentInvert[1])
          valG = 255 - valG;
        dst[1] = valG;

        // B
        int valB = ((int)srcB[0]) * componentScale[2];
        valB = clip(valB, 0, 255);
        if (componentInvert[2])
          valB = 255 - valB;
        dst[2] = valB;

        srcR += offsetToNextValue;
        srcG += offsetToNextValue;
        srcB += offsetToNextValue;
        dst += 3;
      }
    }
    else
      Q_ASSERT_X(false, "videoHandlerRGB::convertSourceToRGB888", "No RGB format with less than 8 or more than 16 bits supported yet.");
  }
  else
    Q_ASSERT_X(false, "videoHandlerRGB::convertSourceToRGB888", "Unsupported display mode.");
}

void videoHandlerRGB::getPixelValue(QPoint pixelPos, unsigned int &R, unsigned int &G, unsigned int &B)
{
  // Update the raw RGB data if necessary
  loadRawRGBData(currentFrameIdx);

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

bool videoHandlerRGB::isPixelDark(QPoint pixelPos)
{
  unsigned int R, G, B;
  getPixelValue( pixelPos, R, G, B );
  const unsigned int drawWhitLevel = 1 << (srcPixelFormat.bitsPerValue - 1);
  return (R < drawWhitLevel && G < drawWhitLevel && B < drawWhitLevel);
}

void videoHandlerRGB::setFrameSize(QSize size, bool emitSignal)
{
  if (size != frameSize)
  {
    currentFrameRawRGBData_frameIdx = -1;
    currentFrameIdx = -1;
  }

  videoHandler::setFrameSize(size, emitSignal);
}

void videoHandlerRGB::setFormatFromSize(QSize size, int bitDepth, qint64 fileSize, QString subFormat)
{
  // If the bit depth could not be determined, check 8 and 10 bit
  int testBitDepths = (bitDepth > 0) ? 1 : 2;

  for (int i = 0; i < testBitDepths; i++)
  {
    if (testBitDepths == 2)
      bitDepth = (i == 0) ? 8 : 10;

    if (bitDepth==8)
    {
      // assume rgb if subFormat does not indicate anything else
      rgbPixelFormat cFormat;
      cFormat.setRGBFormatFromString(subFormat);
      cFormat.bitsPerValue = 8;
      
      // Check if the file size and the assumed format match
      int bpf = cFormat.bytesPerFrame( size );
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        frameSize = size;
        setSrcPixelFormat( cFormat );
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
      int bpf = cFormat.bytesPerFrame( size );
      if (bpf != 0 && (fileSize % bpf) == 0)
      {
        // Bits per frame and file size match
        frameSize = size;
        setSrcPixelFormat( cFormat );
        return;
      }
    }
    else
    {
        // do other stuff
    }
  }
}

void videoHandlerRGB::drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2)
{
  // Get the other RGB item (if any)
  videoHandlerRGB *rgbItem2 = NULL;
  if (item2 != NULL)
    rgbItem2 = dynamic_cast<videoHandlerRGB*>(item2);

  // The center point of the pixel (0,0).
  QPoint centerPointZero = ( QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );
  const unsigned int drawWhitLevel = 1 << (srcPixelFormat.bitsPerValue - 1);
  for (unsigned int x = xMin; x <= xMax; x++)
  {
    for (unsigned int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);
     
      // Get the text to show
      QString valText;
      if (rgbItem2 != NULL)
      {
        unsigned int R0, G0, B0, R1, G1, B1;
        getPixelValue(QPoint(x,y), R0, G0, B0);
        rgbItem2->getPixelValue(QPoint(x,y), R1, G1, B1);

        valText = QString("R%1\nG%2\nB%3").arg(R0-R1).arg(G0-G1).arg(B0-B1);
        painter->setPen( Qt::white );
      }
      else
      {
        unsigned int R, G, B;
        getPixelValue( QPoint(x, y), R, G, B );
        valText = QString("R%1\nG%2\nB%3").arg(R).arg(G).arg(B);
        painter->setPen( (R < drawWhitLevel && G < drawWhitLevel && B < drawWhitLevel) ? Qt::white : Qt::black );
      }
      
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QPixmap videoHandlerRGB::calculateDifference(videoHandler *item2, int frame, QList<infoItem> &conversionInfoList, int amplificationFactor, bool markDifference)
{
  videoHandlerRGB *rgbItem2 = dynamic_cast<videoHandlerRGB*>(item2);
  if (rgbItem2 == NULL)
    // The given item is not a yuv source. We cannot compare YUV values to non YUV values.
    // Call the base class comparison function to compare the items using the RGB values.
    videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);

  if (srcPixelFormat.bitsPerValue != rgbItem2->srcPixelFormat.bitsPerValue)
    // The two items have different bit depths. Compare RGB values instead.
    // TODO: Or should we do this in the YUV domain somehow?
    videoHandler::calculateDifference(item2, frame, conversionInfoList, amplificationFactor, markDifference);

  const int width  = qMin(frameSize.width(), rgbItem2->frameSize.width());
  const int height = qMin(frameSize.height(), rgbItem2->frameSize.height());

  // Load the right raw YUV data (if not already loaded).
  // This will just update the raw YUV data. No conversion to pixmap (RGB) is performed. This is either
  // done on request if the frame is actually shown or has already been done by the caching process.
  if (!loadRawRGBData(frame))
    return QPixmap();  // Loading failed
  if (!rgbItem2->loadRawRGBData(frame))
    return QPixmap();  // Loading failed
  
  // Also calculate the MSE while we're at it (R,G,B)
  qint64 mseAdd[3] = {0, 0, 0};

  // The output array to be converted to pixmap (RGB 8bit)
  QByteArray tmpDiffBufferRGB;
  tmpDiffBufferRGB.resize( width * height * 3 );
  unsigned char *dst = (unsigned char*)tmpDiffBufferRGB.data();

  if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
  {
    // 9 to 16 bits per component. We assume two bytes per value.

    // How many values do we have to skip in src to get to the next input value?
    // In case of 8 or less bits this is 1 byte per value, for 9 to 16 bits it is 2 bytes per value.
    int offsetToNextValue = (srcPixelFormat.alphaChannel) ? 4 : 3;
    if (srcPixelFormat.planar)
      offsetToNextValue = 1;
    
    if (srcPixelFormat.bitsPerValue > 8 && srcPixelFormat.bitsPerValue <= 16)
    {
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

      // First get the pointer to the first value of each channel. (the other item)
      unsigned short *srcR1, *srcG1, *srcB1;
      if (srcPixelFormat.planar)
      {
        srcR1 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG1 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB1 = (unsigned short*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height()); 
      }
      else
      {
        srcR1 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG1 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB1 = (unsigned short*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
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
            dst[0] = (deltaR == 0) ? 0 : 255;
            dst[1] = (deltaG == 0) ? 0 : 255;
            dst[2] = (deltaB == 0) ? 0 : 255;
          }
          else
          {
            // We want to see the difference
            dst[0] = clip( 128 + deltaR * amplificationFactor, 0, 255);
            dst[1] = clip( 128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = clip( 128 + deltaB * amplificationFactor, 0, 255);
          }
          dst += 3;
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
        srcR1 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posR * frameSize.width() * frameSize.height());
        srcG1 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posG * frameSize.width() * frameSize.height());
        srcB1 = (unsigned char*)currentFrameRawRGBData.data() + (srcPixelFormat.posB * frameSize.width() * frameSize.height()); 
      }
      else
      {
        srcR1 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posR;
        srcG1 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posG;
        srcB1 = (unsigned char*)currentFrameRawRGBData.data() + srcPixelFormat.posB;
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
            dst[0] = (deltaR == 0) ? 0 : 255;
            dst[1] = (deltaG == 0) ? 0 : 255;
            dst[2] = (deltaB == 0) ? 0 : 255;
          }
          else
          {
            // We want to see the difference
            dst[0] = clip( 128 + deltaR * amplificationFactor, 0, 255);
            dst[1] = clip( 128 + deltaG * amplificationFactor, 0, 255);
            dst[2] = clip( 128 + deltaB * amplificationFactor, 0, 255);
          }
          dst += 3;
        }
      }
    }
    else
      Q_ASSERT_X(false, "videoHandlerRGB::getPixelValue", "No RGB format with less than 8 or more than 16 bits supported yet.");
  }

  // Append the conversion information that will be returned
  conversionInfoList.append( infoItem("Difference Type", QString("RGB %1bit").arg(srcPixelFormat.bitsPerValue)) );
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  conversionInfoList.append( infoItem("MSE R",QString("%1").arg(mse[0])) );
  conversionInfoList.append( infoItem("MSE G",QString("%1").arg(mse[1])) );
  conversionInfoList.append( infoItem("MSE B",QString("%1").arg(mse[2])) );
  conversionInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  // Convert the image in tmpDiffBufferRGB to a QPixmap using a QImage intermediate.
  // TODO: Isn't there a faster way to do this? Maybe load a pixmap from "BMP"-like data?
  QImage tmpImage((unsigned char*)tmpDiffBufferRGB.data(), width, height, QImage::Format_RGB888);
  QPixmap retPixmap;
  retPixmap.convertFromImage(tmpImage);
  return retPixmap;
}