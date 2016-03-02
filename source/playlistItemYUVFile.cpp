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

playlistItemYUVFile::playlistItemYUVFile(QString yuvFilePath, bool tryFormatGuess)
  : playlistItemYuvSource(yuvFilePath)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

  dataSource.openFile(yuvFilePath);

  if (!dataSource.isOk())
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

  startEndFrame.first = 0;
  startEndFrame.second = getNumberFrames() - 1;

  cache->setCostPerFrame(getBytesPerYUVFrame()>>10);
}

playlistItemYUVFile::~playlistItemYUVFile()
{

}

qint64 playlistItemYUVFile::getNumberFrames()
{
  if (!dataSource.isOk() || !isFormatValid())
  {
    // File could not be loaded or there is no valid format set (width/height/yuvFormat)
    return 0;
  }

  // The file was opened successfully
  qint64 bpf = getBytesPerYUVFrame();
  return (bpf == 0) ? -1 : dataSource.getFileSize() / bpf;
}

QList<infoItem> playlistItemYUVFile::getInfoList()
{
  QList<infoItem> infoList;

  // At first append the file information part (path, date created, file size...)
  infoList.append(dataSource.getFileInfoList());

  infoList.append(infoItem("Num Frames", QString::number(getNumberFrames())));
  infoList.append(infoItem("Bytes per Frame", QString("%1").arg(getBytesPerYUVFrame())));

  if (dataSource.isOk() && isFormatValid())
  {
    // Check if the size of the file and the number of bytes per frame can be divided
    // without any remainder. If not, then there is probably something wrong with the
    // selected YUV format / width / height ...

    qint64 bpf = getBytesPerYUVFrame();
    if ((dataSource.getFileSize() % bpf) != 0)
    {
      // Add a warning
      infoList.append(infoItem("Warning", "The file size and the given video size and/or YUV format do not match."));
    }
  }
  infoList.append(infoItem("Frames Cached",QString::number(cache->getCacheSize())));

  return infoList;
}

void playlistItemYUVFile::setFormatFromFileName()
{
  int width, height, rate, bitDepth, subFormat;
  dataSource.formatFromFilename(width, height, rate, bitDepth, subFormat);
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
        if (bpf != 0 && (dataSource.getFileSize() % bpf) == 0)
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
        if (bpf != 0 && (dataSource.getFileSize() % bpf) == 0)
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

  if(dataSource.getFileSize() < 1)
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
  qint64 fileSize = dataSource.getFileSize();
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
      dataSource.readBytes(yuvBytes, 0, picSize*2);

      // assumptions: YUV is planar (can be changed if necessary)
      // only check mse in luminance
      ptr  = (unsigned char*) yuvBytes.data();
      candidateModes[i].mseY = computeMSE( ptr, ptr + picSize, picSize );
    }
    i++;
  };

  // step3: select best candidate
  i=0;
  leastMSE = std::numeric_limits<float>::max(); // large error...
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
  if (propertiesWidget->objectName().isEmpty())
    propertiesWidget->setObjectName(QStringLiteral("playlistItemYUVFile"));

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout(propertiesWidget);
  vAllLaout->setContentsMargins( 0, 0, 0, 0 );

  QFrame *line = new QFrame(propertiesWidget);
  line->setObjectName(QStringLiteral("line"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);

  // First add the parents controls (first video controls (width/height...) then yuv controls (format,...)
  vAllLaout->addLayout( playlistItemVideo::createVideoControls() );
  vAllLaout->addWidget( line );
  vAllLaout->addLayout( playlistItemYuvSource::createVideoControls() );

  // Insert a stretch at the bottom of the vertical global layout so that everything
  // gets 'pushed' to the top
  vAllLaout->insertStretch(3, 1);

  // Set the layout and add widget
  propertiesWidget->setLayout( vAllLaout );
}

void playlistItemYUVFile::savePlaylist(QDomElement &root, QDir playlistDir)
{
  // Determine the relative path to the yuv file. We save both in the playlist.
  QUrl fileURL( dataSource.getAbsoluteFilePath() );
  fileURL.setScheme("file");
  QString relativePath = playlistDir.relativeFilePath( dataSource.getAbsoluteFilePath() );

  QDomElementYUV d = root.ownerDocument().createElement("playlistItemYUVFile");
  
  // Apppend all the properties of the yuv file (the path to the file. Relative and absolute)
  d.appendProperiteChild( "absolutePath", fileURL.toString() );
  d.appendProperiteChild( "relativePath", relativePath  );
  
  // Now go up the inheritance hierarchie and append the properties of the base classes
  playlistItemYuvSource::appendItemProperties(d);
    
  root.appendChild(d);
}

