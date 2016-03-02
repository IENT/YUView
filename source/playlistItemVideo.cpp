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

#include "playlistItemVideo.h"

// ------ Initialize the static list of frame size presets ----------

playlistItemVideo::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size" << "QCIF" << "QVGA" << "WQVGA" << "CIF" << "VGA" << "WVGA" << "4CIF" << "ITU R.BT601" << "720i/p" << "1080i/p" << "4k" << "XGA" << "XGA+";
  sizes << QSize(-1,-1) << QSize(176,144) << QSize(320, 240) << QSize(416, 240) << QSize(352, 288) << QSize(640, 480) << QSize(832, 480) << QSize(704, 576) << QSize(720, 576) << QSize(1280, 720) << QSize(1920, 1080) << QSize(3840, 2160) << QSize(1024, 768) << QSize(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList playlistItemVideo::frameSizePresetList::getFormatedNames()
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
playlistItemVideo::frameSizePresetList playlistItemVideo::presetFrameSizes;

// --------- playlistItemVideo -------------------------------------

playlistItemVideo::playlistItemVideo(QString itemNameOrFileName) : playlistItem(itemNameOrFileName)
{
  // Init variables
  frameRate = DEFAULT_FRAMERATE;
  startEndFrame = indexRange(-1,-1);
  sampling = 1;
  currentFrameIdx = -1;
  currentFrame_Image_FrameIdx = -1;

  // create a cache object and a thread, move the object onto
  // the thread and start the thread

  cache = new videoCache(this);
  cacheThread = new QThread;
  cache->moveToThread(cacheThread);
  cacheThread->start();
}

playlistItemVideo::~playlistItemVideo()
{
  //TODO: this is not nice yet
  while (cache->isCacheRunning())
    {
      stopCaching();
    }

  // Kill the thread, kill it NAU!!!
 while (cacheThread != NULL && cacheThread->isRunning())
   {
     cacheThread->exit();
   }
 // cleanup
 cache->clearCache();
 cache->deleteLater();
 cacheThread->deleteLater();
}

QLayout *playlistItemVideo::createVideoControls(bool isSizeFixed)
{
  setupUi(propertiesWidget);

  if (startEndFrame == indexRange(-1,-1))
  {
    startEndFrame.first = 0;
    startEndFrame.second = getNumberFrames() - 1;
  }

  // Set default values
  widthSpinBox->setMaximum(100000);
  widthSpinBox->setValue( frameSize.width() );
  widthSpinBox->setEnabled( !isSizeFixed );
  heightSpinBox->setMaximum(100000);
  heightSpinBox->setValue( frameSize.height() );
  heightSpinBox->setEnabled( !isSizeFixed );
  startSpinBox->setValue( startEndFrame.first );
  endSpinBox->setMaximum( getNumberFrames() - 1 );
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

  return playlistItemVideoLayout;
}

void playlistItemVideo::setFrameSize(QSize newSize, bool emitSignal)
{
  // Set the new size
  frameSize = newSize;

  if (!propertiesWidgetCreated())
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

void playlistItemVideo::setStartEndFrame(indexRange range, bool emitSignal)
{
  // Set the new start/end frame
  startEndFrame = range;

  if (!propertiesWidgetCreated())
    // spin boxes not created yet
    return;

  if (!emitSignal)
  {
    QObject::disconnect(startSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
    QObject::disconnect(endSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
  }

  startSpinBox->setValue( range.first );
  endSpinBox->setMaximum( getNumberFrames() - 1 );
  endSpinBox->setValue( range.second );

  if (!emitSignal)
  {
    connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
    connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
  }
}

void playlistItemVideo::slotVideoControlChanged()
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
      frameSize = newSize;

      // Check if the new resolution changed the number of frames in the sequence
      if (endSpinBox->maximum() != (getNumberFrames() - 1))
      {
        // Adjust the endSpinBox maximum and the current value of endSpinBox (if necessary).
        // If the current value is changed another event will be triggered that will update startEndFrame.
        endSpinBox->setMaximum( getNumberFrames() - 1 );
        if (endSpinBox->value() >= getNumberFrames())
          endSpinBox->setValue( getNumberFrames() );
      }

      // Set the current frame in the buffer to be invalid and emit the signal that something has changed
      currentFrameIdx = -1;
      emit signalItemChanged(true);
      //qDebug() << "Emit Redraw";
    }
  }
  else if (sender == frameSizeComboBox)
  {
    QSize newSize = presetFrameSizes.getSize( frameSizeComboBox->currentIndex() );
    if (newSize != frameSize && newSize != QSize(-1,-1))
    {
      // Set the new size and update the controls.
      setFrameSize(newSize);

      // Set the current frame in the buffer to be invalid and emit the signal that something has changed
      currentFrameIdx = -1;
      emit signalItemChanged(true);
      //qDebug() << "Emit Redraw";
    }
  }
  else if (sender == startSpinBox ||
           sender == endSpinBox ||
           sender == rateSpinBox ||
           sender == samplingSpinBox )
  {
    startEndFrame.first  = startSpinBox->value();
    startEndFrame.second = endSpinBox->value();
    frameRate = rateSpinBox->value();
    sampling  = samplingSpinBox->value();

    // The current frame in the buffer is not invalid, but emit that something has changed.
    emit signalItemChanged(false);
  }
}

void playlistItemVideo::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != currentFrameIdx)
  {
    // The current buffer is out of date. Update it.
    loadFrame( frameIdx );

    if (frameIdx != currentFrameIdx)
      // Loading failed ...
      return;
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

void playlistItemVideo::drawPixelValues(QPainter *painter, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax, double zoomFactor)
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
      QRgb pixVal = getPixelVal(x, y);
      QString valText = QString("R%1\nG%2\nB%3").arg(qRed(pixVal)).arg(qGreen(pixVal)).arg(qBlue(pixVal));
           
      painter->setPen( (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128) ? Qt::white : Qt::black );
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QPixmap playlistItemVideo::calculateDifference(playlistItemVideo *item2, int frame, QList<infoItem> &conversionInfoList)
{
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

  conversionInfoList.append( infoItem("Difference Type","RGB") );
  
  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  conversionInfoList.append( infoItem("MSE R",QString("%1").arg(mse[0])) );
  conversionInfoList.append( infoItem("MSE G",QString("%1").arg(mse[1])) );
  conversionInfoList.append( infoItem("MSE B",QString("%1").arg(mse[2])) );
  conversionInfoList.append( infoItem("MSE All",QString("%1").arg(mse[3])) );

  return QPixmap::fromImage(diffImg);
}

ValuePairList playlistItemVideo::getPixelValuesDifference(playlistItemVideo *item2, QPoint pixelPos)
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

QRgb playlistItemVideo::getPixelVal(QPoint pixelPos)
{
  if (currentFrame_Image_FrameIdx != currentFrameIdx)
    currentFrame_Image = currentFrame.toImage();

  return currentFrame_Image.pixel( pixelPos );
}

QRgb playlistItemVideo::getPixelVal(int x, int y)
{
  if (currentFrame_Image_FrameIdx != currentFrameIdx)
    currentFrame_Image = currentFrame.toImage();

  return currentFrame_Image.pixel( x, y );
}

void playlistItemVideo::startCaching(indexRange range)
{
  // add a job to the queue
  cache->addRangeToQueue(range);

  // as cache is now moved onto another thread, use invokeMethod to
  // start the worker. This ensures that our thread is persistent and
  // not deleted after completion of the run routine

  // if the worker is not running, restart it
  if (!cache->isCacheRunning())
  {
    QMetaObject::invokeMethod(cache, "run", Qt::QueuedConnection);
  }
}

// TODO: where to call this
void playlistItemVideo::stopCaching()
{
  while (cache->isCacheRunning())
    cache->cancelCaching();
}

bool playlistItemVideo::isCaching()
{
   return cache->isCacheRunning();
}

// Append all properties from the playlistItemVideo to the QDomElement
void playlistItemVideo::appendItemProperties(QDomElementYUV &root)
{

  root.appendProperiteChild( "width", QString::number(frameSize.width()) );
  root.appendProperiteChild( "height", QString::number(frameSize.height()) );
  root.appendProperiteChild( "startFrame", QString::number(startEndFrame.first) );
  root.appendProperiteChild( "endFrame", QString::number(startEndFrame.second) );
  root.appendProperiteChild( "sampling", QString::number(sampling) );
  root.appendProperiteChild( "frameRate", QString::number(frameRate) );
}

void playlistItemVideo::parseProperties(QDomElementYUV root)
{
  int width = root.findChildValue("width").toInt();
  int height = root.findChildValue("height").toInt();
  int startFrame = root.findChildValue("startFrame").toInt();
  int endFrame = root.findChildValue("endFrame").toInt();
  
  frameSize = QSize(width, height);
  startEndFrame = indexRange(startFrame, endFrame);
}