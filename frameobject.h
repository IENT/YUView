#ifndef FRAMEOBJECT_H
#define FRAMEOBJECT_H

#include <QObject>
#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QImage>
#include "videofile.h"
#include "displayobject.h"

class FrameObject : public DisplayObject
{
    Q_OBJECT

public:
    FrameObject(const QString& srcFileName, QObject* parent = 0);

    ~FrameObject();

    QString path() {return p_srcFile->getPath();}
    QString createdtime() {return p_srcFile->getCreatedtime();}
    QString modifiedtime() {return p_srcFile->getModifiedtime();}

    int numFrames() { p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height, p_pixelFormat); return p_numFrames; }

    void setColorFormat(int newFormat) { p_pixelFormat = (YUVCPixelFormatType)newFormat; emit informationChanged(p_lastIdx); }
    void setInterpolationMode(int newMode) { p_interpolationMode = newMode; emit informationChanged(p_lastIdx); }
    void setColorConversionMode(int newMode) { p_colorConversionMode = newMode; emit informationChanged(p_lastIdx); }

    YUVCPixelFormatType pixelFormat() { return p_pixelFormat; }
    int interpolationMode() { return p_interpolationMode; }
    int colorConversionMode() { return p_colorConversionMode; }

    void loadImage(unsigned int frameIdx);

    int getPixelValue(int x, int y);

public slots:

    void refreshDisplayImage();

private:

    VideoFile* p_srcFile;

    // YUV to RGB conversion
    YUVCPixelFormatType p_pixelFormat;
    int p_interpolationMode;
    int p_colorConversionMode;

};

#endif // FRAMEOBJECT_H