/* Parse the playlist and return a new playlistItemYUVFile.
*/
playlistItemYUVFile *playlistItemYUVFile::newplaylistItemYUVFile(QDomElementYUV root, QString playlistFilePath)
{
  // Parse the dom element. It should have all values of a playlistItemYUVFile
  QString absolutePath = root.findChildValue("absolutePath");
  QString relativePath = root.findChildValue("relativePath");
  
  // check if file with absolute path exists, otherwise check relative path
  QFileInfo checkAbsoluteFile(absolutePath);
  if (!checkAbsoluteFile.exists())
  {
    QFileInfo plFileInfo(playlistFilePath);
    QString combinePath = QDir(plFileInfo.path()).filePath(relativePath);
    QFileInfo checkRelativeFile(combinePath);
    if (checkRelativeFile.exists() && checkRelativeFile.isFile())
    {
      absolutePath = QDir::cleanPath(combinePath);
    }
  }

  // We can still not be sure that the file really exists, but we gave our best to try to find it.
  playlistItemYUVFile *newFile = new playlistItemYUVFile(absolutePath, false);

  // Walk up the inhritance tree and let the base classes parse their properites from the file
  newFile->parseProperties(root);
  
  return newFile;
}

void playlistItemYUVFile::loadFrame(int frameIdx)
{
  CacheIdx cIdx = CacheIdx(dataSource.absoluteFilePath(),frameIdx);
  QPixmap* cachedFrame;
  if (!cache->readFromCache(cIdx,cachedFrame))
    {
      cachedFrame = new QPixmap();
      // Load one frame in YUV format
      qint64 fileStartPos = frameIdx * getBytesPerYUVFrame();
      mutex.lock();
      dataSource.readBytes( tempYUVFrameBuffer, fileStartPos, getBytesPerYUVFrame() );
      // Convert one frame from YUV to RGB
      convertYUVBufferToPixmap( tempYUVFrameBuffer, *cachedFrame );
      cache->addToCache(cIdx,cachedFrame);
      mutex.unlock();
    }
  currentFrame = *cachedFrame;
  currentFrameIdx = frameIdx;
}

bool playlistItemYUVFile::loadIntoCache(int frameIdx)
{
  CacheIdx cIdx = CacheIdx(dataSource.absoluteFilePath(),frameIdx);
  QPixmap* cachedFrame;
  bool frameIsInCache = false;
  if (!cache->readFromCache(cIdx,cachedFrame))
    {
      frameIsInCache = true;
      cachedFrame = new QPixmap();
      // Load one frame in YUV format
      qint64 fileStartPos = frameIdx * getBytesPerYUVFrame();
      mutex.lock();
      dataSource.readBytes( tempYUVFrameBuffer, fileStartPos, getBytesPerYUVFrame() );
      // Convert one frame from YUV to RGB
      convertYUVBufferToPixmap( tempYUVFrameBuffer, *cachedFrame );
      cache->addToCache(cIdx,cachedFrame);
      mutex.unlock();
    }
  return frameIsInCache;
}

void playlistItemYUVFile::removeFromCache(indexRange range)
{
  for (int frameIdx = range.first;frameIdx<=range.second;frameIdx++)
    {
      CacheIdx cIdx = CacheIdx(dataSource.absoluteFilePath(),frameIdx);
      cache->removeFromCache(cIdx);
    }
}

void playlistItemYUVFile::getPixelValue(QPoint pixelPos, unsigned int &Y, unsigned int &U, unsigned int &V)
{
  // Get the YUV data from the tmpBufferOriginal
  const quint64 frameOffset = currentFrameIdx * getBytesPerYUVFrame();
  const unsigned int offsetCoordinateY  = frameSize.width() * pixelPos.y() + pixelPos.x();
  const unsigned int offsetCoordinateUV = (frameSize.width() / srcPixelFormat.subsamplingHorizontal * pixelPos.y() / srcPixelFormat.subsamplingVertical) + pixelPos.x() / srcPixelFormat.subsamplingHorizontal;
  const unsigned int planeLengthY  = frameSize.width() * frameSize.height();
  const unsigned int planeLengthUV = frameSize.width() / srcPixelFormat.subsamplingHorizontal * frameSize.height() / srcPixelFormat.subsamplingVertical;
  if (srcPixelFormat.bitsPerSample > 8)
  {
    // TODO: Test for 10-bit. This is probably wrong.

    // Two bytes per value
    QByteArray vals;

    // Read Y value
    dataSource.readBytes(vals, frameOffset + offsetCoordinateY, 2);
    Y = (vals[0] << 8) + vals[1];

    // Read U value
    dataSource.readBytes(vals, frameOffset + planeLengthY + offsetCoordinateUV, 2);
    U = (vals[0] << 8) + vals[1];

    // Read V value
    dataSource.readBytes(vals, frameOffset + planeLengthY + planeLengthUV + offsetCoordinateUV, 2);
    V = (vals[0] << 8) + vals[1];
  }
  else
  {
    // One byte per value
    QByteArray vals;
    // Read Y value
    dataSource.readBytes(vals, frameOffset + offsetCoordinateY, 1);
    Y = (unsigned char)vals[0];

    // Read U value
    dataSource.readBytes(vals, frameOffset + planeLengthY + offsetCoordinateUV, 1);
    U = (unsigned char)vals[0]; 

    // Read V value
    dataSource.readBytes(vals, frameOffset + planeLengthY + planeLengthUV + offsetCoordinateUV, 1);
    V = (unsigned char)vals[0];

  }
}