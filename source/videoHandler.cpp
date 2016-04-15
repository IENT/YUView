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

#include <assert.h>
#include <QTime>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>

#include "videoHandler.h"

// ------ Initialize the static list of frame size presets ----------

videoHandler::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size" << "QCIF" << "QVGA" << "WQVGA" << "CIF" << "VGA" << "WVGA" << "4CIF" << "ITU R.BT601" << "720i/p" << "1080i/p" << "4k" << "XGA" << "XGA+";
  sizes << QSize(-1,-1) << QSize(176,144) << QSize(320, 240) << QSize(416, 240) << QSize(352, 288) << QSize(640, 480) << QSize(832, 480) << QSize(704, 576) << QSize(720, 576) << QSize(1280, 720) << QSize(1920, 1080) << QSize(3840, 2160) << QSize(1024, 768) << QSize(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList videoHandler::frameSizePresetList::getFormatedNames()
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
videoHandler::frameSizePresetList videoHandler::presetFrameSizes;

// --------- videoHandler -------------------------------------

videoHandler::videoHandler()
{
  // Init variables
  frameRate = DEFAULT_FRAMERATE;
  startEndFrame = indexRange(-1,-1);
  startEndFrameLimit = indexRange(-1,-1);
  sampling = 1;
  currentFrameIdx = -1;
  currentFrame_Image_FrameIdx = -1;
  controlsCreated = false;
  startEndFrameChanged = false;

  connect(&cachingTimer, SIGNAL(timeout()), this, SLOT(cachingTimerEvent()));
  connect(this, SIGNAL(cachingTimerStart()), &cachingTimer, SLOT(start()));
}

videoHandler::~videoHandler()
{
}

QLayout *videoHandler::createVideoHandlerControls(QWidget *parentWidget, bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!controlsCreated);

  setupUi(parentWidget);

  if (startEndFrame == indexRange(-1,-1))
  {
    emit signalGetFrameLimits();
  }

  // Set default values
  widthSpinBox->setMaximum(100000);
  widthSpinBox->setValue( frameSize.width() );
  widthSpinBox->setEnabled( !isSizeFixed );
  heightSpinBox->setMaximum(100000);
  heightSpinBox->setValue( frameSize.height() );
  heightSpinBox->setEnabled( !isSizeFixed );
  startSpinBox->setMinimum( startEndFrameLimit.first );
  startSpinBox->setMaximum( startEndFrameLimit.second );
  startSpinBox->setValue( startEndFrame.first );
  endSpinBox->setMinimum( startEndFrameLimit.first );
  endSpinBox->setMaximum( startEndFrameLimit.second );
  endSpinBox->setValue( startEndFrame.second );
  rateSpinBox->setValue( frameRate );
  rateSpinBox->setMaximum(1000);
  samplingSpinBox->setMinimum(1);
  samplingSpinBox->setMaximum(100000);
  samplingSpinBox->setValue( sampling );
  frameSizeComboBox->addItems( presetFrameSizes.getFormatedNames() );
  int idx = presetFrameSizes.findSize( frameSize );
  frameSizeComboBox->setCurrentIndex(idx);
  frameSizeComboBox->setEnabled( !isSizeFixed );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(slotVideoControlChanged()));
  connect(samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  connect(frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoControlChanged()));

  // The controls have been created and can be used now
  controlsCreated = true;

  return videoHandlerLayout;
}

