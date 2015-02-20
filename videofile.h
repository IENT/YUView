#ifndef VIDEOFILE_H
#define VIDEOFILE_H

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDateTime>
#include <QCache>
#include "typedef.h"

enum InterpolationMode
{
    NearestNeighbor,
    BiLinear
};

typedef struct {
    float colorFactor;
    int name;
} formatProp_t;

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


class VideoFile : public QObject
{
public:
    explicit VideoFile(const QString &fname, QObject *parent = 0);

    ~VideoFile();

public:

    virtual void getOneFrame( void* &frameData, unsigned int frameIdx, int width, int height, ColorFormat srcFormat, int bpp );

    virtual void extractFormat(int* width, int* height, ColorFormat* cFormat, int* numFrames, double* frameRate) = 0;

    virtual void refreshNumberFrames(int* numFrames, int width, int height, ColorFormat cFormat, int bpp) = 0;

    virtual QString fileName();

    void clearCache();

    //  methods for querying file information
    virtual QString getPath() {return p_path;}
    virtual QString getCreatedtime() {return p_createdtime;}
    virtual QString getModifiedtime() {return p_modifiedtime;}

    void setInterPolationMode(InterpolationMode newMode) { p_interpolationMode = newMode; }
    InterpolationMode getInterpolationMode() { return p_interpolationMode; }

    static QCache<CacheIdx, QByteArray> frameCache;

protected:
    QFile *p_srcFile;
    QFileInfo fileInfo;

    QString p_path;
    QString p_createdtime;
    QString p_modifiedtime;

    QByteArray p_tmpBuffer;

    InterpolationMode p_interpolationMode;

    virtual unsigned int getFileSize();

    virtual int getFrames( QByteArray *targetBuffer, unsigned int frameIndex, unsigned int frames2read, int width, int height, ColorFormat cFormat, int bpp ) = 0;

    int convert2YUV444Interleave(QByteArray *sourceBuffer, ColorFormat cFormat, int componentWidth, int componentHeight, QByteArray *targetBuffer);
    void convertYUV4442RGB888(QByteArray *buffer, int bps=8);
};

#endif // VIDEOFILE_H
