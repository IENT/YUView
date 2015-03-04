#ifndef FRAMEOBJECT_H
#define FRAMEOBJECT_H

#include <QObject>
#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QImage>
#include "yuvfile.h"
#include "displayobject.h"

class CacheIdx
 {
 public:
     CacheIdx(const QString &name, const unsigned int idx) { fileName=name; frameIdx=idx; }

     QString fileName;
     unsigned int frameIdx;
 };

 inline bool operator==(const CacheIdx &e1, const CacheIdx &e2)
 {
     return e1.fileName == e2.fileName && e1.frameIdx == e2.frameIdx;
 }

 inline uint qHash(const CacheIdx &cIdx)
 {
     uint tmp = qHash(cIdx.fileName) ^ qHash(cIdx.frameIdx);
     return tmp;
 }

class FrameObject : public DisplayObject
{
    Q_OBJECT

public:
    FrameObject(const QString& srcFileName, QObject* parent = 0);

    ~FrameObject();

    QString path() {return p_srcFile->getPath();}
    QString createdtime() {return p_srcFile->getCreatedtime();}
    QString modifiedtime() {return p_srcFile->getModifiedtime();}

    void setNumFrames(int newNumFrames)
    {
        if(newNumFrames == 0)
            p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height, p_pixelFormat);
        else
            p_numFrames = newNumFrames;

        emit informationChanged(p_lastIdx);
    }
    int numFrames()
    {
        if( p_numFrames == 0 )
            p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height, p_pixelFormat);

        return p_numFrames;
    }

    void setPixelFormat(int newFormat) { p_pixelFormat = (YUVCPixelFormatType)newFormat; emit informationChanged(p_lastIdx); }
    void setInterpolationMode(int newMode) { p_interpolationMode = newMode; emit informationChanged(p_lastIdx); }
    void setColorConversionMode(int newMode) { p_colorConversionMode = newMode; emit informationChanged(p_lastIdx); }

    YUVCPixelFormatType pixelFormat() { return p_pixelFormat; }
    int interpolationMode() { return p_interpolationMode; }
    int colorConversionMode() { return p_colorConversionMode; }

    void loadImage(unsigned int frameIdx);

    int getPixelValue(int x, int y);

    static QCache<CacheIdx, QByteArray> frameCache;
    YUVFile *getyuvfile() {return p_srcFile;}

public slots:

    void refreshDisplayImage();

    void clearCache() { frameCache.clear(); }

private:

    YUVFile* p_srcFile;

    // YUV to RGB conversion
    YUVCPixelFormatType p_pixelFormat;
    int p_interpolationMode;
    int p_colorConversionMode;

};

#endif // FRAMEOBJECT_H
