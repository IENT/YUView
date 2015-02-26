#include "yuvfile.h"

#include <QFileInfo>
#include <QDir>

#include "math.h"
#include <cfloat>

#include <assert.h>
#include <unistd.h>


#define MAX(a, b) (((a) > (b)) ? (a) : (b))



#if __STDC__ != 1
#    define restrict __restrict /* use implementation __ format */
#else
#    ifndef __STDC_VERSION__
#        define restrict __restrict /* use implementation __ format */
#    else
#        if __STDC_VERSION__ < 199901L
#            define restrict __restrict /* use implementation __ format */
#        else
#            /* all ok */
#        endif
#    endif
#endif

static unsigned char clp[384+256+384];
static unsigned char *clip = clp+384;

inline quint32 SwapInt32(quint32 arg) {
    quint32 result;
    result = ((arg & 0xFF) << 24) | ((arg & 0xFF00) << 8) | ((arg >> 8) & 0xFF00) | ((arg >> 24) & 0xFF);
    return result;
}

inline quint16 SwapInt16(quint16 arg) {
    quint16 result;
    result = (quint16)(((arg << 8) & 0xFF00) | ((arg >> 8) & 0xFF));
    return result;
}

inline quint32 SwapInt32BigToHost(quint32 arg) {
#if __BIG_ENDIAN__
    return arg;
#else
    return SwapInt32(arg);
#endif
}

inline quint32 SwapInt32LittleToHost(quint32 arg) {
#if __LITTLE_ENDIAN__
    return arg;
#else
    return SwapInt32(arg);
#endif
}

inline quint16 SwapInt16LittleToHost(quint16 arg) {
#if __LITTLE_ENDIAN__
    return arg;
#else
    return SwapInt16(arg);
#endif
}

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

YUVFile::YUVFile(const QString &fname, QObject *parent) : QObject(parent)
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

    p_interpolationMode = BiLinearInterpolation;

    // initialize clipping table
    memset(clp, 0, 384);
    int i;
    for (i = 0; i < 256; i++) {
        clp[384+i] = i;
    }
    memset(clp+384+256, 255, 384);

    // create map of pixel format types
    p_formatProperties[YUVC_UnknownPixelFormat].setParams("Unknown Pixel Format", 0, 0, 0, 0, 0, false);
    p_formatProperties[YUVC_32RGBAPixelFormat].setParams("Unknown Pixel Format", 8, 32, 1, 1, 1, false);
    p_formatProperties[YUVC_24RGBPixelFormat].setParams("Unknown Pixel Format", 8, 24, 1, 1, 1, false);
    p_formatProperties[YUVC_24BGRPixelFormat].setParams("Unknown Pixel Format", 8, 24, 1, 1, 1, false);
    p_formatProperties[YUVC_411YpCbCr8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 12, 1, 4, 1, true);
    p_formatProperties[YUVC_420YpCbCr8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 12, 1, 2, 2, true);
    p_formatProperties[YUVC_422YpCbCr8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 16, 1, 2, 1, true);
    p_formatProperties[YUVC_UYVY422PixelFormat].setParams("Unknown Pixel Format", 8, 16, 1, 2, 1, false);
    p_formatProperties[YUVC_422YpCbCr10PixelFormat].setParams("Unknown Pixel Format", 10, 128, 6, 2, 1, false);
    p_formatProperties[YUVC_444YpCbCr8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 24, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr12LEPlanarPixelFormat].setParams("Unknown Pixel Format", 12, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr12BEPlanarPixelFormat].setParams("Unknown Pixel Format", 12, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr16LEPlanarPixelFormat].setParams("Unknown Pixel Format", 16, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr16BEPlanarPixelFormat].setParams("Unknown Pixel Format", 16, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr12NativePlanarPixelFormat].setParams("Unknown Pixel Format", 12, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr12SwappedPlanarPixelFormat].setParams("Unknown Pixel Format", 12, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr16NativePlanarPixelFormat].setParams("Unknown Pixel Format", 16, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_444YpCbCr16SwappedPlanarPixelFormat].setParams("Unknown Pixel Format", 16, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_8GrayPixelFormat].setParams("Unknown Pixel Format", 8, 8, 1, 0, 0, true);
    p_formatProperties[YUVC_GBR12in16LEPlanarPixelFormat].setParams("Unknown Pixel Format", 12, 48, 1, 1, 1, true);
    p_formatProperties[YUVC_420YpCbCr10LEPlanarPixelFormat].setParams("Unknown Pixel Format", 10, 24, 1, 2, 2, true);
    p_formatProperties[YUVC_422YpCrCb8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 16, 1, 2, 1, true);
    p_formatProperties[YUVC_444YpCrCb8PlanarPixelFormat].setParams("Unknown Pixel Format", 8, 24, 1, 1, 1, true);
    p_formatProperties[YUVC_UYVY422YpCbCr10PixelFormat].setParams("Unknown Pixel Format", 10, 128, 6, 2, 1, true);

}

