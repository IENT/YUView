#include "differenceobject.h"

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

template <typename T> inline T clip(const T& n, const T& lower, const T& upper) { return std::max(lower, std::min(n, upper)); }

DifferenceObject::DifferenceObject(QObject* parent) : FrameObject("", parent)
{
    p_frameObjects[0] = NULL;
    p_frameObjects[1] = NULL;
}

DifferenceObject::~DifferenceObject()
{

}

void DifferenceObject::setFrameObjects(FrameObject* firstObject, FrameObject* secondObject)
{
    p_frameObjects[0] = firstObject;
    p_frameObjects[1] = secondObject;

    if( firstObject == NULL )
        return;

    p_width = firstObject->width();
    p_height = firstObject->height();
    p_numFrames = firstObject->numFrames();
    p_frameRate = firstObject->frameRate();

    p_colorConversionMode = firstObject->colorConversionMode();

    emit informationChanged();

}

void DifferenceObject::loadImage(unsigned int frameIdx, YUVCPixelFormatType outPixelFormat)
{
    if( p_frameObjects[0] == NULL || p_frameObjects[1] == NULL || p_frameObjects[0]->getyuvfile() == NULL || p_frameObjects[1]->getyuvfile() == NULL )
    {
        QImage tmpImage(p_width,p_height,QImage::Format_ARGB32);
        tmpImage.fill(qRgba(0, 0, 0, 0));   // clear with transparent color
        p_displayImage.convertFromImage(tmpImage);
        return;
    }

    // TODO: make sure that both yuv files have same bit depth

    const int width = MIN(p_frameObjects[0]->width(), p_frameObjects[1]->width());
    const int height = MIN(p_frameObjects[0]->height(), p_frameObjects[1]->height());

    // load both YUV444 buffers
    QByteArray yuv444Arrays[2];
    p_frameObjects[0]->getyuvfile()->getOneFrame(&yuv444Arrays[0], frameIdx, width, height);
    p_frameObjects[1]->getyuvfile()->getOneFrame(&yuv444Arrays[1], frameIdx, width, height);

    // create difference array
    subtractYUV444(&yuv444Arrays[0], &yuv444Arrays[1], &p_tmpBufferYUV444, p_frameObjects[0]->getyuvfile()->pixelFormat());

    if( doApplyYUVMath() )
        applyYUVMath(&p_tmpBufferYUV444, p_width, p_height);

    // convert from YUV444 (planar) to RGB888 (interleaved) color format (in place), if necessary
    if( outPixelFormat == YUVC_24RGBPixelFormat )
    {
        convertYUV2RGB(&p_tmpBufferYUV444, &p_PixmapConversionBuffer, YUVC_24RGBPixelFormat);
    }
    else    // calling function requested YUV444 --> copy data!
    {
        p_PixmapConversionBuffer.resize(p_tmpBufferYUV444.size());
        p_PixmapConversionBuffer.setRawData(p_tmpBufferYUV444.data(), p_tmpBufferYUV444.size());
    }

    // Convert the image in p_PixmapConversionBuffer to a QPixmap
    QImage tmpImage((unsigned char*)p_PixmapConversionBuffer.data(),p_width,p_height,QImage::Format_RGB888);

    p_displayImage.convertFromImage(tmpImage);

    p_lastIdx = frameIdx;
}

