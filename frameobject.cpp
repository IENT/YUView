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

#include "frameobject.h"

#include "yuvfile.h"
#include <QPainter>
#include "assert.h"

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

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

static unsigned char clp_buf[384+256+384];
static unsigned char *clip_buf = clp_buf+384;

enum {
   YUVMathDefaultColors,
   YUVMathLumaOnly,
   YUVMathCbOnly,
   YUVMathCrOnly
};

QCache<CacheIdx, QPixmap> FrameObject::frameCache;
QStringList duplicateList;

FrameObject::FrameObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    p_srcFile = NULL;

    // set some defaults
    p_width = 640;
    p_height = 480;
    p_frameRate = 30.0;

    p_lumaScale = 1;
    p_chromaUScale = 1;
    p_chromaVScale = 1;
    p_lumaOffset = 125;
    p_chromaOffset = 128;
    p_lumaInvert = 0;
    p_chromaInvert = 0;

    p_colorConversionMode = YUVC601ColorConversionType;

    // initialize clipping table
    memset(clp_buf, 0, 384);
    int i;
    for (i = 0; i < 256; i++) {
        clp_buf[384+i] = i;
    }
    memset(clp_buf+384+256, 255, 384);

    QFileInfo checkFile(srcFileName);
    if( checkFile.exists() && checkFile.isFile() )
    {

        p_srcFile = new YUVFile(srcFileName);
        p_srcFile->extractFormat(&p_width, &p_height, &p_endFrame, &p_frameRate);
        duplicateList.append(p_srcFile->fileName());
        // listen to changes emitted from YUV file and propagate to GUI
        QObject::connect(p_srcFile, SIGNAL(yuvInformationChanged()), this, SLOT(propagateParameterChanges()));
        QObject::connect(p_srcFile, SIGNAL(yuvInformationChanged()), this, SLOT(refreshDisplayImage()));
        // listen to changes emitted from frame object and propagate to GUI
        QObject::connect(this, SIGNAL(frameInformationChanged()), this, SLOT(propagateParameterChanges()));
        QObject::connect(this, SIGNAL(frameInformationChanged()), this, SLOT(refreshDisplayImage()));

        // set our name (remove file extension)
        int lastPoint = p_srcFile->fileName().lastIndexOf(".");
        p_name = p_srcFile->fileName().left(lastPoint);

        p_endFrame = p_srcFile->getNumberFrames(p_width, p_height)  -1;
    }
}

FrameObject::~FrameObject()
{
    if(p_srcFile != NULL)
    {
        duplicateList.removeOne(p_srcFile->fileName());
        clearCurrentCache();
        delete p_srcFile;
    }
}

void FrameObject::clearCurrentCache()
{
    if (p_srcFile!=NULL)
    {
    if (duplicateList.count(p_srcFile->fileName())==0)
    {
        for (int frameIdx=p_startFrame;frameIdx<=p_endFrame;frameIdx++)
        {
         CacheIdx cIdx(p_srcFile->fileName(), frameIdx);
         if (frameCache.contains(cIdx))
                 frameCache.remove(cIdx);
        }
    }
    }
}

void FrameObject::loadImage(int frameIdx)
{
    // TODO:
    // this method gets called way too many times even
    // if just a single parameter was changed
    if (frameIdx==INT_INVALID || frameIdx >= numFrames())
    {
        p_displayImage = QPixmap();
        return;
    }

    if( p_srcFile == NULL )
        return;

    // check if we have this frame index in our cache already
    CacheIdx cIdx(p_srcFile->fileName(), frameIdx);
    QPixmap* cachedFrame = frameCache.object(cIdx);
    if(cachedFrame == NULL)    // load the corresponding frame from yuv file into the frame buffer
    {
        // add new QPixmap to cache and use its data buffer
        cachedFrame = new QPixmap();

        if( p_srcFile->pixelFormat() != YUVC_24RGBPixelFormat )
        {
            // read YUV444 frame from file
            p_srcFile->getOneFrame(&p_tmpBufferYUV444, frameIdx, p_width, p_height);

            // if requested, do some YUV math
            if( doApplyYUVMath() )
                applyYUVMath(&p_tmpBufferYUV444, p_width, p_height, p_srcFile->pixelFormat());

            // convert from YUV444 (planar) to RGB888 (interleaved) color format (in place)
            convertYUV2RGB(&p_tmpBufferYUV444, &p_PixmapConversionBuffer, YUVC_24RGBPixelFormat);
        }
        else
        {
            // read RGB24 frame from file
            p_srcFile->getOneFrame(&p_PixmapConversionBuffer, frameIdx, p_width, p_height);
        }

        // add this frame into our cache, use MBytes as cost
        int sizeInMB = p_PixmapConversionBuffer.size() >> 20;

        // Convert the image in p_PixmapConversionBuffer to a QPixmap
        QImage tmpImage((unsigned char*)p_PixmapConversionBuffer.data(),p_width,p_height,QImage::Format_RGB888);
        cachedFrame->convertFromImage(tmpImage);

        frameCache.insert(cIdx, cachedFrame, sizeInMB);
    }

    p_lastIdx = frameIdx;

    // update our QImage with frame buffer
    p_displayImage = *cachedFrame;
}

