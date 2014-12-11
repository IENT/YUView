#ifndef YUVFILE_H
#define YUVFILE_H

#include "videofile.h"

class YUVFile : public VideoFile
{
public:
    explicit YUVFile(const QString &fname, QObject *parent = 0);

    void extractFormat(int* width, int* height, ColorFormat* cFormat, int* numFrames, double* frameRate);

    void refreshNumberFrames(int* numFrames, int width, int height, ColorFormat cFormat, int bpp);

private:

    int getFrames( QByteArray *targetBuffer, unsigned int frameIdx, unsigned int frames2read, int width, int height, ColorFormat cFormat, int bpp );

    // method tries to guess format information, returns 'true' on success
    void formatFromCorrelation(int* width, int* height, ColorFormat* cFormat, int* numFrames);
    void formatFromFilename(int* width, int* height, double* frameRate, int* numFrames);

    void readBytes( QByteArray *targetBuffer, unsigned int startPos, unsigned int length );
    void scaleBytes( QByteArray *targetBuffer, unsigned int bpp, unsigned int numShorts );
    int bytesPerFrame(int width, int height, ColorFormat cFormat, int bpp);

};

#endif // YUVFILE_H
