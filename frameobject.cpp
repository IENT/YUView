#include "frameobject.h"

#include "yuvfile.h"
#include <QPainter>

FrameObject::FrameObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    QFileInfo fi(srcFileName);
    QString ext = fi.suffix();
    ext = ext.toLower();
    if( ext == "yuv" )
    {
        p_srcFile = new YUVFile(srcFileName);
    }
    else
    {
        exit(1);
    }

    // preset internal values
    p_pixelFormat = YUVC_UnknownPixelFormat;
    p_interpolationMode = 0;
    p_colorConversionMode = 0;

    // try to extract format information
    p_srcFile->extractFormat(&p_width, &p_height, &p_pixelFormat, &p_numFrames, &p_frameRate);

    // check returned values
    if(p_width < 0)
        p_width = 640;
    if(p_height < 0)
        p_height = 480;
    if(p_pixelFormat == YUVC_UnknownPixelFormat)
        p_pixelFormat = YUVC_420YpCbCr8PlanarPixelFormat;
    if(p_numFrames < 0)
        p_numFrames = 1;
    if(p_frameRate < 0)
        p_frameRate = 30.0;

    // set our name (remove file extension)
    int lastPoint = p_srcFile->fileName().lastIndexOf(".");
    p_name = p_srcFile->fileName().left(lastPoint);
}

FrameObject::~FrameObject()
{
    delete p_srcFile;
}

void FrameObject::loadImage(unsigned int frameIdx)
{
    if( p_srcFile == NULL )
        return;

    void* frameData = NULL;

    // load the corresponding frame from yuv file into the frame buffer
    p_srcFile->getOneFrame(frameData, frameIdx, p_width, p_height, p_pixelFormat);

    p_lastIdx = frameIdx;

    if( frameData == NULL )
        return;

    // update our QImage with frame buffer
    p_displayImage = QImage((uchar*)frameData, p_width, p_height, QImage::Format_RGB888);
}

// this slot is called when some parameters of the frame change
void FrameObject::refreshDisplayImage()
{
    p_srcFile->clearCache();
    loadImage(p_lastIdx);
}

int FrameObject::getPixelValue(int x, int y) {
    if ( (p_srcFile == NULL) || (x < 0) || (y < 0) || (x >= p_width) || (y >= p_height) )
        return 0;

    void* frameData = NULL;

    // load the corresponding frame from our yuv file into the frame buffer
    p_srcFile->getOneFrame(frameData, p_lastIdx, p_width, p_height, p_pixelFormat);

    char* dstYUV = static_cast<char*>(frameData);
    int ret=0;
    unsigned char *components = reinterpret_cast<unsigned char*>(&ret);
    components[3] = dstYUV[3*(y*p_width + x)+0];
    components[2] = dstYUV[3*(y*p_width + x)+1];
    components[1] = dstYUV[3*(y*p_width + x)+2];
    return ret;
}


