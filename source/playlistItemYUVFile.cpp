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

#include "playlistItemYUVFile.h"
#include "typedef.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QUrl>
#include <QTime>
#include <QDebug>
#include <QPainter>

// TODO: REMOVE
int playlistItemYUVFile::randomColorStat = 0;

// Compute the MSE between the given char sources for numPixels bytes
float computeMSE( unsigned char *ptr, unsigned char *ptr2, int numPixels )
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

playlistItemYUVFile::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size" << "QCIF" << "QVGA" << "WQVGA" << "CIF" << "VGA" << "WVGA" << "4CIF" << "ITU R.BT601" << "720i/p" << "1080i/p" << "4k" << "XGA" << "XGA+";
  sizes << QSize(-1,-1) << QSize(176,144) << QSize(320, 240) << QSize(416, 240) << QSize(352, 288) << QSize(640, 480) << QSize(832, 480) << QSize(704, 576) << QSize(720, 576) << QSize(1280, 720) << QSize(1920, 1080) << QSize(3840, 2160) << QSize(1024, 768) << QSize(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList playlistItemYUVFile::frameSizePresetList::getFormatedNames()
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
playlistItemYUVFile::frameSizePresetList playlistItemYUVFile::presetFrameSizes;

playlistItemYUVFile::playlistItemYUVFile(QString yuvFilePath, bool tryFormatGuess)
  : playlistItem(yuvFilePath)
  , fileSource(yuvFilePath)
  , yuvSource()
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  // Init variables
  frameRate = DEFAULT_FRAMERATE;
  startFrame = 0;
  sampling = 1;
  currentFrameIdx = -1;
  
  if (!isFileOk())
    // Opening the file failed.
    return;
  
  if (!tryFormatGuess)
    // Do not try to guess the format from the file
    return;

  // Try to get the frame format from the file name. The fileSource can guess this.
  setFormatFromFileName();
  
  if (!isFormatValid()) 
  {
    // Try to get the format from the correlation
    setFormatFromCorrelation();
  }

  endFrame = getNumberFrames() - 1;

  randomColor = randomColorStat;
  randomColorStat++;
}

playlistItemYUVFile::~playlistItemYUVFile()
{
}

int playlistItemYUVFile::getNumberFrames()
{
  if (!isFileOk() || !isFormatValid())
  {
    // File could not be loaded or there is no valid format set (width/height/yuvFormat)
    return 0;
  }

  // The file was opened successfully
  qint64 bpf = getBytesPerYUVFrame();
  return (bpf == 0) ? -1 : getFileSize() / bpf;
}

QList<infoItem> playlistItemYUVFile::getInfoList()
{
  QList<infoItem> infoList;

  // At first append the file information part (path, date created, file size...)
  infoList.append(getFileInfoList());

  infoList.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  //infoList.append(infoItem("Status", getStatusAndInfo()));

  return infoList;
}

void playlistItemYUVFile::setFormatFromFileName()
{
  int width, height, rate, bitDepth, subFormat;
  formatFromFilename(width, height, rate, bitDepth, subFormat);
  QSize size(width, height);

  if(width > 0 && height > 0)
  {
    // We were able to extrace width and height from the file name using
    // regular expressions. Try to get the pixel format by checking with the file size.

    // If the bit depth could not be determined, check 8 and 10 bit
    int testBitDepths = (bitDepth > 0) ? 1 : 2;

    for (int i = 0; i < testBitDepths; i++)
    {
      if (testBitDepths == 2)
        bitDepth = (i == 0) ? 8 : 10;

      if (bitDepth==8)
      {
        // assume 4:2:0, 8bit
        yuvPixelFormat cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 8-bit planar" );
        int bpf = cFormat.bytesPerFrame( size );
        if (bpf != 0 && (getFileSize() % bpf) == 0) 
        {
          // Bits per frame and file size match
          frameSize = size;
          frameRate = rate;
          srcPixelFormat = cFormat;
          return;
        }
      }
      else if (bitDepth==10)
      {
        // Assume 444 format if subFormat is set. Otherwise assume 420
        yuvPixelFormat cFormat;
        if (subFormat == 444)
          cFormat = yuvFormatList.getFromName( "4:4:4 Y'CbCr 10-bit LE planar" );
        else
          cFormat = yuvFormatList.getFromName( "4:2:0 Y'CbCr 10-bit LE planar" );

        int bpf = cFormat.bytesPerFrame( size );
        if (bpf != 0 && (getFileSize() % bpf) == 0) 
        {
          // Bits per frame and file size match
          frameSize = size;
          frameRate = rate;
          srcPixelFormat = cFormat;
          return;
        }
      }
      else
      {
          // do other stuff
      }
    }
  }
}

