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

#include "frameHandler.h"

#include <QPainter>

// ------ Initialize the static list of frame size presets ----------

frameHandler::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size" << "QCIF" << "QVGA" << "WQVGA" << "CIF" << "VGA" << "WVGA" << "4CIF" << "ITU R.BT601" << "720i/p" << "1080i/p" << "4k" << "XGA" << "XGA+";
  sizes << QSize(-1,-1) << QSize(176,144) << QSize(320, 240) << QSize(416, 240) << QSize(352, 288) << QSize(640, 480) << QSize(832, 480) << QSize(704, 576) << QSize(720, 576) << QSize(1280, 720) << QSize(1920, 1080) << QSize(3840, 2160) << QSize(1024, 768) << QSize(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList frameHandler::frameSizePresetList::getFormatedNames()
{
  QStringList presetList;
  presetList.append( "Custom Size" );

  for (int i = 1; i < names.count(); i++)
  {
    QString str = QString("%1 (%2,%3)").arg( names[i] ).arg( sizes[i].width() ).arg( sizes[i].height() );
    presetList.append( str );
  }

  return presetList;
}

// Initialize the static list of frame size presets
frameHandler::frameSizePresetList frameHandler::presetFrameSizes;

// ---------------- frameHandler ---------------------------------

frameHandler::frameHandler()
{
}

frameHandler::~frameHandler()
{
}

QLayout *frameHandler::createFrameHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  ui.setupUi();

  // Set default values
  ui.widthSpinBox->setMaximum(100000);
  ui.widthSpinBox->setValue( frameSize.width() );
  ui.widthSpinBox->setEnabled( !isSizeFixed );
  ui.heightSpinBox->setMaximum(100000);
  ui.heightSpinBox->setValue( frameSize.height() );
  ui.heightSpinBox->setEnabled( !isSizeFixed );
  ui.frameSizeComboBox->addItems( presetFrameSizes.getFormatedNames() );
  int idx = presetFrameSizes.findSize( frameSize );
  ui.frameSizeComboBox->setCurrentIndex(idx);
  ui.frameSizeComboBox->setEnabled( !isSizeFixed );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(ui.heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(ui.frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoControlChanged()));

  return ui.frameHandlerLayout;
}

