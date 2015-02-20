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

    void loadImage(unsigned int frameIdx);

    int getPixelValue(int x, int y);

public slots:


private:

    VideoFile* p_srcFile;

    unsigned int p_lastFrameIdx;

};

#endif // FRAMEOBJECT_H
