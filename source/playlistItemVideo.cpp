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
  heightSpinBox->setMaximum(100000);
  heightSpinBox->setValue( frameSize.height() );
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
      // Set the width/height spin boxes without emitting another signal (disconnect/set/reconnect)
      QObject::disconnect(widthSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
      QObject::disconnect(heightSpinBox, SIGNAL(valueChanged(int)), NULL, NULL);
      widthSpinBox->setValue( newSize.width() );
      heightSpinBox->setValue( newSize.height() );
      QObject::connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));
      QObject::connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotVideoControlChanged()));

      // Set the new size
      frameSize = newSize;

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
