#include "videofile.h"

#include <QFileInfo>
#include <QDir>

QCache<CacheIdx, QByteArray> VideoFile::frameCache;

static unsigned char clp[384+256+384];
static unsigned char *clip = clp+384;

VideoFile::VideoFile(const QString &fname, QObject *parent) : QObject(parent)
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

    p_interpolationMode = BiLinear;

    // initialize clipping table
    memset(clp, 0, 384);
    int i;
    for (i = 0; i < 256; i++) {
        clp[384+i] = i;
    }
    memset(clp+384+256, 255, 384);
}

VideoFile::~VideoFile()
{
    delete p_srcFile;
}


QString VideoFile::fileName()
{
    p_path = p_srcFile->fileName();
    QStringList components = p_srcFile->fileName().split(QDir::separator());
    return components.last();
}

unsigned int VideoFile::getFileSize()
{
    QFileInfo fileInfo(p_srcFile->fileName());

    return fileInfo.size();
}


void VideoFile::getOneFrame(void* &frameData, unsigned int frameIdx, int width, int height, ColorFormat srcFormat, int bpp)
{
    // check if we have this frame index in our cache already
    CacheIdx cIdx(p_srcFile->fileName(), frameIdx);
    QByteArray* cachedFrame = frameCache.object(cIdx);
    if( cachedFrame == NULL )
    {
        // add new QByteArray to cache and use its data buffer
        cachedFrame = new QByteArray;
        int bytesRead = 0;

        if(srcFormat != RGB888)
        {
            // read one frame into temporary buffer
            getFrames( &p_tmpBuffer, frameIdx, 1, width, height, srcFormat, bpp);

            // convert original data format into YUV interlaved format
            bytesRead = convert2YUV444Interleave(&p_tmpBuffer, srcFormat, width, height, cachedFrame);

            // convert from YUV444 to RGB888 color format (in place)
            convertYUV4442RGB888(cachedFrame);
        }
        else
        {
            // read one frame into cached frame
            bytesRead = getFrames( cachedFrame, frameIdx, 1, width, height, srcFormat, bpp);
        }

        // add this frame into our cache, use kBytes as cost
        int sizeInMB = bytesRead >> 20;
        frameCache.insert(cIdx, cachedFrame, sizeInMB);
    }

    frameData = cachedFrame->data();
}