void DifferenceObject::subtractYUV444(QByteArray *srcBuffer0, QByteArray *srcBuffer1, QByteArray *outBuffer, YUVCPixelFormatType srcPixelFormat)
{
    int srcBufferLength0 = srcBuffer0->size();
    int srcBufferLength1 = srcBuffer1->size();
    assert( srcBufferLength0 == srcBufferLength1 );
    assert( srcBufferLength0%3 == 0 ); // YUV444 has 3 bytes per pixel

    // target buffer needs to be of same size as input
    if( outBuffer->size() != srcBufferLength0 )
        outBuffer->resize(srcBufferLength0);

    const int componentLength = srcBufferLength0/3;

    const int bps = YUVFile::bitsPerSample(srcPixelFormat);

    const int diffZero = 128<<(bps-8);

    // compute difference in YUV444 domain
    if (bps == 8)
    {
        const unsigned char * restrict srcY[2] = {(unsigned char*)srcBuffer0->data(), (unsigned char*)srcBuffer1->data()};
        const unsigned char * restrict srcU[2] = {srcY[0] + componentLength, srcY[1] + componentLength};
        const unsigned char * restrict srcV[2] = {srcU[0] + componentLength, srcU[1] + componentLength};
        unsigned char * restrict dstY = (unsigned char*)outBuffer->data();
        unsigned char * restrict dstU = dstY + componentLength;
        unsigned char * restrict dstV = dstU + componentLength;

        int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstY,dstU,dstV) // num_threads(2)
        for (i = 0; i < componentLength; ++i) {
            const int Y_diff = diffZero + (int)srcY[0][i] - (int)srcY[1][i];
            const int U_diff = diffZero + (int)srcU[0][i] - (int)srcU[1][i];
            const int V_diff = diffZero + (int)srcV[0][i] - (int)srcV[1][i];

            dstY[i] = Y_diff;
            dstU[i] = U_diff;
            dstV[i] = V_diff;
        }
    }
    else if (bps > 8 && bps <= 16)
    {
        const unsigned short *srcY[2] = {(unsigned short*)srcBuffer0->data(), (unsigned short*)srcBuffer1->data()};
        const unsigned short *srcU[2] = {srcY[0] + componentLength, srcY[1] + componentLength};
        const unsigned short *srcV[2] = {srcU[0] + componentLength, srcU[1] + componentLength};
        unsigned short * restrict dstY = (unsigned short*)outBuffer->data();
        unsigned short * restrict dstU = dstY + componentLength;
        unsigned short * restrict dstV = dstU + componentLength;

        int i;
#pragma omp parallel for default(none) private(i) shared(srcY,srcU,srcV,dstY,dstU,dstV) // num_threads(2)
        for (i = 0; i < componentLength; ++i) {
            const int Y_diff = diffZero + (int)srcY[0][i] - (int)srcY[1][i];
            const int U_diff = diffZero + (int)srcU[0][i] - (int)srcU[1][i];
            const int V_diff = diffZero + (int)srcV[0][i] - (int)srcV[1][i];

            dstY[i] = Y_diff;
            dstU[i] = U_diff;
            dstV[i] = V_diff;
        }
    }
    else
    {
        printf("bitdepth %i not supported", bps);
    }
}

// this slot is called when some parameters of the frame change
void DifferenceObject::refreshDisplayImage()
{
    // TODO: what do we do here?
}

ValuePairList DifferenceObject::getValuesAt(int x, int y)
{
    // TODO: load both sub-pixmaps in YUV444 and compute difference value at position

    if ( p_frameObjects[0] == NULL || p_frameObjects[1] == NULL || p_frameObjects[0]->getyuvfile() == NULL || p_frameObjects[1]->getyuvfile() == NULL )
        return ValuePairList();
    if( (x < 0) || (y < 0) || (x >= p_width) || (y >= p_height) )
        return ValuePairList();

    // load both YUV444 buffers
    QByteArray yuv444Arrays[2];
    p_frameObjects[0]->getyuvfile()->getOneFrame(&yuv444Arrays[0], p_lastIdx, p_width, p_height);
    p_frameObjects[1]->getyuvfile()->getOneFrame(&yuv444Arrays[1], p_lastIdx, p_width, p_height);

    const unsigned int planeLength = p_width*p_height;

    const int valY = yuv444Arrays[0].data()[y*p_width+x] - yuv444Arrays[1].data()[y*p_width+x];
    const int valU = yuv444Arrays[0].data()[planeLength+(y*p_width+x)] - yuv444Arrays[1].data()[planeLength+(y*p_width+x)];
    const int valV = yuv444Arrays[0].data()[2*planeLength+(y*p_width+x)] - yuv444Arrays[1].data()[2*planeLength+(y*p_width+x)];

    ValuePairList values;

    values.append( ValuePair("Diff Y", QString::number(valY)) );
    values.append( ValuePair("Diff U", QString::number(valU)) );
    values.append( ValuePair("Diff V", QString::number(valV)) );

    return values;
}
