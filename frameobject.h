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

    int numFrames() { p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height, p_colorFormat, p_bitPerPixel); return p_numFrames; }

    void setColorFormat(int newFormat) { p_colorFormat = (ColorFormat)newFormat; emit informationChanged(p_lastIdx); }
    void setBitPerPixel(int bitPerPixel) { p_bitPerPixel = bitPerPixel; emit informationChanged(p_lastIdx); }
    void setInterpolationMode(int newMode) { p_interpolationMode = newMode; emit informationChanged(p_lastIdx); }
    void setColorConversionMode(int newMode) { p_colorConversionMode = newMode; emit informationChanged(p_lastIdx); }

    ColorFormat colorFormat() { return p_colorFormat; }
    int interpolationMode() { return p_interpolationMode; }
    int colorConversionMode() { return p_colorConversionMode; }
    int bitPerPixel() { return p_bitPerPixel; }

    void loadImage(unsigned int frameIdx);

    int getPixelValue(int x, int y);

public slots:

    void refreshDisplayImage();

private:

    VideoFile* p_srcFile;

    // YUV to RGB conversion
    ColorFormat p_colorFormat;
    int p_interpolationMode;
    int p_colorConversionMode;
    int p_bitPerPixel;

};

#endif // FRAMEOBJECT_H
