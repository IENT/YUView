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

#include "yuvfile.h"
#include "common.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include "math.h"
#include <cfloat>
#include <assert.h>
#if _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

// OVERALL CANDIDATE MODES, sieved more in each step
typedef struct {
 int width;
 int height;
 YUVCPixelFormatType pixelFormat;

 // flags set while checking
 bool   interesting;
 float mseY;
} candMode_t;

candMode_t candidateModes[] = {
    {176,144,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {352,240,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {352,288,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {480,480,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {480,576,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {704,480,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {720,480,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {704,576,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {720,576,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1024,768,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1280,720,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1280,960,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1920,1072,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1920,1080,YUVC_420YpCbCr8PlanarPixelFormat, false, 0.0 },
    {176,144,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {352,240,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {352,288,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {480,480,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {480,576,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {704,480,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {720,480,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {720,486,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {704,576,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {720,576,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1024,768,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1280,720,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1280,960,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1920,1072,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {1920,1080,YUVC_422YpCbCr8PlanarPixelFormat, false, 0.0 },
    {-1,-1, YUVC_UnknownPixelFormat, false, 0.0 }
};

YUVFile::YUVFile(const QString &fname, QObject *parent) : YUVSource(parent)
{
    p_srcFile = NULL;

    // open file for reading
    p_srcFile = new QFile(fname);
    p_srcFile->open(QIODevice::ReadOnly);

    // get some more information from file
    QFileInfo fileInfo(p_srcFile->fileName());
    p_path = fileInfo.path();
    p_createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
    p_modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    p_fileSize = fileInfo.size();
}

YUVFile::~YUVFile()
{
    delete p_srcFile;
}

void YUVFile::getFormat(int* width, int* height, int* numFrames, double* frameRate)
{
  if (!isFormatValid()) {
    // Try to guess the values from the file name/size
    formatFromFile();
    
    if (!isFormatValid()) {
      // Try to get the format from the correlation
      formatFromCorrelation();
    }
  }

    // Call the yuvsource getFormat function
  YUVSource::getFormat(width, height, numFrames, frameRate);
}

// Is the format valid? Does width/height/numFrames match the file size?
bool YUVFile::isFormatValid()
{
  if (YUVSource::isFormatValid()) {
    // Widht/Height/numFrames are valid. Do they match the file size?
    QString filePath = p_srcFile->fileName();
    int bpf = YUVFile::bytesPerFrame(p_width, p_height, p_srcPixelFormat);
    if (bpf != 0 && (p_fileSize % bpf) == 0)
      // file size is dividable by bpf. This seems OK.
      return true;
  }
  return false;
}

// Get the number of frames from the file size. 
qint64 YUVFile::getNumberFrames()
{
    if (p_width > 0 && p_height > 0) {
      qint64 bpf = bytesPerFrame(p_width, p_height, p_srcPixelFormat);
      return (bpf == 0) ? -1 : p_fileSize / bpf;
    }
    else
        return -1;
}

void YUVFile::setSize(int width, int height)
{
  // Set the new size
  YUVSource::setSize(width, height);
}

qint64 YUVFile::readFrame( QByteArray &targetBuffer, unsigned int frameIdx, int width, int height )
{
    if(p_srcFile == NULL)
        return 0;

    qint64 bpf = bytesPerFrame(width, height, p_srcPixelFormat);
    qint64 startPos = frameIdx * bpf;

    // check if our buffer is big enough
    if( targetBuffer.size() != bpf )
        targetBuffer.resize(bpf);

    // read bytes from file
    readBytes( targetBuffer.data(), startPos, bpf);

    return bpf;
}

void YUVFile::readBytes( char *targetBuffer, qint64 startPos, qint64 length )
{
    if(p_srcFile == NULL)
        return;

    p_srcFile->seek(startPos);
    p_srcFile->read(targetBuffer, length);
    //targetBuffer->setRawData( (const char*)p_srcFile->map(startPos, length, QFile::NoOptions), length );
}

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

void YUVFile::formatFromFile()
{
  // Try to get the values from the files name
  QString filePath = p_srcFile->fileName();
  int width, height, frameRate, bitDepth, subFormat;
  formatFromFilename(filePath, width, height, frameRate, bitDepth, subFormat);
  
    if(width > 0 && height > 0 && bitDepth > 0)
    {
    // We were able to extrace width, height and bitDepth from the file name using
    // regular expressions. Try to get the pixel format by checking with the file size.

    YUVCPixelFormatType cFormat;
        if (bitDepth==8)
        {
            cFormat = YUVC_420YpCbCr8PlanarPixelFormat;
            int bpf = YUVFile::bytesPerFrame(width, height, cFormat); // assume 4:2:0, 8bit
      if (bpf != 0 && (p_fileSize % bpf) == 0) {
        // Bits per frame and file size match
        setFormat(width, height, frameRate);
        setPixelFormat(cFormat);
        return;
      }
        }
        else if (bitDepth==10)
        {
      cFormat = (subFormat == 444) ? YUVC_444YpCbCr10LEPlanarPixelFormat : YUVC_420YpCbCr10LEPlanarPixelFormat;
      int bpf = YUVFile::bytesPerFrame(width, height, cFormat); // assume 4:2:0 or 4:4:4 if in file name, 10bit
      if (bpf != 0 && (p_fileSize % bpf) == 0) {
        // Bits per frame and file size match
        setFormat(width, height, frameRate);
        setPixelFormat(cFormat);
        return;
      }
        }
        else
        {
            // do other stuff
        }
    }
}

/** Try to guess the format of the file. A list of candidates is tried (candidateModes) and it is checked if 
  * the file size matches and if the correlation of the first two frames is below a threshold.
  */
void YUVFile::formatFromCorrelation()
{
    if(p_srcFile->fileName().isEmpty())
        return;

    unsigned char *ptr;
    float leastMSE, mse;
    int   bestMode;

    // step1: file size must be a multiple of w*h*(color format)
    qint64 picSize;

    if(p_fileSize < 1)
        return;

    // if any candidate exceeds file size for two frames, discard
    // if any candidate does not represent a multiple of file size, discard
    int i = 0;
    bool found = false;
    while( candidateModes[i].pixelFormat != YUVC_UnknownPixelFormat )
    {
        /* one pic in bytes */
        picSize = bytesPerFrame(candidateModes[i].width, candidateModes[i].height, candidateModes[i].pixelFormat);

        if( p_fileSize >= (picSize*2) )    // at least 2 pics for correlation analysis
        {
            if( (p_fileSize % picSize) == 0 ) // important: file size must be multiple of pic size
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
    while( candidateModes[i].pixelFormat != YUVC_UnknownPixelFormat )
    {
        if( candidateModes[i].interesting )
        {
            picSize = bytesPerFrame(candidateModes[i].width, candidateModes[i].height, candidateModes[i].pixelFormat);

            QByteArray yuvBytes(picSize*2, 0);

            readBytes(yuvBytes.data(), 0, picSize*2);

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
    while( candidateModes[i].pixelFormat != YUVC_UnknownPixelFormat )
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
        int width  = candidateModes[bestMode].width;
        int height = candidateModes[bestMode].height;
    setSize(width, height);
    setPixelFormat(candidateModes[bestMode].pixelFormat);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


QString YUVFile::getName()
{
    QStringList components = p_srcFile->fileName().split(QDir::separator());
    return components.last();
}

// Check if the size/format/nrBytes makes sense
QString YUVFile::getStatus()
{
  if (!isFormatValid())
  {
    // The set format seems to be invalid
    return QString("Error: The set format is invalid.");
  }
  return QString("OK");
}

void YUVFile::getOneFrame(QByteArray &targetByteArray, unsigned int frameIdx )
{
    if (p_width <= 0 || p_height <= 0 || p_srcPixelFormat == YUVC_UnknownPixelFormat) {
        // Width, height or pixel Fromat invalid. Set them before getting frames.
        return;
    }

    // check if we need to do chroma upsampling
    if(p_srcPixelFormat != YUVC_444YpCbCr8PlanarPixelFormat && p_srcPixelFormat != YUVC_444YpCbCr12NativePlanarPixelFormat && p_srcPixelFormat != YUVC_444YpCbCr16NativePlanarPixelFormat && p_srcPixelFormat != YUVC_24RGBPixelFormat )
    {
        // read one frame into temporary buffer
        readFrame( p_tmpBufferYUV, frameIdx, p_width, p_height);
        //use dummy data to check
        // convert original data format into YUV444 planar format
        convert2YUV444(p_tmpBufferYUV, p_width, p_height, targetByteArray);
    }
    else    // source and target format are identical --> no conversion necessary
    {
        // read one frame into cached frame (already in YUV444 format)
        readFrame( targetByteArray, frameIdx, p_width, p_height);
    }
}

