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
            p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height);
        else
            p_numFrames = newNumFrames;

        emit informationChanged();
    }
    int numFrames()
    {
        if( p_numFrames == 0 )
            p_srcFile->refreshNumberFrames(&p_numFrames, p_width, p_height);

        return p_numFrames;
    }

    // forward these parameters to our source file
    void setSrcPixelFormat(YUVCPixelFormatType newFormat) { p_srcFile->setSrcPixelFormat(newFormat); emit informationChanged(); }
    void setInterpolationMode(InterpolationMode newMode) { p_srcFile->setInterpolationMode(newMode); emit informationChanged(); }
    void setColorConversionMode(YUVCColorConversionType newMode) { p_colorConversionMode = newMode; emit informationChanged(); }

    YUVCPixelFormatType pixelFormat() { return p_srcFile->pixelFormat(); }
    InterpolationMode interpolationMode() { return p_srcFile->interpolationMode(); }
    YUVCColorConversionType colorConversionMode() { return p_colorConversionMode; }

    void setLumaScale(int index) {p_lumaScale = index; emit informationChanged(); }
    void setUParameter(int value) {p_UParameter = value; emit informationChanged(); }
    void setVParameter(int value) {p_VParameter = value; emit informationChanged(); }

    void setLumaOffset(int arg1) {p_lumaOffset = arg1; emit informationChanged(); }
    void setChromaOffset(int arg1) {p_chromaOffset = arg1; emit informationChanged(); }

    void setLumaInvert(bool checked) { p_lumaInvert = checked; emit informationChanged(); }
    void setChromaInvert(bool checked) { p_chromaInvert = checked; emit informationChanged(); }

    bool doApplyYUVMath() { return p_lumaScale!=1 || p_lumaOffset!=125 || p_chromaOffset!=128 || p_UParameter!=1 || p_VParameter!=1 || p_lumaInvert!=0 || p_chromaInvert!=0; }

    void loadImage(unsigned int frameIdx);

    ValuePairList getValuesAt(int x, int y);

    static QCache<CacheIdx, QPixmap> frameCache;
    YUVFile *getyuvfile() {return p_srcFile;}

public slots:

    void refreshDisplayImage();
    void propagateParameterChanges() { emit informationChanged(); }

    void clearCache() { frameCache.clear(); }

protected:

    void applyYUVMath(QByteArray *sourceBuffer, int lumaWidth, int lumaHeight);
    void convertYUV2RGB(QByteArray *sourceBuffer, QByteArray *targetBuffer, YUVCPixelFormatType targetPixelFormat);

    YUVFile* p_srcFile;

    QByteArray p_PixmapConversionBuffer;
    QByteArray p_tmpBufferYUV444;

    int p_lumaScale;
    int p_lumaOffset;
    int p_chromaOffset;
    int p_UParameter;
    int p_VParameter;
    unsigned short p_lumaInvert;
    unsigned short p_chromaInvert;

    YUVCColorConversionType p_colorConversionMode;
};

#endif // FRAMEOBJECT_H