/** Try to guess the format of the file. A list of candidates is tried (candidateModes) and it is checked if 
  * the file size matches and if the correlation of the first two frames is below a threshold.
  */
void playlistItemYUVFile::setFormatFromCorrelation()
{
  unsigned char *ptr;
  float leastMSE, mse;
  int   bestMode;

  // step1: file size must be a multiple of w*h*(color format)
  qint64 picSize;

  if(getFileSize() < 1)
      return;

  // Define the structure for each candidate mode
  // The definition is here to not pollute any other namespace unnecessarily
  typedef struct {
    QSize   frameSize;
    QString pixelFormatName;

    // flags set while checking
    bool  interesting;
    float mseY;
  } candMode_t;

  // Fill the list of possible candidate modes
  candMode_t candidateModes[] = {
    {QSize(176,144),"4:2:0 Y'CbCr 8-bit planar",false, 0.0 },
    {QSize(352,240),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,288),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,480),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,576),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1024,768),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,720),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,960),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1072),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1080),"4:2:0 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(176,144),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,240),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(352,288),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(480,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,480),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,486),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(704,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(720,576),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1024,768),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,720),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1280,960),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1072),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(1920,1080),"4:2:2 Y'CbCr 8-bit planar", false, 0.0 },
    {QSize(), "Unknown Pixel Format", false, 0.0 }
  };

  // if any candidate exceeds file size for two frames, discard
  // if any candidate does not represent a multiple of file size, discard
  int i = 0;
  bool found = false;
  qint64 fileSize = getFileSize();
  while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
  {
    /* one pic in bytes */
    yuvPixelFormat pixelFormat = yuvFormatList.getFromName( candidateModes[i].pixelFormatName );
    picSize = pixelFormat.bytesPerFrame( candidateModes[i].frameSize );

    if( fileSize >= (picSize*2) )    // at least 2 pics for correlation analysis
    {
      if( (fileSize % picSize) == 0 ) // important: file size must be multiple of pic size
      {
        candidateModes[i].interesting = true; // test passed
        found = true;
      }
    }

    i++;
  };

  if( !found  ) // no proper candidate mode ?
    return;

  // step2: calculate max. correlation for first two frames, use max. candidate frame size
  i=0;
  QByteArray yuvBytes;
  while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
  {
    if( candidateModes[i].interesting )
    {
      yuvPixelFormat pixelFormat = yuvFormatList.getFromName( candidateModes[i].pixelFormatName );
      picSize = pixelFormat.bytesPerFrame( candidateModes[i].frameSize );

      // Read two frames into the buffer
      readBytes(yuvBytes, 0, picSize*2);

      // assumptions: YUV is planar (can be changed if necessary)
      // only check mse in luminance
      ptr  = (unsigned char*) yuvBytes.data();
      candidateModes[i].mseY = computeMSE( ptr, ptr + picSize, picSize );
    }
    i++;
  };

  // step3: select best candidate
  i=0;
  leastMSE = FLT_MAX; // large error...
  bestMode = 0;
  while( candidateModes[i].pixelFormatName != "Unknown Pixel Format" )
  {
    if( candidateModes[i].interesting )
    {
      mse = (candidateModes[i].mseY);
      if( mse < leastMSE )
      {
        bestMode = i;
        leastMSE = mse;
      }
    }
    i++;
  };

  if( leastMSE < 400 )
  {
    // MSE is below threshold. Choose the candidate.
    srcPixelFormat = yuvFormatList.getFromName( candidateModes[bestMode].pixelFormatName );
    frameSize = candidateModes[bestMode].frameSize;
  }
}

