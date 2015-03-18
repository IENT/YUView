/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

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
    QString p_name;
    int p_bitsPerSample;
    int p_bitsPerPixelNominator;
    int p_bitsPerPixelDenominator;
    int p_subsamplingHorizontal;
    int p_subsamplingVertical;
    bool p_planar;
};

typedef std::map<YUVCPixelFormatType,PixelFormat> PixelFormatMapType;

class YUVFile : public QObject
{
    Q_OBJECT
public:
    explicit YUVFile(const QString &fname, QObject *parent = 0);

    ~YUVFile();

    void extractFormat(int* width, int* height, int* numFrames, double* frameRate);

    void refreshNumberFrames(int* numFrames, int width, int height);

    // reads one frame in YUV444 into target byte array
    virtual void getOneFrame( QByteArray* targetByteArray, unsigned int frameIdx, int width, int height );

    virtual QString fileName();

    //  methods for querying file information
    virtual QString getPath() {return p_path;}
    virtual QString getCreatedtime() {return p_createdtime;}
    virtual QString getModifiedtime() {return p_modifiedtime;}

    void setSrcPixelFormat(YUVCPixelFormatType newFormat) { p_srcPixelFormat = newFormat; emit informationChanged(); }
    void setInterpolationMode(InterpolationMode newMode) { p_interpolationMode = newMode; emit informationChanged(); }

    YUVCPixelFormatType pixelFormat() { return p_srcPixelFormat; }
    InterpolationMode interpolationMode() { return p_interpolationMode; }

    static PixelFormatMapType pixelFormatList();
    static int verticalSubSampling(YUVCPixelFormatType pixelFormat);
    static int horizontalSubSampling(YUVCPixelFormatType pixelFormat);
    static int bitsPerSample(YUVCPixelFormatType pixelFormat);
    static int bytesPerFrame(int width, int height, YUVCPixelFormatType cFormat);
    static bool isPlanar(YUVCPixelFormatType pixelFormat);

    static void formatFromFilename(QString filePath, int* width, int* height, double* frameRate, int* numFrames, bool isYUV=true);

private:

    QFile *p_srcFile;
    QFileInfo fileInfo;

    QString p_path;
    QString p_createdtime;
    QString p_modifiedtime;

    QByteArray p_tmpBufferYUV;

    // YUV to RGB conversion
    YUVCPixelFormatType p_srcPixelFormat;
    InterpolationMode p_interpolationMode;

    virtual unsigned int getFileSize();

    void convert2YUV444(QByteArray *sourceBuffer, int lumaWidth, int lumaHeight, QByteArray *targetBuffer);

    static PixelFormatMapType g_pixelFormatList;

    int readFrame( QByteArray *targetBuffer, unsigned int frameIdx, int width, int height );

    // method tries to guess format information, returns 'true' on success
    void formatFromCorrelation(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames);

    void readBytes( char* targetBuffer, unsigned int startPos, unsigned int length );
    void scaleBytes( char *targetBuffer, unsigned int bpp, unsigned int numShorts );

signals:
    void informationChanged();

};

#endif // YUVFILE_H