void videoHandler::setFrameSize(QSize newSize, bool emitSignal)
{
  if (newSize == frameSize)
    // Nothing to update
    return;

  // Set the new size
  cachingFrameSizeMutex.lock();
  frameSize = newSize;
  cachingFrameSizeMutex.unlock();

  if (!controlsCreated)
    // spin boxes not created yet
    return;

  // Set the width/height spin boxes without emitting another signal (disconnect/set/reconnect)
  if (!emitSignal)
  {
    QObject::disconnect(widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  }

  widthSpinBox->setValue( newSize.width() );
  heightSpinBox->setValue( newSize.height() );

  if (!emitSignal)
  {
    QObject::connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    QObject::connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  }
}

void videoHandler::setStartEndFrame(indexRange range, bool emitSignal)
{
  // Set the new start/end frame
  startEndFrame = range;

  if (!controlsCreated)
    // spin boxes not created yet
    return;

  if (!emitSignal)
  {
    QObject::disconnect(startSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  }

  startSpinBox->setMinimum( startEndFrameLimit.first );
  startSpinBox->setMaximum( startEndFrameLimit.second );
  startSpinBox->setValue( startEndFrame.first );
  endSpinBox->setMinimum( startEndFrameLimit.first );
  endSpinBox->setMaximum( startEndFrameLimit.second );
  endSpinBox->setValue( startEndFrame.second );

  if (!emitSignal)
  {
    connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  }
}

void videoHandler::setFrameLimits(indexRange limits) 
{ 
  // If the limits changed, update the start/end spin boxes if necessary but emit no signals
  if (startEndFrameLimit != limits)
  {
    startEndFrameLimit = limits;
    if (!startEndFrameChanged)
      startEndFrame = limits;

    if (!controlsCreated)
      // spin boxes not created yet
      return;

    if (startSpinBox->minimum() != startEndFrameLimit.first || 
        startSpinBox->maximum() != startEndFrameLimit.second  )
    {
      // The limits have changed. Set them and update the current value if necessary.
      QObject::disconnect(startSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);

      startSpinBox->setMinimum( startEndFrameLimit.first );
      startSpinBox->setMaximum( startEndFrameLimit.second );
      if ( startSpinBox->value() < startEndFrameLimit.first )
        startSpinBox->setValue( startEndFrameLimit.first );
      if ( startSpinBox->value() > startEndFrameLimit.second )
        startSpinBox->setValue( startEndFrameLimit.second );

      connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    }
      
    if (endSpinBox->minimum() != startEndFrameLimit.first || 
        endSpinBox->maximum() != startEndFrameLimit.second  )
    {
      // The limits have changed. Set them and update the current value if necessary.
      QObject::disconnect(endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);

      endSpinBox->setMinimum( startEndFrameLimit.first );
      endSpinBox->setMaximum( startEndFrameLimit.second );
      if ( endSpinBox->value() < startEndFrameLimit.first )
        endSpinBox->setValue( startEndFrameLimit.first );
      if ( endSpinBox->value() > startEndFrameLimit.second )
        endSpinBox->setValue( startEndFrameLimit.second );

      connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    }
  }
}

void videoHandler::slotVideoControlChanged()
{
  // The control that caused the slot to be called
  QObject *sender = QObject::sender();

  if (sender == widthSpinBox || sender == heightSpinBox)
  {
    QSize newSize = QSize( widthSpinBox->value(), heightSpinBox->value() );
    if (newSize != frameSize)
    {
      // Set the comboBox index without causing another signal to be emitted (disconnect/set/reconnect).
      QObject::disconnect(frameSizeComboBox, SIGNAL(currentIndexChanged(int)), NULL, NULL);
      int idx = presetFrameSizes.findSize( newSize );
      frameSizeComboBox->setCurrentIndex(idx);
      QObject::connect(frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotVideoControlChanged()));

      // Set new size
      setFrameSize(newSize);

      // Check if the new resolution changed the number of frames in the sequence
      emit signalGetFrameLimits();
      
      // Set the current frame in the buffer to be invalid 
      currentFrameIdx = -1;

      // Clear the cache
      pixmapCache.clear();

      // emit the signal that something has changed
      emit signalHandlerChanged(true, true);
    }
  }
  else if (sender == frameSizeComboBox)
  {
    QSize newSize = presetFrameSizes.getSize( frameSizeComboBox->currentIndex() );
    if (newSize != frameSize && newSize != QSize(-1,-1))
    {
      // Set the new size and update the controls.
      setFrameSize(newSize);

      // Check if the new resolution changed the number of frames in the sequence
      emit signalGetFrameLimits();
      
      // Set the current frame in the buffer to be invalid 
      currentFrameIdx = -1;

      // Clear the cache
      pixmapCache.clear();
      
      // emit the signal that something has changed
      emit signalHandlerChanged(true, true);
    }
  }
  else if (sender == startSpinBox ||
           sender == endSpinBox ||
           sender == rateSpinBox ||
           sender == samplingSpinBox )
  {
    if (sender == startSpinBox || sender == endSpinBox )
      startEndFrameChanged = true;

    startEndFrame.first  = startSpinBox->value();
    startEndFrame.second = endSpinBox->value();
    frameRate = rateSpinBox->value();
    sampling  = samplingSpinBox->value();

    if (startEndFrameChanged)
    {
      // Throw all frames outside of the new range out of the cache.
      for (int i=0; i<startEndFrame.first; i++)
        if (pixmapCache.contains(i))
          pixmapCache.remove(i);
      for (int i=startEndFrame.second+1; i=startEndFrameLimit.second; i++)
        if (pixmapCache.contains(i))
          pixmapCache.remove(i);
    }

    // The current frame in the buffer is not invalid, but emit that something has changed.
    // We also emit that the cache has changed.
    emit signalHandlerChanged(false, startEndFrameChanged);
  }
}

void videoHandler::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != currentFrameIdx)
  {
    // TODO: This has to be done differently.

    // The current buffer is out of date. Update it.
    if (pixmapCache.contains(frameIdx))
    {
      // The frame is buffered
      currentFrame = pixmapCache[frameIdx];
      currentFrameIdx = frameIdx;

      // TODO: Now the yuv values that will be shown using the getPixel(...) function is wrong.
    }
    else
    {
      // Frame not in buffer. Load it.
      loadFrame( frameIdx );

      if (frameIdx != currentFrameIdx)
        // Loading failed ...
        return;
    }
  }

  // Create the video rect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize( frameSize * zoomFactor );
  videoRect.moveCenter( QPoint(0,0) );

  // Draw the current image ( currentFrame )
  painter->drawPixmap( videoRect, currentFrame );

  if (zoomFactor >= 64)
  {
    // Draw the pixel values onto the pixels

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

    drawPixelValues(painter, xMin, xMax, yMin, yMax, zoomFactor);
  }
}