void playlistItemYUVFile::createPropertiesWidget( )
{
  // Absolutely always only call this once
  assert( propertiesWidget == NULL );

  // Create a new widget and populate it with controls
  propertiesWidget = new QWidget;

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout;
  vAllLaout->setContentsMargins( 0, 0, 0, 0 );

  // Create the grid layout that contains width, height, start, stop, rate, sampling
  QGridLayout *topGrid = new QGridLayout;
  vAllLaout->addLayout( topGrid );
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

  // Create the grid that contains the frame size-, yuv format-, color components-, 
  // chroma interpolation- and color converion combo boxes
  QGridLayout *midGrid = new QGridLayout;
  vAllLaout->addLayout( midGrid );
  midGrid->setContentsMargins( 0, 0, 0, 0 );

  // Create/add controls
  midGrid->addWidget( new QLabel("Frame Size", propertiesWidget), 3, 0);
  frameSizeComboBox = new QComboBox( propertiesWidget );
  frameSizeComboBox->addItems( presetFrameSizes.getFormatedNames() );
  midGrid->addWidget( frameSizeComboBox, 3, 1 );

  midGrid->addWidget( new QLabel("YUV File Format", propertiesWidget), 4, 0);
  yuvFileFormatComboBox = new QComboBox( propertiesWidget );
  yuvFileFormatComboBox->addItems( yuvFormatList.getFormatedNames() );
  midGrid->addWidget( yuvFileFormatComboBox, 4, 1 );

  midGrid->addWidget( new QLabel("Color Components", propertiesWidget), 5, 0);
  colorComponentsComboBox = new QComboBox( propertiesWidget );
  colorComponentsComboBox->addItems( QStringList() << "Y'CbCr" << "Luma Only" << "Cb only" << "Cr only" );
  midGrid->addWidget( colorComponentsComboBox, 5, 1 );

  midGrid->addWidget( new QLabel("Chroma Interpolation", propertiesWidget), 6, 0);
  chromaInterpolationComboBox = new QComboBox( propertiesWidget );
  chromaInterpolationComboBox->addItems( QStringList() << "Nearest neighbour" << "Bilinear" );
  midGrid->addWidget( chromaInterpolationComboBox, 6, 1 );

  midGrid->addWidget( new QLabel("Color Conversion", propertiesWidget), 7, 0);
  colorConversionComboBox = new QComboBox( propertiesWidget );
  colorConversionComboBox->addItems( QStringList() << "ITU-R.BT709" << "ITU-R.BT601" << "ITU-R.BT202" );
  midGrid->addWidget( colorConversionComboBox, 7, 1 );

  // The loer horizontal layout that contains the Scale/Offset/Invert controls for Luma/Chroma
  QHBoxLayout *botLayout = new QHBoxLayout;
  vAllLaout->addLayout( botLayout );
  botLayout->setContentsMargins( 0, 0, 0, 0 );

  // Add left group box (Luma)
  QGroupBox *lumaGroup = new QGroupBox("Luma");
  botLayout->addWidget( lumaGroup );
  QGridLayout *lumaGroupLayout = new QGridLayout;
  lumaGroup->setLayout( lumaGroupLayout );
    
  lumaGroupLayout->addWidget( new QLabel("Scale", propertiesWidget), 1, 0 );
  lumaScaleSpinBox = new QSpinBox(propertiesWidget);
  lumaGroupLayout->addWidget( lumaScaleSpinBox, 1, 1 );
  lumaGroupLayout->addWidget( new QLabel("Offset", propertiesWidget), 2, 0 );
  lumaOffsetSpinBox = new QSpinBox(propertiesWidget);
  lumaOffsetSpinBox->setMaximum(1000);
  lumaGroupLayout->addWidget( lumaOffsetSpinBox, 2, 1 );
  lumaInvertCheckBox = new QCheckBox("Invert", propertiesWidget);
  lumaGroupLayout->addWidget( lumaInvertCheckBox );

  // Add right group box (Chroma)
  QGroupBox *chromaGroup = new QGroupBox("Chroma");
  botLayout->addWidget( chromaGroup );
  QGridLayout *chromaGroupLayout = new QGridLayout;
  chromaGroup->setLayout( chromaGroupLayout );

  chromaGroupLayout->addWidget( new QLabel("Scale", propertiesWidget), 1, 0 );
  chromaScaleSpinBox = new QSpinBox(propertiesWidget);
  chromaGroupLayout->addWidget( chromaScaleSpinBox, 1, 1 );
  chromaGroupLayout->addWidget( new QLabel("Offset", propertiesWidget), 2, 0 );
  chromaOffsetSpinBox = new QSpinBox(propertiesWidget);
  chromaOffsetSpinBox->setMaximum(1000);
  chromaGroupLayout->addWidget( chromaOffsetSpinBox, 2, 1 );
  chromaInvertCheckBox = new QCheckBox("Invert", propertiesWidget);
  chromaGroupLayout->addWidget( chromaInvertCheckBox );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
  
  // Set all the values of the properties widget to the values of this class
  widthSpinBox->setValue( frameSize.width() );
  heightSpinBox->setValue( frameSize.height() );
  startSpinBox->setValue( startFrame );
  endSpinBox->setMaximum( getNumberFrames() - 1 );
  endSpinBox->setValue( endFrame );
  rateSpinBox->setValue( frameRate );
  samplingSpinBox->setValue( sampling );
  int idx = presetFrameSizes.findSize( frameSize );
  frameSizeComboBox->setCurrentIndex(idx);
  idx = yuvFormatList.indexOf( srcPixelFormat );
  yuvFileFormatComboBox->setCurrentIndex( idx );
  colorComponentsComboBox->setCurrentIndex( (int)componentDisplayMode );
  chromaInterpolationComboBox->setCurrentIndex( (int)interpolationMode );
  colorConversionComboBox->setCurrentIndex( (int)yuvColorConversionType );
  lumaScaleSpinBox->setValue( lumaScale );
  lumaOffsetSpinBox->setValue( lumaOffset );
  lumaInvertCheckBox->setChecked( lumaInvert );
  chromaScaleSpinBox->setValue( chromaScale );
  chromaOffsetSpinBox->setValue( chromaOffset );
  chromaInvertCheckBox->setChecked( chromaInvert );

  // Connect all the change signals from the controls to "connectWidgetSignals()"
  // This is not done before because we don't want the 
  QObject::connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(startSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(endSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(rateSpinBox, SIGNAL(valueChanged(double)), this, SLOT(slotControlChanged()));
  QObject::connect(samplingSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(yuvFileFormatComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(colorComponentsComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(chromaInterpolationComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(colorConversionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(lumaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(lumaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(lumaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(chromaScaleSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(chromaOffsetSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
  QObject::connect(chromaInvertCheckBox, SIGNAL(stateChanged(int)), this, SLOT(slotControlChanged()));
}

void playlistItemYUVFile::slotControlChanged()
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
      QObject::connect(frameSizeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotControlChanged()));

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
      QObject::connect(widthSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));
      QObject::connect(heightSpinBox, SIGNAL(valueChanged(int)), this, SLOT(slotControlChanged()));

      // Set the new size
      frameSize = newSize;

      // Set the current frame in the buffer to be invalid and emit the signal that something has changed
      currentFrameIdx = -1;
      emit signalRedrawItem();
      //qDebug() << "Emit Redraw";
    }
  }
  else if (sender == colorComponentsComboBox || 
           sender == chromaInterpolationComboBox ||
           sender == colorConversionComboBox ||
           sender == lumaScaleSpinBox ||
           sender == lumaOffsetSpinBox ||
           sender == lumaInvertCheckBox ||
           sender == chromaScaleSpinBox ||
           sender == chromaOffsetSpinBox ||
           sender == chromaInvertCheckBox )
  {
    componentDisplayMode = (ComponentDisplayMode)colorComponentsComboBox->currentIndex();
    interpolationMode = (InterpolationMode)chromaInterpolationComboBox->currentIndex();
    yuvColorConversionType = (YUVCColorConversionType)colorConversionComboBox->currentIndex();
    lumaScale = lumaScaleSpinBox->value();
    lumaOffset = lumaOffsetSpinBox->value();
    lumaInvert = lumaInvertCheckBox->isChecked();
    chromaScale = chromaScaleSpinBox->value();
    chromaOffset = chromaOffsetSpinBox->value();
    chromaInvert = chromaInvertCheckBox->isChecked();

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentFrameIdx = -1;
    emit signalRedrawItem();
  }
  else if (sender == startSpinBox || 
           sender == endSpinBox ||
           sender == rateSpinBox ||
           sender == samplingSpinBox )
  {
    startFrame = startSpinBox->value();
    endFrame = endSpinBox->value();
    frameRate = rateSpinBox->value();
    sampling = samplingSpinBox->value();

    // The current frame in the buffer is not invalid, but emit that something has changed.
    emit signalRedrawItem();
  }
  else if (sender == yuvFileFormatComboBox)
  {
    srcPixelFormat = yuvFormatList.getFromName( yuvFileFormatComboBox->currentText() );

    // Set the current frame in the buffer to be invalid and emit the signal that something has changed
    currentFrameIdx = -1;
    emit signalRedrawItem();
  }
}

void playlistItemYUVFile::savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir)
{
  root.appendChild( createTextElement(doc, "key", "Class") );
  root.appendChild( createTextElement(doc, "string", "YUVFile") );
  root.appendChild( createTextElement(doc, "key", "Properties") );

  // Determine the relative path to the yuv file. We save both in the playlist.
  QUrl fileURL( getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( getAbsoluteFilePath() );

  QDomElement d = doc.createElement("dict");
  
  d.appendChild( createTextElement(doc, "key", "URL") );                 // <key>URL</key>
  d.appendChild( createTextElement(doc, "string", fileURL.toString()) ); // <string>file:///D/Kimono.yuv</string>
  
  d.appendChild( createTextElement(doc, "key", "endFrame") );                      // <key>endFrame</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(endFrame)) );   // <integer>240</integer>
  
  d.appendChild( createTextElement(doc, "key", "frameOffset") );                   // <key>frameOffset</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(startFrame)) ); // <integer>240</integer>
  
  d.appendChild( createTextElement(doc, "key", "frameSampling") );                 // <key>frameSampling</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(sampling)) );   // <integer>1</integer>
  
  QString rate; rate.setNum(frameRate);
  d.appendChild( createTextElement(doc, "key", "framerate") );  // <key>framerate</key>
  d.appendChild( createTextElement(doc, "real", rate) );        // <integer>1</integer>
  
  d.appendChild( createTextElement(doc, "key", "height") );                                // <key>height</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(frameSize.height())) ); // <integer>1080</integer>
  
  d.appendChild( createTextElement(doc, "key", "pixelFormat") );           // <key>pixelFormat</key>
  d.appendChild( createTextElement(doc, "string", srcPixelFormat.name) );  // <string>19</string>
  
  d.appendChild( createTextElement(doc, "key", "rURL") );                  // <key>rURL</key>
  d.appendChild( createTextElement(doc, "string", relativePath) );         // <string>../Kimono.yuv</string>
  
  d.appendChild( createTextElement(doc, "key", "width") );                                 // <key>width</key>
  d.appendChild( createTextElement(doc, "integer", QString::number(frameSize.width())) );  // <integer>1920</integer>
  
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemYUVFile.
*/
playlistItemYUVFile *playlistItemYUVFile::newplaylistItemYUVFile(QDomElement stringElement, QString playlistFilePath)
{
  // stringElement should be the <string>YUVFile</string> element
  assert(stringElement.text() == "YUVFile");

  QDomElement propertiesKey = stringElement.nextSiblingElement();
  if (propertiesKey.tagName() != QLatin1String("key") || propertiesKey.text() != "Properties")
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "<key>Properties</key> not found in YUVFile entry";
    return NULL;
  }

  QDomElement propertiesDict = propertiesKey.nextSiblingElement();
  if (propertiesDict.tagName() != QLatin1String("dict"))
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "<dict> not found in YUVFile properties entry";
    return NULL;
  }

  // Parse all the properties
  QDomElement it = propertiesDict.firstChildElement();

  QString filePath, pixelFormat, relativePath;
  int endFrame, startFrame, frameSampling, height, width;
  double framerate;
  try
  {
    filePath = parseStringFromPlaylist(it, "URL");
    endFrame = parseIntFromPlaylist(it, "endFrame");
    startFrame = parseIntFromPlaylist(it, "frameOffset");
    frameSampling = parseIntFromPlaylist(it, "frameSampling");
    framerate = parseDoubleFromPlaylist(it, "framerate");
    height = parseIntFromPlaylist(it, "height");
    pixelFormat = parseStringFromPlaylist(it, "pixelFormat");
    relativePath = parseStringFromPlaylist(it, "rURL");
    width = parseIntFromPlaylist(it, "width");
  }
  catch (parsingException err)
  {
    qDebug() << QTime::currentTime().toString("hh:mm:ss.zzz") << "Error parsing playlist file.";
    qDebug() << err;
    return NULL;
  }

  // Check the values 
  
  // check if file with absolute path exists, otherwise check relative path
  QFileInfo checkAbsoluteFile(filePath);
  if (!checkAbsoluteFile.exists())
  {
    QFileInfo plFileInfo(playlistFilePath);
    QString combinePath = QDir(plFileInfo.path()).filePath(relativePath);
    QFileInfo checkRelativeFile(combinePath);
    if (checkRelativeFile.exists() && checkRelativeFile.isFile())
    {
      filePath = QDir::cleanPath(combinePath);
    }
  }

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemYUVFile *newFile = new playlistItemYUVFile(filePath, false);
  newFile->endFrame = endFrame;
  newFile->startFrame = startFrame;
  newFile->sampling = frameSampling;
  newFile->frameRate = framerate;
  newFile->frameSize = QSize(width, height);
  newFile->srcPixelFormat = yuvFormatList.getFromName(pixelFormat);

  return newFile;
}

void playlistItemYUVFile::drawFrame(QPainter *painter, int frameIdx, double zoomFactor)
{
  // Check if the frameIdx changed and if we have to load a new frame
  if (frameIdx != currentFrameIdx)
  {
    currentFrameIdx = frameIdx;

    // Load one frame in YUV format
    qint64 fileStartPos = frameIdx * getBytesPerYUVFrame();
    readBytes( tempYUVFrameBuffer, fileStartPos, getBytesPerYUVFrame() );

    // Convert one frame from YUV to RGB
    convertYUVBufferToPixmap( tempYUVFrameBuffer, currentFrame );
  }

  // Create the video rect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize( frameSize * zoomFactor );
  videoRect.moveCenter( QPoint(0,0) );

  // Draw the current image ( currentFrame )
  painter->drawPixmap( videoRect, currentFrame );

  //QString frameStr = QString("Frame: %1\nName: %2").arg(frameIdx).arg(getName());

  //// For now instead of drawing the actual frame, draw a rect and put a text in the center
  //if (randomColor == 0)
  //  painter->fillRect(videoRect, QColor(128,128,0) );
  //else if (randomColor == 1)
  //  painter->fillRect(videoRect, QColor(128,0,128) );
  //else if (randomColor == 2)
  //  painter->fillRect(videoRect, QColor(0,128,128) );
  //else 
  //  painter->fillRect(videoRect, QColor(128,0,0) );

  //painter->drawText( videoRect, Qt::AlignCenter, frameStr);
}