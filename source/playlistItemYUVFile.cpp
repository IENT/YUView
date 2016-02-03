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
  , fileSource(yuvFilePath)
{
  // Set the properties of the playlistItem
  setIcon(0, QIcon(":img_television.png"));
  setFlags(flags() | Qt::ItemIsDropEnabled);

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

  startEndFrame.second = getNumberFrames() - 1;
}

playlistItemYUVFile::~playlistItemYUVFile()
{
}

qint64 playlistItemYUVFile::getNumberFrames()
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

  // On the top level everything is layout vertically
  QVBoxLayout *vAllLaout = new QVBoxLayout;
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

void playlistItemYUVFile::savePlaylist(QDomDocument &doc, QDomElement &root, QDir playlistDir)
{
  //root.appendChild( createTextElement(doc, "key", "Class") );
  //root.appendChild( createTextElement(doc, "string", "YUVFile") );
  //root.appendChild( createTextElement(doc, "key", "Properties") );

  //// Determine the relative path to the yuv file. We save both in the playlist.
  //QUrl fileURL( getAbsoluteFilePath() );
  //fileURL.setScheme("file");
  //QString relativePath = playlistDir.relativeFilePath( getAbsoluteFilePath() );

  //QDomElement d = doc.createElement("dict");
  //
  //d.appendChild( createTextElement(doc, "key", "URL") );                 // <key>URL</key>
  //d.appendChild( createTextElement(doc, "string", fileURL.toString()) ); // <string>file:///D/Kimono.yuv</string>
  //
  //d.appendChild( createTextElement(doc, "key", "endFrame") );                      // <key>endFrame</key>
  //d.appendChild( createTextElement(doc, "integer", QString::number(endFrame)) );   // <integer>240</integer>
  //
  //d.appendChild( createTextElement(doc, "key", "frameOffset") );                   // <key>frameOffset</key>
  //d.appendChild( createTextElement(doc, "integer", QString::number(startFrame)) ); // <integer>240</integer>
  //
  //d.appendChild( createTextElement(doc, "key", "frameSampling") );                 // <key>frameSampling</key>
  //d.appendChild( createTextElement(doc, "integer", QString::number(sampling)) );   // <integer>1</integer>
  //
  //QString rate; rate.setNum(frameRate);
  //d.appendChild( createTextElement(doc, "key", "framerate") );  // <key>framerate</key>
  //d.appendChild( createTextElement(doc, "real", rate) );        // <integer>1</integer>
  //
  //d.appendChild( createTextElement(doc, "key", "height") );                                // <key>height</key>
  //d.appendChild( createTextElement(doc, "integer", QString::number(frameSize.height())) ); // <integer>1080</integer>
  //
  //d.appendChild( createTextElement(doc, "key", "pixelFormat") );           // <key>pixelFormat</key>
  //d.appendChild( createTextElement(doc, "string", srcPixelFormat.name) );  // <string>19</string>
  //
  //d.appendChild( createTextElement(doc, "key", "rURL") );                  // <key>rURL</key>
  //d.appendChild( createTextElement(doc, "string", relativePath) );         // <string>../Kimono.yuv</string>
  //
  //d.appendChild( createTextElement(doc, "key", "width") );                                 // <key>width</key>
  //d.appendChild( createTextElement(doc, "integer", QString::number(frameSize.width())) );  // <integer>1920</integer>
  //
  //root.appendChild(d);
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
  /*newFile->endFrame = endFrame;
  newFile->startFrame = startFrame;
  newFile->sampling = frameSampling;
  newFile->frameRate = framerate;
  newFile->frameSize = QSize(width, height);
  newFile->srcPixelFormat = yuvFormatList.getFromName(pixelFormat);*/

  return newFile;
}

void playlistItemYUVFile::loadFrame(int frameIdx)
{
  // Load one frame in YUV format
  qint64 fileStartPos = frameIdx * getBytesPerYUVFrame();
  readBytes( tempYUVFrameBuffer, fileStartPos, getBytesPerYUVFrame() );

  // Convert one frame from YUV to RGB
  convertYUVBufferToPixmap( tempYUVFrameBuffer, currentFrame );

  currentFrameIdx = frameIdx;