void videoHandler::drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor, videoHandler *item2)
{
  // The center point of the pixel (0,0).
  QPoint centerPointZero = ( QPoint(-frameSize.width(), -frameSize.height()) * zoomFactor + QPoint(zoomFactor,zoomFactor) ) / 2;
  // This rect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize( QSize(zoomFactor, zoomFactor) );
  for (unsigned int x = xMin; x <= xMax; x++)
  {
    for (unsigned int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor)) and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);
     
      // Get the text to show
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
      }
      else
        pixVal = getPixelVal(x, y);
      QString valText = QString("R%1\nG%2\nB%3").arg(qRed(pixVal)).arg(qGreen(pixVal)).arg(qBlue(pixVal));
           
      painter->setPen( (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128) ? Qt::white : Qt::black );
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QPixmap videoHandler::calculateDifference(videoHandler *item2, int frame, QList<infoItem> &differenceInfoList, int amplificationFactor, bool markDifference)
{
  // TODO: Add amplification factor and marking of differences

  // Load the right images, if not already loaded)
  if (currentFrameIdx != frame)
    loadFrame(frame);
  loadFrame(frame);
  if (item2->currentFrameIdx != frame)
    item2->loadFrame(frame);

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

      int r = clip( 128 + dR, 0, 255);
      int g = clip( 128 + dG, 0, 255);
      int b = clip( 128 + dB, 0, 255);

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

ValuePairList videoHandler::getPixelValuesDifference(QPoint pixelPos, videoHandler *item2)
{
  int width  = qMin(frameSize.width(), item2->frameSize.width());
  int height = qMin(frameSize.height(), item2->frameSize.height());

  if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
    return ValuePairList();

  QRgb pixel1 = getPixelVal( pixelPos );
  QRgb pixel2 = item2->getPixelVal( pixelPos );

  int r = qRed(pixel1) - qRed(pixel2);
  int g = qGreen(pixel1) - qGreen(pixel2);
  int b = qBlue(pixel1) - qBlue(pixel2);

  ValuePairList values("Difference Values (A,B,A-B)");

  values.append( ValuePair("R", QString("%1,%2,%3").arg(qRed(pixel1)).arg(qRed(pixel2)).arg(r)) );
  values.append( ValuePair("G", QString("%1,%2,%3").arg(qGreen(pixel1)).arg(qGreen(pixel2)).arg(g)) );
  values.append( ValuePair("B", QString("%1,%2,%3").arg(qBlue(pixel1)).arg(qBlue(pixel2)).arg(b)) );

  return values;
}

QRgb videoHandler::getPixelVal(QPoint pixelPos)
{
  if (currentFrame_Image_FrameIdx != currentFrameIdx)
    currentFrame_Image = currentFrame.toImage();

  return currentFrame_Image.pixel( pixelPos );
}

QRgb videoHandler::getPixelVal(int x, int y)
{
  if (currentFrame_Image_FrameIdx != currentFrameIdx)
    currentFrame_Image = currentFrame.toImage();

  return currentFrame_Image.pixel( x, y );
}

// Put the frame into the cache (if it is not already in there)
void videoHandler::cacheFrame(int frameIdx)
{
  if (pixmapCache.contains(frameIdx))  
    // No need to add it again
    return;

  // Load the frame. While this is happending in the background the frame size must not change.
  QPixmap cachePixmap;
  cachingFrameSizeMutex.lock();
  loadFrameForCaching(frameIdx, cachePixmap);
  cachingFrameSizeMutex.unlock();

  // Put it into the cache
  pixmapCache.insert(frameIdx, cachePixmap);

  // We will emit a signalHandlerChanged(false) if a frame was cached but we don't want to emit one signal for every 
  // frame. This is just not necessary. We limit the number of signals to one per second.
  if (!cachingTimer.isActive())
  {
    // Start the timer (one shot, 1s).
    // When the timer runs out an signalHandlerChanged(false) signal will be emitted.
    cachingTimer.setSingleShot(true);
    cachingTimer.setInterval(1000);
    emit cachingTimerStart();
  }
}

void videoHandler::removeFrameFromCache(int frameIdx)
{
  qDebug() << "removeFrameFromCache " << frameIdx;
}

void videoHandler::cachingTimerEvent()
{
  emit signalHandlerChanged(false, false);
}

// Compute the MSE between the given char sources for numPixels bytes
float videoHandler::computeMSE( unsigned char *ptr, unsigned char *ptr2, int numPixels ) const
{
  float mse=0.0;

  if( numPixels > 0 )
  {
    for(int i=0; i<numPixels; i++)
    {
      float diff = (float)ptr[i] - (float)ptr2[i];
      mse += diff*diff;
    }

    /* normalize on correlated pixels */
    mse /= (float)(numPixels);
  }

  return mse;
}

ValuePairList videoHandler::getPixelValues(QPoint pixelPos)
{
  // Get the RGB values from the pixmap
  if (!currentFrame)
    return ValuePairList();

  ValuePairList values;

  QRgb val = getPixelVal(pixelPos);
  values.append( ValuePair("R", QString::number(qRed(val))) );
  values.append( ValuePair("G", QString::number(qGreen(val))) );
  values.append( ValuePair("B", QString::number(qBlue(val))) );

  return values;
}