ValuePairList FrameObject::getValuesAt(int x, int y)
{
    if ( (p_srcFile == NULL) || (x < 0) || (y < 0) || (x >= p_width) || (y >= p_height) )
        return ValuePairList();

    // read YUV 444 from file
    QByteArray yuvByteArray;
    p_srcFile->getOneFrame(&yuvByteArray, p_lastIdx, p_width, p_height);

    const unsigned int planeLength = p_width*p_height;

    const unsigned char valY = yuvByteArray.data()[y*p_width+x];
    const unsigned char valU = yuvByteArray.data()[planeLength+(y*p_width+x)];
    const unsigned char valV = yuvByteArray.data()[2*planeLength+(y*p_width+x)];

    ValuePairList values;

    values.append( ValuePair("Y", QString::number(valY)) );
    values.append( ValuePair("U", QString::number(valU)) );
    values.append( ValuePair("V", QString::number(valV)) );

    return values;
}

void FrameObject::applyYUVMath(QByteArray *sourceBuffer, int lumaWidth, int lumaHeight, YUVCPixelFormatType srcPixelFormat)
{
    const int lumaLength = lumaWidth*lumaHeight;
    const int singleChromaLength = lumaLength;
    const int chromaLength = 2*singleChromaLength;
    const int sourceBPS = YUVFile::bitsPerSample( srcPixelFormat );
    const int maxVal = (1<<sourceBPS)-1;

    const bool yInvert = p_lumaInvert;
    const int yOffset = p_lumaOffset;
    const int yMultiplier = p_lumaScale;
    const bool cInvert = p_chromaInvert;
    const int cOffset = p_chromaOffset;
    const int cMultiplier0 = p_chromaUScale;
    const int cMultiplier1 = p_chromaVScale;

    int colorMode = YUVMathDefaultColors;

    if( p_lumaScale != 0 && p_chromaUScale == 0 && p_chromaVScale == 0 )
        colorMode = YUVMathLumaOnly;
    else if( p_lumaScale == 0 && p_chromaUScale != 0 && p_chromaVScale == 0 )
        colorMode = YUVMathCbOnly;
    else if( p_lumaScale == 0 && p_chromaUScale == 0 && p_chromaVScale != 0 )
        colorMode = YUVMathCrOnly;

    if (sourceBPS == 8)
    {
        const unsigned char *src = (const unsigned char*)sourceBuffer->data();
        unsigned char *dst = (unsigned char*)sourceBuffer->data();

        //int i;
        if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
        {
            int i;
#pragma omp parallel for default(none) shared(src,dst)
            for (i = 0; i < lumaLength; i++) {
                int newVal = yInvert ? (maxVal-(int)(src[i])):((int)(src[i]));
                newVal = (newVal - yOffset) * yMultiplier + yOffset;
                newVal = MAX( 0, MIN( maxVal, newVal ) );
                dst[i] = (unsigned char)newVal;
            }
            dst += lumaLength;
        }
        src += lumaLength;

        for (int c = 0; c < 2; c++) {
            if (   colorMode == YUVMathDefaultColors
                   || (colorMode == YUVMathCbOnly && c == 0)
                   || (colorMode == YUVMathCrOnly && c == 1)
                   )
            {
                int i;
                int cMultiplier = (c==0)?cMultiplier0:cMultiplier1;
#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
                for (i = 0; i < singleChromaLength; i++) {
                    int newVal = cInvert?(maxVal-(int)(src[i])):((int)(src[i]));
                    newVal = (newVal - cOffset) * cMultiplier + cOffset;
                    newVal = MAX( 0, MIN( maxVal, newVal ) );
                    dst[i] = (unsigned char)newVal;
                }
                dst += singleChromaLength;
            }
            src += singleChromaLength;
        }

        if (colorMode != YUVMathDefaultColors) {
            // clear the chroma planes
            memset(dst, 128, chromaLength);
        }

    }
    else if (sourceBPS == 16)
    {
        const unsigned short *src = (const unsigned short*)sourceBuffer->data();
        unsigned short *dst = (unsigned short*)sourceBuffer->data();

        if (colorMode == YUVMathDefaultColors || colorMode == YUVMathLumaOnly)
        {
            int i;
#pragma omp parallel for default(none) shared(src,dst)
            for (i = 0; i < lumaLength; i++) {
                int newVal = yInvert?(maxVal-(int)(src[i])):((int)(src[i]));
                newVal = (newVal - yOffset) * yMultiplier + yOffset;
                newVal = MAX( 0, MIN( maxVal, newVal ) );
                dst[i] = (unsigned short)newVal;
            }
            dst += lumaLength;
        }
        src += lumaLength;

        for (int c = 0; c < 2; c++)
        {
            if (   colorMode == YUVMathDefaultColors
                   || (colorMode == YUVMathCbOnly && c == 0)
                   || (colorMode == YUVMathCrOnly && c == 1)
                   )
            {
                int i;
                int cMultiplier = (c==0)?cMultiplier0:cMultiplier1;
#pragma omp parallel for default(none) shared(src,dst,cMultiplier)
                for (i = 0; i < singleChromaLength; i++) {
                    int newVal = cInvert?(maxVal-(int)(src[i])):((int)(src[i]));
                    newVal = (newVal - cOffset) * cMultiplier + cOffset;
                    newVal = MAX( 0, MIN( maxVal, newVal ) );
                    dst[i] = (unsigned short)newVal;
                }
                dst += singleChromaLength;
            }
            src += singleChromaLength;

            if (colorMode != YUVMathDefaultColors)
            {
                // clear the chroma planes
                int i;
#pragma omp parallel for default(none) shared(dst)
                for (i = 0; i < chromaLength; i++)
                {
                    dst[i] = 32768;
                }
            }
        }
    }
    else
    {
        printf("unsupported bitdepth %d, returning original data\n", sourceBPS);
    }
}