YUVFile::~YUVFile()
{
    delete p_srcFile;
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

int YUVFile::readFrame( QByteArray *targetBuffer, unsigned int frameIdx, int width, int height, YUVCPixelFormatType cFormat )
{
    if(p_srcFile == NULL)
        return 0;

    int bpf = bytesPerFrame(width, height, cFormat);
    int startPos = frameIdx * bpf;

    // check if our buffer is big enough
    if( targetBuffer->size() != bpf )
        targetBuffer->resize(bpf);

    // read bytes from file
    readBytes( targetBuffer->data(), startPos, bpf);

    // TODO: check if this works for e.g. 10bit sequences
    int bitsPerPixel = bitsPerSample(cFormat);
    if( bitsPerPixel > 8 )
        scaleBytes( targetBuffer->data(), bitsPerPixel, bpf / 2 );

    return bpf;
}

void YUVFile::readBytes( char *targetBuffer, unsigned int startPos, unsigned int length )
{
    if(p_srcFile == NULL)
        return;

    p_srcFile->seek(startPos);
    p_srcFile->read(targetBuffer, length);
    //targetBuffer->setRawData( (const char*)p_srcFile->map(startPos, length, QFile::NoOptions), length );
}

void YUVFile::scaleBytes( char *targetBuffer, unsigned int bitsPerPixel, unsigned int numShorts )
{
    unsigned int leftShifts = 16 - bitsPerPixel;

    unsigned short * dataPtr = (unsigned short*) targetBuffer;

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

    if( leastMSE < 100 )
    {
        *width  = candidateModes[bestMode].width;
        *height = candidateModes[bestMode].height;
        *cFormat = candidateModes[bestMode].pixelFormat;
        *numFrames = fileSize / bytesPerFrame(*width, *height, *cFormat);
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


QString YUVFile::fileName()
{
    p_path = p_srcFile->fileName();
    QStringList components = p_srcFile->fileName().split(QDir::separator());
    return components.last();
}

unsigned int YUVFile::getFileSize()
{
    QFileInfo fileInfo(p_srcFile->fileName());

    return fileInfo.size();
}


void YUVFile::getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx, int width, int height, YUVCPixelFormatType srcPixelFormat)
{
    if(srcPixelFormat != YUVC_24RGBPixelFormat)
    {
        // read one frame into temporary buffer
        readFrame( &p_tmpBufferYUV, frameIdx, width, height, srcPixelFormat);

        // convert original data format into YUV interlaved format
        convert2YUV444(&p_tmpBufferYUV, srcPixelFormat, width, height, &p_tmpBufferYUV444);

        // convert from YUV444 to RGB888 color format (in place)
        convertYUV2RGB(&p_tmpBufferYUV444, targetByteArray, YUVC_24RGBPixelFormat);
    }
    else
    {
        // read one frame into cached frame (already RGB24)
        readFrame( targetByteArray, frameIdx, width, height, YUVC_24RGBPixelFormat);
    }
}

void YUVFile::convert2YUV444(QByteArray *sourceBuffer, YUVCPixelFormatType pixelFormatType, int lumaWidth, int lumaHeight, QByteArray *targetBuffer)
{
    const int componentWidth = lumaWidth;
    const int componentHeight = lumaHeight;
    const int componentLength = componentWidth*componentHeight;
    const int horiSubsampling = horizontalSubSampling(pixelFormatType);
    const int vertSubsampling = verticalSubSampling(pixelFormatType);
    const int chromaWidth = horiSubsampling==0 ? 0 : lumaWidth / horiSubsampling;
    const int chromaHeight = vertSubsampling == 0 ? 0 : lumaHeight / vertSubsampling;
    const int chromaLength = chromaWidth * chromaHeight;

    // make sure target buffer is big enough (YUV444 means 3 byte per sample)
    int targetBufferLength = 3*componentWidth*componentHeight;
    if( targetBuffer->size() != targetBufferLength )
        targetBuffer->resize(targetBufferLength);

    if (chromaLength == 0) {
        const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
        unsigned char *dstY = (unsigned char*)targetBuffer->data();
        unsigned char *dstU = dstY + componentLength;
        memcpy(dstY, srcY, componentLength);
        memset(dstU, 128, 2*componentLength);
    } else if (pixelFormatType == YUVC_UYVY422PixelFormat) {
        const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
        unsigned char *dstY = (unsigned char*)targetBuffer->data();
        unsigned char *dstU = dstY + componentLength;
        unsigned char *dstV = dstU + componentLength;

        int y;
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
        for (y = 0; y < componentHeight; y++) {
            for (int x = 0; x < componentWidth; x++) {
                dstY[x + y*componentWidth] = srcY[((x+y*componentWidth)<<1)+1];
                dstU[x + y*componentWidth] = srcY[((((x>>1)<<1)+y*componentWidth)<<1)];
                dstV[x + y*componentWidth] = srcY[((((x>>1)<<1)+y*componentWidth)<<1)+2];
            }
        }
    } else if (pixelFormatType == YUVC_UYVY422YpCbCr10PixelFormat) {
        const quint32 *srcY = (quint32*)sourceBuffer->data();
        quint16 *dstY = (quint16*)targetBuffer->data();
        quint16 *dstU = dstY + componentLength;
        quint16 *dstV = dstU + componentLength;

        int i;
#define BIT_INCREASE 6
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
        for (i = 0; i < ((componentLength+5)/6); i++) {
            const int srcPos = i*4;
            const int dstPos = i*6;
            quint32 srcVal;
            srcVal = SwapInt32BigToHost(srcY[srcPos]);
            dstV[dstPos]   = dstV[dstPos+1] = (srcVal&0xffc00000)>>(22-BIT_INCREASE);
            dstY[dstPos]   =                  (srcVal&0x003ff000)>>(12-BIT_INCREASE);
            dstU[dstPos]   = dstU[dstPos+1] = (srcVal&0x00000ffc)<<(BIT_INCREASE-2);
            srcVal = SwapInt32BigToHost(srcY[srcPos+1]);
            dstY[dstPos+1] =                  (srcVal&0xffc00000)>>(22-BIT_INCREASE);
            dstV[dstPos+2] = dstV[dstPos+3] = (srcVal&0x003ff000)>>(12-BIT_INCREASE);
            dstY[dstPos+2] =                  (srcVal&0x00000ffc)<<(BIT_INCREASE-2);
            srcVal = SwapInt32BigToHost(srcY[srcPos+2]);
            dstU[dstPos+2] = dstU[dstPos+3] = (srcVal&0xffc00000)>>(22-BIT_INCREASE);
            dstY[dstPos+3] =                  (srcVal&0x003ff000)>>(12-BIT_INCREASE);
            dstV[dstPos+4] = dstV[dstPos+5] = (srcVal&0x00000ffc)<<(BIT_INCREASE-2);
            srcVal = SwapInt32BigToHost(srcY[srcPos+3]);
            dstY[dstPos+4] =                  (srcVal&0xffc00000)>>(22-BIT_INCREASE);
            dstU[dstPos+4] = dstU[dstPos+5] = (srcVal&0x003ff000)>>(12-BIT_INCREASE);
            dstY[dstPos+5] =                  (srcVal&0x00000ffc)<<(BIT_INCREASE-2);
        }
    } else if (pixelFormatType == YUVC_422YpCbCr10PixelFormat) {
        const quint32 *srcY = (quint32*)sourceBuffer->data();
        quint16 *dstY = (quint16*)targetBuffer->data();
        quint16 *dstU = dstY + componentLength;
        quint16 *dstV = dstU + componentLength;

        int i;
#define BIT_INCREASE 6
#pragma omp parallel for default(none) shared(dstY,dstU,dstV,srcY)
        for (i = 0; i < ((componentLength+5)/6); i++) {
            const int srcPos = i*4;
            const int dstPos = i*6;
            quint32 srcVal;
            srcVal = SwapInt32LittleToHost(srcY[srcPos]);
            dstV[dstPos]   = dstV[dstPos+1] = (srcVal&0x3ff00000)>>(20-BIT_INCREASE);
            dstY[dstPos]   =                  (srcVal&0x000ffc00)>>(10-BIT_INCREASE);
            dstU[dstPos]   = dstU[dstPos+1] = (srcVal&0x000003ff)<<BIT_INCREASE;
            srcVal = SwapInt32LittleToHost(srcY[srcPos+1]);
            dstY[dstPos+1] =                  (srcVal&0x000003ff)<<BIT_INCREASE;
            dstU[dstPos+2] = dstU[dstPos+3] = (srcVal&0x000ffc00)>>(10-BIT_INCREASE);
            dstY[dstPos+2] =                  (srcVal&0x3ff00000)>>(20-BIT_INCREASE);
            srcVal = SwapInt32LittleToHost(srcY[srcPos+2]);
            dstU[dstPos+4] = dstU[dstPos+5] = (srcVal&0x3ff00000)>>(20-BIT_INCREASE);
            dstY[dstPos+3] =                  (srcVal&0x000ffc00)>>(10-BIT_INCREASE);
            dstV[dstPos+2] = dstV[dstPos+3] = (srcVal&0x000003ff)<<BIT_INCREASE;
            srcVal = SwapInt32LittleToHost(srcY[srcPos+3]);
            dstY[dstPos+4] =                  (srcVal&0x000003ff)<<BIT_INCREASE;
            dstV[dstPos+4] = dstV[dstPos+5] = (srcVal&0x000ffc00)>>(10-BIT_INCREASE);
            dstY[dstPos+5] =                  (srcVal&0x3ff00000)>>(20-BIT_INCREASE);
        }
    } else if (pixelFormatType == YUVC_420YpCbCr8PlanarPixelFormat && p_interpolationMode == BiLinearInterpolation) {
        // vertically midway positioning - unsigned rounding
        const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
        const unsigned char *srcU = srcY + componentLength;
        const unsigned char *srcV = srcU + chromaLength;
        const unsigned char *srcUV[2] = {srcU, srcV};
        unsigned char *dstY = (unsigned char*)targetBuffer->data();
        unsigned char *dstU = dstY + componentLength;
        unsigned char *dstV = dstU + componentLength;
        unsigned char *dstUV[2] = {dstU, dstV};

        const int dstLastLine = (componentHeight-1)*componentWidth;
        const int srcLastLine = (chromaHeight-1)*chromaWidth;

        memcpy(dstY, srcY, componentLength);

        int c;
        for (c = 0; c < 2; c++) {
            //NSLog(@"%i", omp_get_num_threads());
            // first line
            dstUV[c][0] = srcUV[c][0];
            int i;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (i = 0; i < chromaWidth-1; i++) {
                dstUV[c][i*2+1] = (( (int)(srcUV[c][i]) + (int)(srcUV[c][i+1]) + 1 ) >> 1);
                dstUV[c][i*2+2] = srcUV[c][i+1];
            }
            dstUV[c][componentWidth-1] = dstUV[c][componentWidth-2];

            int j;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (j = 0; j < chromaHeight-1; j++) {
                const int dstTop = (j*2+1)*componentWidth;
                const int dstBot = (j*2+2)*componentWidth;
                const int srcTop = j*chromaWidth;
                const int srcBot = (j+1)*chromaWidth;
                dstUV[c][dstTop] = (( 3*(int)(srcUV[c][srcTop]) +   (int)(srcUV[c][srcBot]) + 2 ) >> 2);
                dstUV[c][dstBot] = ((   (int)(srcUV[c][srcTop]) + 3*(int)(srcUV[c][srcBot]) + 2 ) >> 2);
                for (int i = 0; i < chromaWidth-1; i++) {
                    const int tl = srcUV[c][srcTop+i];
                    const int tr = srcUV[c][srcTop+i+1];
                    const int bl = srcUV[c][srcBot+i];
                    const int br = srcUV[c][srcBot+i+1];
                    dstUV[c][dstTop+i*2+1] = (( 6*tl + 6*tr + 2*bl + 2*br + 8 ) >> 4);
                    dstUV[c][dstBot+i*2+1] = (( 2*tl + 2*tr + 6*bl + 6*br + 8 ) >> 4);
                    dstUV[c][dstTop+i*2+2] = ((        3*tr +          br + 2 ) >> 2);
                    dstUV[c][dstBot+i*2+2] = ((          tr +        3*br + 2 ) >> 2);
                }
                dstUV[c][dstTop+componentWidth-1] = dstUV[c][dstTop+componentWidth-2];
                dstUV[c][dstBot+componentWidth-1] = dstUV[c][dstBot+componentWidth-2];
            }

            dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (i = 0; i < chromaWidth-1; i++) {
                dstUV[c][dstLastLine+i*2+1] = (( (int)(srcUV[c][srcLastLine+i]) + (int)(srcUV[c][srcLastLine+i+1]) + 1 ) >> 1);
                dstUV[c][dstLastLine+i*2+2] = srcUV[c][srcLastLine+i+1];
            }
            dstUV[c][dstLastLine+componentWidth-1] = dstUV[c][dstLastLine+componentWidth-2];
        }
    } else if (pixelFormatType == YUVC_420YpCbCr8PlanarPixelFormat && p_interpolationMode == InterstitialInterpolation) {
        // interstitial positioning - unsigned rounding, takes 2 times as long as nearest neighbour
        const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
        const unsigned char *srcU = srcY + componentLength;
        const unsigned char *srcV = srcU + chromaLength;
        const unsigned char *srcUV[2] = {srcU, srcV};
        unsigned char *dstY = (unsigned char*)targetBuffer->data();
        unsigned char *dstU = dstY + componentLength;
        unsigned char *dstV = dstU + componentLength;
        unsigned char *dstUV[2] = {dstU, dstV};

        const int dstLastLine = (componentHeight-1)*componentWidth;
        const int srcLastLine = (chromaHeight-1)*chromaWidth;

        memcpy(dstY, srcY, componentLength);

        int c;
        for (c = 0; c < 2; c++) {
            // first line
            dstUV[c][0] = srcUV[c][0];

            int i;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (i = 0; i < chromaWidth-1; i++) {
                dstUV[c][2*i+1] = ((3*(int)(srcUV[c][i]) +   (int)(srcUV[c][i+1]) + 2)>>2);
                dstUV[c][2*i+2] = ((  (int)(srcUV[c][i]) + 3*(int)(srcUV[c][i+1]) + 2)>>2);
            }
            dstUV[c][componentWidth-1] = srcUV[c][chromaWidth-1];

            int j;
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (j = 0; j < chromaHeight-1; j++) {
                const int dstTop = (j*2+1)*componentWidth;
                const int dstBot = (j*2+2)*componentWidth;
                const int srcTop = j*chromaWidth;
                const int srcBot = (j+1)*chromaWidth;
                dstUV[c][dstTop] = (( 3*(int)(srcUV[c][srcTop]) +   (int)(srcUV[c][srcBot]) + 2 ) >> 2);
                dstUV[c][dstBot] = ((   (int)(srcUV[c][srcTop]) + 3*(int)(srcUV[c][srcBot]) + 2 ) >> 2);
                for (int i = 0; i < chromaWidth-1; i++) {
                    const int tl = srcUV[c][srcTop+i];
                    const int tr = srcUV[c][srcTop+i+1];
                    const int bl = srcUV[c][srcBot+i];
                    const int br = srcUV[c][srcBot+i+1];
                    dstUV[c][dstTop+i*2+1] = (9*tl + 3*tr + 3*bl +   br + 8) >> 4;
                    dstUV[c][dstBot+i*2+1] = (3*tl +   tr + 9*bl + 3*br + 8) >> 4;
                    dstUV[c][dstTop+i*2+2] = (3*tl + 9*tr +   bl + 3*br + 8) >> 4;
                    dstUV[c][dstBot+i*2+2] = (  tl + 3*tr + 3*bl + 9*br + 8) >> 4;
                }
                dstUV[c][dstTop+componentWidth-1] = (( 3*(int)(srcUV[c][srcTop+chromaWidth-1]) +   (int)(srcUV[c][srcBot+chromaWidth-1]) + 2 ) >> 2);
                dstUV[c][dstBot+componentWidth-1] = ((   (int)(srcUV[c][srcTop+chromaWidth-1]) + 3*(int)(srcUV[c][srcBot+chromaWidth-1]) + 2 ) >> 2);
            }

            dstUV[c][dstLastLine] = srcUV[c][srcLastLine];
#pragma omp parallel for default(none) shared(dstUV,srcUV) firstprivate(c)
            for (i = 0; i < chromaWidth-1; i++) {
                dstUV[c][dstLastLine+i*2+1] = ((3*(int)(srcUV[c][srcLastLine+i]) +   (int)(srcUV[c][srcLastLine+i+1]) + 2)>>2);
                dstUV[c][dstLastLine+i*2+2] = ((  (int)(srcUV[c][srcLastLine+i]) + 3*(int)(srcUV[c][srcLastLine+i+1]) + 2)>>2);
            }
            dstUV[c][dstLastLine+componentWidth-1] = srcUV[c][srcLastLine+chromaWidth-1];
        }
    } /*else if (pixelFormatType == YUVC_420YpCbCr8PlanarPixelFormat && self.chromaInterpolation == 3) {
           // interstitial positioning - correct signed rounding - takes 6/5 times as long as unsigned rounding
           const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
           const unsigned char *srcU = srcY + componentLength;
           const unsigned char *srcV = srcU + chromaLength;
           unsigned char *dstY = (unsigned char*)targetBuffer->data();
           unsigned char *dstU = dstY + componentLength;
           unsigned char *dstV = dstU + componentLength;

           memcpy(dstY, srcY, componentLength);

           unsigned char *dstC = dstU;
           const unsigned char *srcC = srcU;
           int c;
           for (c = 0; c < 2; c++) {
              // first line
              unsigned char *endLine = dstC + componentWidth;
              unsigned char *endComp = dstC + componentLength;
              *dstC++ = *srcC;
              while (dstC < (endLine-1)) {
                 *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
                 *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
                 srcC++;
              }
              *dstC++ = *srcC++;
              srcC -= chromaWidth;

              while (dstC < endComp - 2*componentWidth) {
                 endLine = dstC + componentWidth;
                 *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
                 *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
                 dstC++;
                 while (dstC < endLine-1) {
                    int tl = (int)*srcC;
                    int tr = (int)*(srcC+1);
                    int bl = (int)*(srcC+chromaWidth);
                    int br = (int)*(srcC+chromaWidth+1);
                    *(dstC)                  = thresAddAndShift(9*tl + 3*tr + 3*bl +   br, 2048, 7, 8, 4);
                    *(dstC+1)                = thresAddAndShift(3*tl + 9*tr +   bl + 3*br, 2048, 7, 8, 4);
                    *(dstC+componentWidth)   = thresAddAndShift(3*tl +   tr + 9*bl + 3*br, 2048, 7, 8, 4);
                    *(dstC+componentWidth+1) = thresAddAndShift(  tl + 3*tr + 3*bl + 9*br, 2048, 7, 8, 4);
                    srcC++;
                    dstC+=2;
                 }
                 *(dstC)                = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
                 *(dstC+componentWidth) = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+chromaWidth)), 512, 1, 2, 2);
                 dstC++;
                 srcC++;
                 dstC += componentWidth;
              }

              endLine = dstC + componentWidth;
              *dstC++ = *srcC;
              while (dstC < (endLine-1)) {
                 *dstC++ = thresAddAndShift( 3*(int)(*srcC) +   (int)(*(srcC+1)), 512, 1, 2, 2);
                 *dstC++ = thresAddAndShift(   (int)(*srcC) + 3*(int)(*(srcC+1)), 512, 1, 2, 2);
                 srcC++;
              }
              *dstC++ = *srcC++;

              dstC = dstV;
              srcC = srcV;
           }
         }*/ else if (isPlanar(pixelFormatType) && bitsPerSample(pixelFormatType) == 8) {
        // sample and hold interpolation
        const bool reverseUV = (pixelFormatType == YUVC_444YpCrCb8PlanarPixelFormat) || (pixelFormatType == YUVC_422YpCrCb8PlanarPixelFormat);
        const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
        const unsigned char *srcU = srcY + componentLength + (reverseUV?chromaLength:0);
        const unsigned char *srcV = srcY + componentLength + (reverseUV?0:chromaLength);
        unsigned char *dstY = (unsigned char*)targetBuffer->data();
        unsigned char *dstU = dstY + componentLength;
        unsigned char *dstV = dstU + componentLength;
        int horiShiftTmp = 0;
        int vertShiftTmp = 0;
        while (((1<<horiShiftTmp) & horiSubsampling) != 0) horiShiftTmp++;
        while (((1<<vertShiftTmp) & vertSubsampling) != 0) vertShiftTmp++;
        const int horiShift = horiShiftTmp;
        const int vertShift = vertShiftTmp;

        memcpy(dstY, srcY, componentLength);

        if (2 == horiSubsampling && 2 == vertSubsampling) {
            int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
            for (y = 0; y < chromaHeight; y++) {
                for (int x = 0; x < chromaWidth; x++) {
                    dstU[2*x + 2*y*componentWidth] = dstU[2*x+1 + 2*y*componentWidth] = srcU[x + y*chromaWidth];
                    dstV[2*x + 2*y*componentWidth] = dstV[2*x+1 + 2*y*componentWidth] = srcV[x + y*chromaWidth];
                }
                memcpy(&dstU[(2*y+1)*componentWidth], &dstU[(2*y)*componentWidth], componentWidth);
                memcpy(&dstV[(2*y+1)*componentWidth], &dstV[(2*y)*componentWidth], componentWidth);
            }
        } else if ((1<<horiShift) == horiSubsampling && (1<<vertShift) == vertSubsampling) {
            int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
            for (y = 0; y < componentHeight; y++) {
                for (int x = 0; x < componentWidth; x++) {
                    //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
                    dstU[x + y*componentWidth] = srcU[(x>>horiShift) + (y>>vertShift)*chromaWidth];
                    dstV[x + y*componentWidth] = srcV[(x>>horiShift) + (y>>vertShift)*chromaWidth];
                }
            }
        } else {
            int y;
#pragma omp parallel for default(none) shared(dstV,dstU,srcV,srcU)
            for (y = 0; y < componentHeight; y++) {
                for (int x = 0; x < componentWidth; x++) {
                    //dstY[x + y*componentWidth] = srcY[x + y*componentWidth];
                    dstU[x + y*componentWidth] = srcU[x/horiSubsampling + y/vertSubsampling*chromaWidth];
                    dstV[x + y*componentWidth] = srcV[x/horiSubsampling + y/vertSubsampling*chromaWidth];
                }
            }
        }
    } else if (pixelFormatType == YUVC_420YpCbCr10LEPlanarPixelFormat) {
        // TODO: chroma interpolation for 4:2:0 10bit planar
        const unsigned short *srcY = (unsigned short*)sourceBuffer->data();
        const unsigned short *srcU = srcY + componentLength;
        const unsigned short *srcV = srcU + chromaLength;
        unsigned short *dstY = (unsigned short*)targetBuffer->data();
        unsigned short *dstU = dstY + componentLength;
        unsigned short *dstV = dstU + componentLength;

        int y;
#pragma omp parallel for default(none) shared(dstY,dstV,dstU,srcY,srcV,srcU)
        for (y = 0; y < componentHeight; y++) {
            for (int x = 0; x < componentWidth; x++) {
                //dstY[x + y*componentWidth] = MIN(1023, CFSwapInt16LittleToHost(srcY[x + y*componentWidth])) << 6; // clip value for data which excesses the 2^10-1 range
                dstY[x + y*componentWidth] = SwapInt16LittleToHost(srcY[x + y*componentWidth]) << 6;
                dstU[x + y*componentWidth] = SwapInt16LittleToHost(srcU[x/2 + (y/2)*chromaWidth]) << 6;
                dstV[x + y*componentWidth] = SwapInt16LittleToHost(srcV[x/2 + (y/2)*chromaWidth]) << 6;
            }
        }
    }
    else if (   pixelFormatType == YUVC_444YpCbCr12SwappedPlanarPixelFormat
                  || pixelFormatType == YUVC_444YpCbCr16SwappedPlanarPixelFormat)
    {
        swab((char*)sourceBuffer->data(), (char*)targetBuffer->data(), bytesPerFrame(componentWidth,componentHeight,pixelFormatType));
    } else {
        printf("Unhandled pixel format: %d", pixelFormatType);
    }

    return;
}

void YUVFile::convertYUV2RGB(QByteArray *sourceBuffer, QByteArray *targetBuffer, YUVCPixelFormatType targetPixelFormat)
{
    assert(targetPixelFormat == YUVC_24RGBPixelFormat);

    // make sure target buffer is big enough
    int srcBufferLength = sourceBuffer->size();
    assert( srcBufferLength%3 == 0 ); // YUV444 has 3 bytes per pixel

    // target buffer needs to be of same size as input
    if( targetBuffer->size() != srcBufferLength )
        targetBuffer->resize(srcBufferLength);

    const int componentLength = srcBufferLength/3;

    const int bps = bitsPerSample(targetPixelFormat);

    const int yOffset = 16<<(bps-8);
    const int cZero = 128<<(bps-8);
    const int rgbMax = (1<<bps)-1;
    int yMult, rvMult, guMult, gvMult, buMult;

    // TODO: to be adapted by GUI!
    YUVCColorConversionType colorConversionType = YUVC601ColorConversionType;

    unsigned char *dst = (unsigned char*)targetBuffer->data();

    if (bps == 8) {
        switch (colorConversionType) {
        case YUVC601ColorConversionType:
            yMult =   76309;
            rvMult = 104597;
            guMult = -25675;
            gvMult = -53279;
            buMult = 132201;
            break;
        case YUVC709ColorConversionType:
        default:
            yMult =   76309;
            rvMult = 117489;
            guMult = -13975;
            gvMult = -34925;
            buMult = 138438;
        }
        const unsigned char * restrict srcY = (unsigned char*)sourceBuffer->data();
        const unsigned char * restrict srcU = srcY + componentLength;
        const unsigned char * restrict srcV = srcU + componentLength;
        unsigned char * restrict dstMem = dst;

        int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip)// num_threads(2)
        for (i = 0; i < componentLength; ++i) {
            const int Y_tmp = ((int)srcY[i] - yOffset) * yMult;
            const int U_tmp = (int)srcU[i] - cZero;
            const int V_tmp = (int)srcV[i] - cZero;

            const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;
            const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
            const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;

            dstMem[3*i]   = clip[R_tmp];
            dstMem[3*i+1] = clip[G_tmp];
            dstMem[3*i+2] = clip[B_tmp];
        }
    } else if (bps > 8 && bps <= 16) {
        switch (colorConversionType) {
        case YUVC601ColorConversionType:
            yMult =   19535114;
            rvMult =  26776886;
            guMult =  -6572681;
            gvMult = -13639334;
            buMult =  33843539;
            break;
        case YUVC709ColorConversionType:
        default:
            yMult =   19535114;
            rvMult =  30077204;
            guMult =  -3577718;
            gvMult =  -8940735;
            buMult =  35440221;
        }
        if (bps < 16) {
            yMult  = (yMult  + (1<<(15-bps))) >> (16-bps);
            rvMult = (rvMult + (1<<(15-bps))) >> (16-bps);
            guMult = (guMult + (1<<(15-bps))) >> (16-bps);
            gvMult = (gvMult + (1<<(15-bps))) >> (16-bps);
            buMult = (buMult + (1<<(15-bps))) >> (16-bps);
        }
        const unsigned short *srcY = (unsigned short*)sourceBuffer->data();
        const unsigned short *srcU = srcY + componentLength;
        const unsigned short *srcV = srcU + componentLength;
        unsigned char *dstMem = dst;

        int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult) // num_threads(2)
        for (i = 0; i < componentLength; ++i) {
            qint64 Y_tmp = ((qint64)srcY[i] - yOffset) * yMult;
            qint64 U_tmp = (qint64)srcU[i] - cZero;
            qint64 V_tmp = (qint64)srcV[i] - cZero;

            qint64 R_tmp  = (Y_tmp                  + V_tmp * rvMult) >> (8+bps);
            dstMem[i*3]   = (R_tmp<0 ? 0 : (R_tmp>rgbMax ? rgbMax : R_tmp))>>(bps-8);
            qint64 G_tmp  = (Y_tmp + U_tmp * guMult + V_tmp * gvMult) >> (8+bps);
            dstMem[i*3+1] = (G_tmp<0 ? 0 : (G_tmp>rgbMax ? rgbMax : G_tmp))>>(bps-8);
            qint64 B_tmp  = (Y_tmp + U_tmp * buMult                 ) >> (8+bps);
            dstMem[i*3+2] = (B_tmp<0 ? 0 : (B_tmp>rgbMax ? rgbMax : B_tmp))>>(bps-8);
        }
    } else {
        printf("bitdepth %i not supported", bps);
    }
}

