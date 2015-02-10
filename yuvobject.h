#ifndef YUVOBJECT_H
#define YUVOBJECT_H

#include <QObject>
#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <QImage>

#include "videofile.h"

class YUVObject : public QObject
{
    Q_OBJECT

public:
    YUVObject(const QString& srcFileName, QObject* parent = 0);

    ~YUVObject();

    void loadFrame(unsigned int frameIdx);
    QImage frameImage() { return p_frameImage; }

    QString name() { return p_name; }
    QString path() {return p_srcFile->getPath();}
    QString createdtime() {return p_srcFile->getCreatedtime();}
    QString modifiedtime() {return p_srcFile->getModifiedtime();}

    int width();
    int height();
    int numFrames() { return p_numFrames; }
    bool playUntilEnd() { return p_playUntilEnd; }
    float frameRate() { return p_frameRate; }
    int getNumFramesFromFileSize() { int numFrames=0; p_srcFile->refreshNumberFrames(&numFrames, p_width, p_height, p_colorFormat, p_bitPerPixel); return numFrames; }

    ColorFormat colorFormat() { return p_colorFormat; }
    int bitPerPixel() { return p_bitPerPixel;}

    int startFrame() { return p_startFrame; }
    int sampling() { return p_sampling; }

    int getPixelValue(int x, int y);

public slots:

    void setWidth(int newWidth);
    void setHeight(int newHeight);
    void setColorFormat(int newFormat);
    void setFrameRate(double newRate);
    void setNumFrames(int newNumFrames);
    void setName(QString& newName);
    void setStartFrame(int newStartFrame);
    void setSampling(int newSampling);
    void setBitPerPixel(int bitPerPixel);
    void setInterpolationMode(int newMode);
    void setPlayUntilEnd(bool play) { p_playUntilEnd = play; }

signals:

    void informationChanged();

private:

    VideoFile* p_srcFile;
    QImage p_frameImage;

    QString p_name;

    int p_width;
    int p_height;
    int p_numFrames;
    bool p_playUntilEnd;
    int p_startFrame;
    double p_frameRate;
    int p_sampling;

    unsigned int p_lastFrameIdx;

    ColorFormat p_colorFormat;
    int p_bitPerPixel;

};

#endif // YUVOBJECT_H