void FrameObject::convertYUV2RGB(QByteArray *sourceBuffer, QByteArray *targetBuffer, YUVCPixelFormatType targetPixelFormat)
{
    Q_ASSERT(targetPixelFormat == YUVC_24RGBPixelFormat);

    // make sure target buffer is big enough
    int srcBufferLength = sourceBuffer->size();
    Q_ASSERT( srcBufferLength%3 == 0 ); // YUV444 has 3 bytes per pixel

    // target buffer needs to be of same size as input
    if( targetBuffer->size() != srcBufferLength )
        targetBuffer->resize(srcBufferLength);

    const int componentLength = srcBufferLength/3;

    const int bps = YUVFile::bitsPerSample(targetPixelFormat);

    const int yOffset = 16<<(bps-8);
    const int cZero = 128<<(bps-8);
    const int rgbMax = (1<<bps)-1;
    int yMult, rvMult, guMult, gvMult, buMult;

    unsigned char *dst = (unsigned char*)targetBuffer->data();

    if (bps == 8) {
        switch (p_colorConversionMode) {
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
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstMem,yMult,rvMult,guMult,gvMult,buMult,clip_buf)// num_threads(2)
        for (i = 0; i < componentLength; ++i) {
            const int Y_tmp = ((int)srcY[i] - yOffset) * yMult;
            const int U_tmp = (int)srcU[i] - cZero;
            const int V_tmp = (int)srcV[i] - cZero;

            const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;
            const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
            const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;

            dstMem[3*i]   = clip_buf[R_tmp];
            dstMem[3*i+1] = clip_buf[G_tmp];
            dstMem[3*i+2] = clip_buf[B_tmp];
        }
    } else if (bps > 8 && bps <= 16) {
        switch (p_colorConversionMode) {
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
        printf("bitdepth %i not supported\n", bps);
    }
}