int VideoFile::convert2YUV444Interleave(QByteArray *sourceBuffer, ColorFormat cFormat, int componentWidth, int componentHeight, QByteArray *targetBuffer)
{
    // make sure target buffer is big enough
    int targetBufferLength = 3*componentWidth*componentHeight;
    if( targetBuffer->capacity() < targetBufferLength )
        targetBuffer->resize(targetBufferLength);

    const int componentLength = componentWidth*componentHeight;
    //const int chromaLength = componentLength>>2;

    const unsigned char *srcY = (unsigned char*)sourceBuffer->data();
    const unsigned char *srcU = srcY + componentLength;
    const unsigned char *srcV = srcU + componentLength/4;
    char *dstYUV = targetBuffer->data();

    // currently, sample and hold interpolation only
   if (cFormat == YUV420)
   {
       int chromaWidth = componentWidth>>1;
       int chromaHeight= componentHeight>>1;

       if( p_interpolationMode == NearestNeighbor )
       {
           int y;
#pragma omp parallel for default(none) shared(dstYUV,srcY,srcU,srcV,componentWidth,chromaHeight,chromaWidth)
           for (y = 0; y < chromaHeight; y++)
           {
               for (int x = 0; x < chromaWidth; x++)
               {
                   const char tmpU = srcU[y*chromaWidth + x];
                   const char tmpV = srcV[y*chromaWidth + x];

                   // top left pixel
                   dstYUV[3*(2*y*componentWidth + 2*x)]   = srcY[2*y*componentWidth + 2*x];
                   dstYUV[3*(2*y*componentWidth + 2*x)+1] = tmpU;
                   dstYUV[3*(2*y*componentWidth + 2*x)+2] = tmpV;
                   // top right pixel
                   dstYUV[3*(2*y*componentWidth + 2*x +1)]   = srcY[2*y*componentWidth + 2*x + 1];
                   dstYUV[3*(2*y*componentWidth + 2*x +1)+1] = tmpU;
                   dstYUV[3*(2*y*componentWidth + 2*x +1)+2] = tmpV;
                   // bottom left pixel
                   dstYUV[3*((2*y+1)*componentWidth + 2*x)]   = srcY[(2*y+1)*componentWidth + 2*x];
                   dstYUV[3*((2*y+1)*componentWidth + 2*x)+1] = tmpU;
                   dstYUV[3*((2*y+1)*componentWidth + 2*x)+2] = tmpV;
                   // bottom right pixel
                   dstYUV[3*((2*y+1)*componentWidth + 2*x +1)]   = srcY[(2*y+1)*componentWidth + 2*x +1];
                   dstYUV[3*((2*y+1)*componentWidth + 2*x +1)+1] = tmpU;
                   dstYUV[3*((2*y+1)*componentWidth + 2*x +1)+2] = tmpV;
               }
           }
       }
       else if( p_interpolationMode == BiLinear )
       {
          const int dstLastLine = (componentHeight-1)*componentWidth;
          const int srcLastLine = (chromaHeight-1)*chromaWidth;

         // first row
         dstYUV[0] = srcY[0];
         dstYUV[1] = srcU[0];
         dstYUV[2] = srcV[0];

         int i;
#pragma omp parallel for default(none) shared(dstYUV,chromaWidth,srcY,srcU,srcV)
         for (i = 0; i < chromaWidth-1; i++)
         {
            const char tmpU = (char)(((int)srcU[i] + (int)srcU[i+1] + 1) / 2);
            const char tmpV = (char)(((int)srcV[i] + (int)srcV[i+1] + 1) / 2);

            dstYUV[3*(i*2+1)]   = srcY[i*2+1];
            dstYUV[3*(i*2+1)+1] = tmpU;
            dstYUV[3*(i*2+1)+2] = tmpV;

            dstYUV[3*(i*2+2)]  = srcY[i*2+2];
            dstYUV[3*(i*2+2)+1] = srcU[i+1];
            dstYUV[3*(i*2+2)+2] = srcV[i+1];
         }
         dstYUV[3*(componentWidth-1)]   = dstYUV[3*(componentWidth-2)];
         dstYUV[3*(componentWidth-1)+1] = dstYUV[3*(componentWidth-2)+1];
         dstYUV[3*(componentWidth-1)+2] = dstYUV[3*(componentWidth-2)+2];

         int j;
#pragma omp parallel for default(none) shared(dstYUV,chromaWidth,chromaHeight,componentWidth,srcY,srcU,srcV)
         for (j = 0; j < chromaHeight-1; j++)
         {
            const int dstTop = (j*2+1)*componentWidth;
            const int dstBot = (j*2+2)*componentWidth;
            const int srcTop = j*chromaWidth;
            const int srcBot = (j+1)*chromaWidth;

            // first column
            const char tmpU_t = (char)(( 3*((int)srcU[srcTop]) +   ((int)srcU[srcBot]) + 2 ) / 4);
            const char tmpV_t = (char)(( 3*((int)srcV[srcTop]) +   ((int)srcV[srcBot]) + 2 ) / 4);
            const char tmpU_b = (char)((   ((int)srcU[srcTop]) + 3*((int)srcU[srcBot]) + 2 ) / 4);
            const char tmpV_b = (char)((   ((int)srcV[srcTop]) + 3*((int)srcV[srcBot]) + 2 ) / 4);
            dstYUV[3*dstTop]   = srcY[dstTop];
            dstYUV[3*dstTop+1] = tmpU_t;
            dstYUV[3*dstTop+2] = tmpV_t;
            dstYUV[3*dstBot]   = srcY[dstBot];
            dstYUV[3*dstBot+1] = tmpU_b;
            dstYUV[3*dstBot+2] = tmpV_b;

            // interior pixels
            for (int i = 0; i < chromaWidth-1; i++)
            {
                // U component
                const int srcU_tl = srcU[srcTop+i];
                const int srcU_tr = srcU[srcTop+i+1];
                const int srcU_bl = srcU[srcBot+i];
                const int srcU_br = srcU[srcBot+i+1];

                const char dstU_tl = (char)(( 6*srcU_tl + 6*srcU_tr + 2*srcU_bl + 2*srcU_br + 8 ) / 16);
                const char dstU_bl = (char)(( 2*srcU_tl + 2*srcU_tr + 6*srcU_bl + 6*srcU_br + 8 ) / 16);
                const char dstU_tr = (char)((             3*srcU_tr +               srcU_br + 2 ) / 4);
                const char dstU_br = (char)((               srcU_tr +             3*srcU_br + 2 ) / 4);

                // V component
                const int srcV_tl = srcV[srcTop+i];
                const int srcV_tr = srcV[srcTop+i+1];
                const int srcV_bl = srcV[srcBot+i];
                const int srcV_br = srcV[srcBot+i+1];

                const char dstV_tl = (char)(( 6*srcV_tl + 6*srcV_tr + 2*srcV_bl + 2*srcV_br + 8 ) / 16);
                const char dstV_bl = (char)(( 2*srcV_tl + 2*srcV_tr + 6*srcV_bl + 6*srcV_br + 8 ) / 16);
                const char dstV_tr = (char)((             3*srcV_tr +               srcV_br + 2 ) / 4);
                const char dstV_br = (char)((               srcV_tr +             3*srcV_br + 2 ) / 4);

                dstYUV[3*(dstTop+i*2+1)]   = srcY[dstTop+i*2+1];
                dstYUV[3*(dstTop+i*2+1)+1] = dstU_tl;
                dstYUV[3*(dstTop+i*2+1)+2] = dstV_tl;

                dstYUV[3*(dstBot+i*2+1)]   = srcY[dstBot+i*2+1];
                dstYUV[3*(dstBot+i*2+1)+1] = dstU_bl;
                dstYUV[3*(dstBot+i*2+1)+2] = dstV_bl;

                dstYUV[3*(dstTop+i*2+2)]   = srcY[dstTop+i*2+2];
                dstYUV[3*(dstTop+i*2+2)+1] = dstU_tr;
                dstYUV[3*(dstTop+i*2+2)+2] = dstV_tr;

                dstYUV[3*(dstBot+i*2+2)]   = srcY[dstBot+i*2+2];
                dstYUV[3*(dstBot+i*2+2)+1] = dstU_br;
                dstYUV[3*(dstBot+i*2+2)+2] = dstV_br;
            }

            // last column
            dstYUV[3*(dstTop+componentWidth-1)]   = dstYUV[3*(dstTop+componentWidth-2)];
            dstYUV[3*(dstTop+componentWidth-1)+1] = dstYUV[3*(dstTop+componentWidth-2)+1];
            dstYUV[3*(dstTop+componentWidth-1)+2] = dstYUV[3*(dstTop+componentWidth-2)+2];
            dstYUV[3*(dstBot+componentWidth-1)]   = dstYUV[3*(dstBot+componentWidth-2)];
            dstYUV[3*(dstBot+componentWidth-1)+1] = dstYUV[3*(dstBot+componentWidth-2)+1];
            dstYUV[3*(dstBot+componentWidth-1)+2] = dstYUV[3*(dstBot+componentWidth-2)+2];
         }

         // last row
         dstYUV[3*dstLastLine] = srcY[dstLastLine];
         dstYUV[3*dstLastLine+1] = srcU[srcLastLine];
         dstYUV[3*dstLastLine+2] = srcV[srcLastLine];
#pragma omp parallel for default(none) shared(dstYUV,chromaWidth,srcY,srcU,srcV)
         for (i = 0; i < chromaWidth-1; i++)
         {
            const char tmpU = (char)(( ((int)srcU[srcLastLine+i]) + ((int)srcU[srcLastLine+i+1]) + 1 ) / 2);
            const char tmpV = (char)(( ((int)srcV[srcLastLine+i]) + ((int)srcV[srcLastLine+i+1]) + 1 ) / 2);

            dstYUV[3*(dstLastLine+i*2+1)]   = srcY[dstLastLine+i*2+1];
            dstYUV[3*(dstLastLine+i*2+1)+1] = tmpU;
            dstYUV[3*(dstLastLine+i*2+1)+2] = tmpV;
            dstYUV[3*(dstLastLine+i*2+2)]   = srcY[dstLastLine+i*2+1];
            dstYUV[3*(dstLastLine+i*2+2)+1] = srcU[srcLastLine+i+1];
            dstYUV[3*(dstLastLine+i*2+2)+2] = srcV[srcLastLine+i+1];
         }
         dstYUV[3*(dstLastLine+componentWidth-1)]   = dstYUV[3*(dstLastLine+componentWidth-2)];
         dstYUV[3*(dstLastLine+componentWidth-1)+1] = dstYUV[3*(dstLastLine+componentWidth-2)+1];
         dstYUV[3*(dstLastLine+componentWidth-1)+2] = dstYUV[3*(dstLastLine+componentWidth-2)+2];
       }
   }

   return targetBufferLength;
}

