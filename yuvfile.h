#ifndef YUVFILE_H
#define YUVFILE_H


#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDateTime>
#include <QCache>
#include "typedef.h"
#include <map>


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

 class PixelFormat
 {
 public:
     PixelFormat()
     {
         p_name = "";
         p_bitsPerSample = 8;
         p_bitsPerPixelNominator = 32;
         p_bitsPerPixelDenominator = 1;
         p_subsamplingHorizontal = 1;
         p_subsamplingVertical = 1;
         p_planar = true;
     }

     void setParams(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator, int subsamplingHorizontal, int subsamplingVertical, bool isPlanar)
     {
         p_name = name;
         p_bitsPerSample = bitsPerSample;
         p_bitsPerPixelNominator = bitsPerPixelNominator;
         p_bitsPerPixelDenominator = bitsPerPixelDenominator;
         p_subsamplingHorizontal = subsamplingHorizontal;
         p_subsamplingVertical = subsamplingVertical;
         p_planar = isPlanar;
     }

     ~PixelFormat() {}

     QString name() { return p_name; }
     int bitsPerSample() { return p_bitsPerSample; }
     int bitsPerPixelNominator() { return p_bitsPerPixelNominator; }
     int bitsPerPixelDenominator() { return p_bitsPerPixelDenominator; }
     int subsamplingHorizontal() { return p_subsamplingHorizontal; }
     int subsamplingVertical() { return p_subsamplingVertical; }
     int isPlanar() { return p_planar; }

 private:
     int p_identifier;
     QString p_name;
     int p_bitsPerSample;
     int p_bitsPerPixelNominator;
     int p_bitsPerPixelDenominator;
     int p_subsamplingHorizontal;
     int p_subsamplingVertical;
     bool p_planar;
 };


class YUVFile : public QObject
{
public:
    explicit YUVFile(const QString &fname, QObject *parent = 0);

    ~YUVFile();

    void extractFormat(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames, double* frameRate);

    void refreshNumberFrames(int* numFrames, int width, int height, YUVCPixelFormatType cFormat);

    virtual void getOneFrame( void* &frameData, unsigned int frameIdx, int width, int height, YUVCPixelFormatType srcFormat );

    //virtual void extractFormat(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames, double* frameRate) = 0;

    //virtual void refreshNumberFrames(int* numFrames, int width, int height, YUVCPixelFormatType cFormat) = 0;

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

    QByteArray p_tmpBufferYUV;
    QByteArray p_tmpBufferYUV444;

    InterpolationMode p_interpolationMode;

    std::map<YUVCPixelFormatType,PixelFormat> p_formatProperties;

    virtual unsigned int getFileSize();

    //virtual int getFrames( QByteArray *targetBuffer, unsigned int frameIndex, unsigned int frames2read, int width, int height, YUVCPixelFormatType srcPixelFormat ) = 0;

    void convert2YUV444(QByteArray *sourceBuffer, YUVCPixelFormatType srcPixelFormat, int lumaWidth, int lumaHeight, QByteArray *targetBuffer);
    void convertYUV2RGB(QByteArray *sourceBuffer, QByteArray *targetBuffer, YUVCPixelFormatType targetPixelFormat);

    int verticalSubSampling(YUVCPixelFormatType pixelFormat);
    int horizontalSubSampling(YUVCPixelFormatType pixelFormat);
    int bitsPerSample(YUVCPixelFormatType pixelFormat);
    int bytesPerFrame(int width, int height, YUVCPixelFormatType cFormat);
    bool isPlanar(YUVCPixelFormatType pixelFormat);

private:

    int getFrames( QByteArray *targetBuffer, unsigned int frameIdx, unsigned int frames2read, int width, int height, YUVCPixelFormatType cFormat );

    // method tries to guess format information, returns 'true' on success
    void formatFromCorrelation(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames);
    void formatFromFilename(int* width, int* height, double* frameRate, int* numFrames);

    void readBytes( QByteArray *targetBuffer, unsigned int startPos, unsigned int length );
    void scaleBytes( QByteArray *targetBuffer, unsigned int bpp, unsigned int numShorts );

};

#endif // YUVFILE_H
