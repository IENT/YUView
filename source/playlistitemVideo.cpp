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
  startEndFrame = indexRange(0,0);
  sampling = 1;
  currentFrameIdx = -1;
}

playlistItemVideo::~playlistItemVideo()
{
}

QLayout *playlistItemVideo::createVideoControls(bool isSizeFixed)
{
  // Create the grid layout that contains width, height, start, stop, rate, sampling
  topVBoxLayout = new QVBoxLayout();

  QGridLayout *topGrid = new QGridLayout;
  topVBoxLayout->addLayout(topGrid);
  topGrid->setContentsMargins( 0, 0, 0, 0 );

  // Create/add controls
  topGrid->addWidget( new QLabel("Width", propertiesWidget), 0, 0 );
  widthSpinBox = new QSpinBox(propertiesWidget);
  widthSpinBox->setMaximum(100000);
  topGrid->addWidget( widthSpinBox, 0, 1 );
  topGrid->addWidget( new QLabel("Height", propertiesWidget), 0, 2 );
  heightSpinBox = new QSpinBox(propertiesWidget);
  heightSpinBox->setMaximum(100000);
  topGrid->addWidget( heightSpinBox, 0, 3 );

  topGrid->addWidget( new QLabel("Start", propertiesWidget), 1, 0 );
  startSpinBox = new QSpinBox(propertiesWidget);
  topGrid->addWidget( startSpinBox, 1, 1 );
  topGrid->addWidget( new QLabel("End", propertiesWidget), 1, 2 );
  endSpinBox = new QSpinBox(propertiesWidget);
  topGrid->addWidget( endSpinBox, 1, 3 );

  topGrid->addWidget( new QLabel("Rate", propertiesWidget), 2, 0 );
  rateSpinBox = new QDoubleSpinBox(propertiesWidget);
  rateSpinBox->setMaximum(1000);
  topGrid->addWidget( rateSpinBox, 2, 1 );
  topGrid->addWidget( new QLabel("Sampling", propertiesWidget), 2, 2 );
  samplingSpinBox = new QSpinBox(propertiesWidget);
  samplingSpinBox->setMinimum(1);
  samplingSpinBox->setMaximum(100000);
  topGrid->addWidget( samplingSpinBox, 2, 3 );

  // The 
  QGridLayout *dropdownGrid = new QGridLayout();
  topVBoxLayout->addLayout(dropdownGrid);
  dropdownGrid->addWidget( new QLabel("Frame Size", propertiesWidget), 0, 0 );
  frameSizeComboBox = new QComboBox( propertiesWidget );
  frameSizeComboBox->addItems( presetFrameSizes.getFormatedNames() );
  dropdownGrid->addWidget( frameSizeComboBox, 0, 1 );

  // Set default values
  widthSpinBox->setValue( frameSize.width() );
  heightSpinBox->setValue( frameSize.height() );
  startSpinBox->setValue( startEndFrame.first );
  endSpinBox->setMaximum( getNumberFrames() - 1 );
  endSpinBox->setValue( startEndFrame.second );
  rateSpinBox->setValue( frameRate );
  samplingSpinBox->setValue( sampling );
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

  return topVBoxLayout;
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

      // Set the current frame in the buffer to be invalid and emit the signal that something has changed
      currentFrameIdx = -1;
      emit signalRedrawItem();
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
      emit signalRedrawItem();
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
    emit signalRedrawItem();
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