void VideoFile::convertYUV4442RGB888(QByteArray *buffer, int bps)
{
    // make sure target buffer is big enough
    int bufferLength = buffer->capacity();
    //assert( bufferLength%3 == 0 ); // YUV444 has 3 bytes per pixel

    //const int componentLength = bufferLength/3;

    const int yOffset = 16<<(bps-8);
    const int cZero = 128<<(bps-8);
    //const int rgbMax = (1<<bps)-1;
    int yMult, rvMult, guMult, gvMult, buMult;

    // TODO: currently only BT.601
    yMult =   76309;
    rvMult = 104597;
    guMult = -25675;
    gvMult = -53279;
    buMult = 132201;

    const unsigned char *src = (unsigned char*)buffer->data();
    unsigned char *dst = (unsigned char*)buffer->data();

    for(int i=0; i<bufferLength; i+=3)
    {
        const int Y_tmp = ((int)src[i] - yOffset) * yMult;
        const int U_tmp = (int)src[i+1] - cZero;
        const int V_tmp = (int)src[i+2] - cZero;

        const int R_tmp = (Y_tmp                  + V_tmp * rvMult ) >> 16;
        const int G_tmp = (Y_tmp + U_tmp * guMult + V_tmp * gvMult ) >> 16;
        const int B_tmp = (Y_tmp + U_tmp * buMult                  ) >> 16;

        dst[i]   = clip[R_tmp];
        dst[i+1] = clip[G_tmp];
        dst[i+2] = clip[B_tmp];
    }

}

void VideoFile::clearCache() {
    frameCache.clear();
}