int YUVFile::verticalSubSampling(YUVCPixelFormatType pixelFormat)  { return p_formatProperties.count(pixelFormat)?p_formatProperties[pixelFormat].subsamplingVertical():0; }
int YUVFile::horizontalSubSampling(YUVCPixelFormatType pixelFormat) { return p_formatProperties.count(pixelFormat)?p_formatProperties[pixelFormat].subsamplingHorizontal():0; }
int YUVFile::bitsPerSample(YUVCPixelFormatType pixelFormat)  { return p_formatProperties.count(pixelFormat)?p_formatProperties[pixelFormat].bitsPerSample():0; }
int YUVFile::bytesPerFrame(int width, int height, YUVCPixelFormatType cFormat)
{
    if(p_formatProperties.count(cFormat) == 0)
        return 0;

    unsigned numSamples = width*height;
    unsigned remainder = numSamples % p_formatProperties[cFormat].bitsPerPixelDenominator();
    unsigned bits = numSamples / p_formatProperties[cFormat].bitsPerPixelDenominator();
    if (remainder == 0) {
        bits *= p_formatProperties[cFormat].bitsPerPixelNominator();
    } else {
        printf("warning: pixels not divisable by bpp denominator for pixel format '%d' - rounding up", cFormat);
        bits = (bits+1) * p_formatProperties[cFormat].bitsPerPixelNominator();
    }
    if (bits % 8 != 0) {
        printf("warning: bits not divisible by 8 for pixel format '%d' - rounding up", cFormat);
        bits += 8;
    }

    return bits/8;
}
bool YUVFile::isPlanar(YUVCPixelFormatType pixelFormat) { return p_formatProperties.count(pixelFormat)?p_formatProperties[pixelFormat].isPlanar():false; }