void frameHandler::setFrameSize(QSize newSize, bool emitSignal)
{
  if (newSize == frameSize)
    // Nothing to update
    return;

  // Set the new size
  frameSize = newSize;

  if (!ui.created())
    // spin boxes not created yet
    return;

  // Set the width/height spin boxes without emitting another signal (disconnect/set/reconnect)
  if (!emitSignal)
  {
    QObject::disconnect(ui.widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(ui.heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  }

  ui.widthSpinBox->setValue( newSize.width() );
  ui.heightSpinBox->setValue( newSize.height() );

  if (!emitSignal)
  {
    QObject::connect(ui.widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    QObject::connect(ui.heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  }
}

bool frameHandler::loadCurrentImageFromFile(QString filePath)
{
  // Load the image and return if loading was successfull.
  currentImage = QImage(filePath);
  setFrameSize(currentImage.size());

  return (!currentImage.isNull());
}

void frameHandler::slotVideoControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  QSize newSize;
  if (sender == ui.widthSpinBox || sender == ui.heightSpinBox)
  {
    newSize = QSize( ui.widthSpinBox->value(), ui.heightSpinBox->value() );
    if (newSize != frameSize)
    {
      // Set the comboBox index without causing another signal to be emitted (disconnect/set/reconnect).
      QObject::disconnect(ui.frameSizeComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
      int idx = presetFrameSizes.findSize( newSize );
      ui.frameSizeComboBox->setCurrentIndex(idx);
      QObject::connect(ui.frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoControlChanged()));
    }
  }
  else if (sender == ui.frameSizeComboBox)
  {
    newSize = presetFrameSizes.getSize( ui.frameSizeComboBox->currentIndex() );
  }

  if (newSize != frameSize && newSize != QSize(-1,-1))
  {
    // Set the new size and update the controls.
    setFrameSize(newSize);
    // The frame size changed. We need to redraw/re-cache.
    emit signalHandlerChanged(true, true);
  }
}

void frameHandler::drawFrame(QPainter *painter, double zoomFactor)
{
  // Create the video rect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize( frameSize * zoomFactor );
  videoRect.moveCenter( QPoint(0,0) );

  // Draw the current image ( currentFrame )
  painter->drawImage( videoRect, currentImage );

  if (zoomFactor >= 64)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, 0, videoRect, zoomFactor);
  }
}

void frameHandler::drawPixelValues(QPainter *painter, const int frameIdx, const QRect videoRect, const double zoomFactor, frameHandler *item2, const bool markDifference)
{
  // Draw the pixel values onto the pixels
  Q_UNUSED(frameIdx);

  // TODO: Does this also work for sequences with width/height non divisible by 2? Not sure about that.
    
  // First determine which pixels from this item are actually visible, because we only have to draw the pixel values
  // of the pixels that are actually visible
  QRect viewport = painter->viewport();
  QTransform worldTransform = painter->worldTransform();
    
  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width() )) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height() )) / zoomFactor;

  // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
  // item have to be labeled)
  xMin = clip(xMin, 0, frameSize.width()-1);
  yMin = clip(yMin, 0, frameSize.height()-1);
  xMax = clip(xMax, 0, frameSize.width()-1);
  yMax = clip(yMax, 0, frameSize.height()-1);

  // The center point of the pixel (0,0).
  QPoint centerPointZero = ( QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);
     
      // Get the text to show
      bool drawWhite = false;
      QRgb pixVal;
      if (item2 != NULL)
      {
        QRgb pixel1 = getPixelVal(x, y);
        QRgb pixel2 = item2->getPixelVal(x, y);

        int dR = qRed(pixel1) - qRed(pixel2);
        int dG = qGreen(pixel1) - qGreen(pixel2);
        int dB = qBlue(pixel1) - qBlue(pixel2);

        int r = clip( 128 + dR, 0, 255);
        int g = clip( 128 + dG, 0, 255);
        int b = clip( 128 + dB, 0, 255);

        pixVal = qRgb(r,g,b);

        if (markDifference)
          drawWhite = (dR == 0 && dG == 0 && dB == 0);
        else
          drawWhite = (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
      }
      else
      {
        pixVal = getPixelVal(x, y);
        drawWhite = (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
      }
      QString valText = QString("R%1\nG%2\nB%3").arg(qRed(pixVal)).arg(qGreen(pixVal)).arg(qBlue(pixVal));
           
      painter->setPen( drawWhite ? Qt::white : Qt::black );
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QPixmap frameHandler::calculateDifference(frameHandler *item2, const int frame, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference)
{
  Q_UNUSED(frame);

  int width  = qMin(frameSize.width(), item2->frameSize.width());
  int height = qMin(frameSize.height(), item2->frameSize.height());

  QImage diffImg(width, height, QImage::Format_RGB32);

  // Also calculate the MSE while we're at it (R,G,B)
  qint64 mseAdd[3] = {0, 0, 0};

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      QRgb pixel1 = getPixelVal(x, y);
      QRgb pixel2 = item2->getPixelVal(x, y);

      int dR = qRed(pixel1) - qRed(pixel2);
      int dG = qGreen(pixel1) - qGreen(pixel2);
      int dB = qBlue(pixel1) - qBlue(pixel2);

      int r, g, b;
      if (markDifference)
      {
        r = (dR != 0) ? 255 : 0;
        g = (dG != 0) ? 255 : 0;
        b = (dB != 0) ? 255 : 0;
      }
      else if (amplificationFactor != 1)
      {  
        r = clip( 128 + dR * amplificationFactor, 0, 255);
        g = clip( 128 + dG * amplificationFactor, 0, 255);
        b = clip( 128 + dB * amplificationFactor, 0, 255);
      }
      else
      {  
        r = clip( 128 + dR, 0, 255);
        g = clip( 128 + dG, 0, 255);
        b = clip( 128 + dB, 0, 255);
      }
      
      mseAdd[0] += dR * dR;
      mseAdd[1] += dG * dG;
      mseAdd[2] += dB * dB;

      QRgb val = qRgb( r, g, b );
      diffImg.setPixel(x, y, val);
    }
  }

  differenceInfoList.append( infoItem("Difference Type","RGB") );
  
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append( infoItem("MSE R",QString("%1").arg(mse[0])) );
  differenceInfoList.append( infoItem("MSE G",QString("%1").arg(mse[1])) );
  differenceInfoList.append( infoItem("MSE B",QString("%1").arg(mse[2])) );
  differenceInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  return QPixmap::fromImage(diffImg);
}

bool frameHandler::isPixelDark(QPoint pixelPos)
{
  QRgb pixVal = getPixelVal(pixelPos);
  return (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
}

ValuePairList frameHandler::getPixelValues(QPoint pixelPos, int frameIdx, frameHandler *item2)
{
  Q_UNUSED(frameIdx);

  int width  = qMin(frameSize.width(), item2 ? item2->frameSize.width() : 0);
  int height = qMin(frameSize.height(), item2 ? item2->frameSize.height() : 0);

  if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
    return ValuePairList();

  // Is the format (of both items) valid?
  if (!isFormatValid())
    return ValuePairList();
  if (item2 && !item2->isFormatValid())
    return ValuePairList();

  // Get the RGB values from the pixmap
  ValuePairList values;

  if (item2)
  {
    // There is a second item. Return the difference values.
    QRgb pixel1 = getPixelVal( pixelPos );
    QRgb pixel2 = item2->getPixelVal( pixelPos );

    int r = qRed(pixel1) - qRed(pixel2);
    int g = qGreen(pixel1) - qGreen(pixel2);
    int b = qBlue(pixel1) - qBlue(pixel2);

    ValuePairList diffValues;
    diffValues.append( ValuePair("R", QString::number(r)) );
    diffValues.append( ValuePair("G", QString::number(g)) );
    diffValues.append( ValuePair("B", QString::number(b)) );
  }
  else
  {
    // No second item. Return the RGB values of this iten.
    QRgb val = getPixelVal(pixelPos);
    values.append( ValuePair("R", QString::number(qRed(val))) );
    values.append( ValuePair("G", QString::number(qGreen(val))) );
    values.append( ValuePair("B", QString::number(qBlue(val))) );
  }

  return values;
}
