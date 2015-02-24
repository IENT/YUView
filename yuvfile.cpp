#include "yuvfile.h"

#include <QFileInfo>
#include <QDir>

#include "math.h"
#include <cfloat>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

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

YUVFile::YUVFile(const QString &fname, QObject *parent) : VideoFile(fname, parent)
{

}

void YUVFile::extractFormat(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames, double* frameRate)
{
    // preset return values
    int width1 = -1;
    int height1 = -1;
    int numFrames1 = -1;
    int width2 = -1;
    int height2 = -1;
    int numFrames2 = -1;

    double frameRate1 = -1, frameRate2 = -1;
    YUVCPixelFormatType cFormat1 = YUVC_UnknownPixelFormat, cFormat2 = YUVC_UnknownPixelFormat;

    // try to get information
    formatFromFilename(&width1, &height1, &frameRate1, &numFrames1);
    formatFromCorrelation(&width2, &height2, &cFormat2, &numFrames2);

    // set return values
    *width = MAX( width1, width2 );
    *height = MAX( height1, height2 );
    *cFormat = MAX( cFormat1, cFormat2 );
    *numFrames = MAX( numFrames1, numFrames2 );
    *frameRate = MAX( frameRate1, frameRate2 );
}

void YUVFile::refreshNumberFrames(int* numFrames, int width, int height, YUVCPixelFormatType cFormat)
{
    int fileSize = getFileSize();

    if(width > 0 && height > 0)
        *numFrames = fileSize / bytesPerFrame(width, height, cFormat);
    else
        *numFrames = -1;
}

int YUVFile::getFrames( QByteArray *targetBuffer, unsigned int frameIdx, unsigned int frames2read, int width, int height, YUVCPixelFormatType cFormat )
{
    if(p_srcFile == NULL)
        return 0;

    int bpf = bytesPerFrame(width, height, cFormat);

    int numBytes = frames2read * bpf;
    int startPos = frameIdx * bpf;

    // read bytes from file
    readBytes( targetBuffer, startPos, numBytes);

    int bitsPerPixel = bitsPerSample(cFormat);

    if( bitsPerPixel > 8 )
        scaleBytes( targetBuffer, bitsPerPixel, bpf / 2 );

    return numBytes;
}

void YUVFile::readBytes( QByteArray *targetBuffer, unsigned int startPos, unsigned int length )
{
    if(p_srcFile == NULL)
        return;

    // check if our buffer is big enough
    if( targetBuffer->capacity() < (int)length )
        targetBuffer->resize(length);

    p_srcFile->seek(startPos);
    p_srcFile->read(targetBuffer->data(), length);
    //targetBuffer->setRawData( (const char*)p_srcFile->map(startPos, length, QFile::NoOptions), length );
}

void YUVFile::scaleBytes( QByteArray *targetBuffer, unsigned int bitsPerPixel, unsigned int numShorts )
{
    unsigned int leftShifts = 16 - bitsPerPixel;

    unsigned short * dataPtr = (unsigned short*) targetBuffer->data();

    for( unsigned int i = 0; i < numShorts; i++ )
    {
        dataPtr[i] = dataPtr[i] << leftShifts;
    }
}


float computeMSE( unsigned char *ptr, unsigned char *ptr2, int numPixels )
{
    float mse=0.0;
    float diff;

    if( numPixels > 0 )
    {
        for(int i=0; i<numPixels; i++)
        {
            diff = (float)ptr[i] - (float)ptr2[i];
            mse += diff*diff;
        }

        /* normalize on correlated pixels */
        mse /= (float)(numPixels);
    }

    return mse;
}

void YUVFile::formatFromFilename(int* width, int* height, double* frameRate, int* numFrames)
{
    if(p_srcFile->fileName().isEmpty())
        return;

    // parse filename and extract width, height and framerate
    // default format is: sequenceName_widthxheight_framerate.yuv

    QString widthString;
    QString heightString;
    QString rateString;
    QRegExp rx("([0-9]+)x([0-9]+)_([0-9]+)");

    // preset return values first
    *width = -1;
    *height = -1;
    *frameRate = -1;
    *numFrames = -1;

    int pos = rx.indexIn(p_srcFile->fileName());

    if(pos > -1)
    {
        rateString = rx.cap(3);
        *frameRate = rateString.toDouble();

        widthString = rx.cap(1);
        *width = widthString.toInt();

        heightString = rx.cap(2);
        *height = heightString.toInt();
    }

    int fileSize = getFileSize();

    if(*width > 0 && *height > 0)
        *numFrames = fileSize / bytesPerFrame(*width, *height, YUVC_420YpCbCr8PlanarPixelFormat); // assume 4:2:0, 8bit
    else
        *numFrames = -1;

}

void YUVFile::formatFromCorrelation(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames)
{
    if(p_srcFile->fileName().isEmpty())
        return;

    // all below is plain old C :-)
    int col, w, h;

    unsigned char *ptr;
    float leastMSE, mse;
    int   bestMode;

    // step1: file size must be a multiple of w*h*(color format)
    QFileInfo fileInfo(*p_srcFile);
    unsigned int fileSize = fileInfo.size();
    unsigned int picSize;

    if(fileSize == 0)
        return;

    // if any candidate exceeds file size for two frames, discard
    // if any candidate does not represent a multiple of file size, discard
    int i = 0;
    bool found = false;
    while( candidateModes[i].pixelFormat != YUVC_UnknownPixelFormat )
    {
        /* one pic in bytes */
        picSize = bytesPerFrame(candidateModes[i].width, candidateModes[i].height, candidateModes[i].pixelFormat);

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
    while( candidateModes[i].pixelFormat != YUVC_UnknownPixelFormat )
    {
        if( candidateModes[i].interesting )
        {
            picSize = bytesPerFrame(candidateModes[i].width, candidateModes[i].height, candidateModes[i].pixelFormat);

            QByteArray yuvBytes(picSize*2, 0);

            readBytes(&yuvBytes, 0, picSize*2);

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

    if( leastMSE < 100 )
    {
        *width  = candidateModes[bestMode].width;
        *height = candidateModes[bestMode].height;
        *cFormat = candidateModes[bestMode].pixelFormat;
        *numFrames = fileSize / bytesPerFrame(*width, *height, *cFormat);
    }

}
