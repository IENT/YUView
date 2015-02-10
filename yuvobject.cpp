#include "yuvobject.h"

#include "yuvfile.h"

YUVObject::YUVObject(const QString& srcFileName, QObject* parent) : QObject(parent)
{
    QFileInfo fi(srcFileName);
    QString ext = fi.suffix();
    ext = ext.toLower();
    if( ext == "yuv" )
    {
        p_srcFile = new YUVFile(srcFileName);
    }
    else
        exit(1);

    // preset internal values
    p_startFrame = 0;
    p_sampling = 1;

    p_bitPerPixel = 8;

    p_width = -1;
    p_height = -1;
    p_colorFormat = INVALID;
    p_numFrames = -1;
    p_frameRate = -1;
    p_playUntilEnd = true;

    p_lastFrameIdx = INT_MAX;    // initialize with magic number ;)

    // try to extract format information
    p_srcFile->extractFormat(&p_width, &p_height, &p_colorFormat, &p_numFrames, &p_frameRate);

    // check returned values
    if(p_width < 0)
        p_width = 640;
    if(p_height < 0)
        p_height = 480;
    if(p_colorFormat == INVALID)
        p_colorFormat = YUV420;
    if(p_numFrames < 0)
        p_numFrames = 10;
    if(p_frameRate < 0)
        p_frameRate = 20.0;

    // set our name
    p_name = p_srcFile->fileName();
}

YUVObject::~YUVObject()
{
    delete p_srcFile;
}

void YUVObject::setWidth(int newWidth)
{
    p_width = newWidth;
    // force reload of current frame
    p_srcFile->clearCache();
    loadFrame(p_lastFrameIdx);

    emit informationChanged();
}

void YUVObject::setHeight(int newHeight)
{
    p_height = newHeight;
    // force reload of current frame
    p_srcFile->clearCache();
    loadFrame(p_lastFrameIdx);

    emit informationChanged();
}

int YUVObject::width()
{
    return p_width;
}

int YUVObject::height()
{
    return p_height;
}

void YUVObject::setColorFormat(int newFormat)
{
    p_colorFormat = (ColorFormat)newFormat;

    // also update number of frames
    p_srcFile->refreshNumberFrames(&p_numFrames, p_width,p_height,p_colorFormat,p_bitPerPixel);

    // force reload of current frame
    p_srcFile->clearCache();
    loadFrame(p_lastFrameIdx);

    emit informationChanged();
}

void YUVObject::setFrameRate(double newRate)
{
    p_frameRate = newRate;
    emit informationChanged();
}

void YUVObject::setName(QString& newName)
{
    p_name = newName;
    emit informationChanged();
}

void YUVObject::setNumFrames(int newNumFrames)
{
    p_numFrames = newNumFrames;
    emit informationChanged();
}

void YUVObject::setStartFrame(int newStartFrame)
{
    p_startFrame = newStartFrame;
    emit informationChanged();
}

void YUVObject::setSampling(int newSampling)
{
    p_sampling = newSampling;
    emit informationChanged();
}

void YUVObject::loadFrame(unsigned int frameIdx)
{
    if( p_srcFile == NULL )
        return;

    void* frameData = NULL;

    // load the corresponding frame from yuv file into the frame buffer
    p_srcFile->getOneFrame(frameData, frameIdx, p_width, p_height, p_colorFormat, p_bitPerPixel);

    p_lastFrameIdx = frameIdx;

    if( frameData == NULL )
        return;

    // update our QImage with frame buffer
    p_frameImage = QImage((uchar*)frameData, p_width, p_height, QImage::Format_RGB888);
}

void YUVObject::setBitPerPixel(int bitPerPixel)
{
    p_bitPerPixel = bitPerPixel;

    // also update number of frames
    p_srcFile->refreshNumberFrames(&p_numFrames, p_width,p_height,p_colorFormat,p_bitPerPixel);

    // force reload of current frame
    p_srcFile->clearCache();
    loadFrame(p_lastFrameIdx);

    emit informationChanged();
}

void YUVObject::setInterpolationMode(int newMode)
{
    // propagate to VideoFile
    p_srcFile->setInterPolationMode((InterpolationMode)newMode);

    // force reload of current frame
    p_srcFile->clearCache();
    loadFrame(p_lastFrameIdx);

    emit informationChanged();
}

int YUVObject::getPixelValue(int x, int y) {
    if ( (p_srcFile == NULL) || (x < 0) || (y < 0) || (x >= p_width) || (y >= p_height) )
        return 0;

    void* frameData = NULL;

    // load the corresponding frame from our yuv file into the frame buffer
    p_srcFile->getOneFrame(frameData, p_lastFrameIdx, p_width, p_height, p_colorFormat, p_bitPerPixel);

    char* dstYUV = static_cast<char*>(frameData);
    int ret=0;
    unsigned char *components = reinterpret_cast<unsigned char*>(&ret);
    components[3] = dstYUV[3*(y*p_width + x)+0];
    components[2] = dstYUV[3*(y*p_width + x)+1];
    components[1] = dstYUV[3*(y*p_width + x)+2];
    return ret;
}
