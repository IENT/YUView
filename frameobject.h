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
    int     nrBytes() {return p_srcFile->getNumberBytes();}
    QString status() { return p_srcFile->getStatus(p_width, p_height); }

    void setInternalScaleFactor(int) {}    // no internal scaling

    // forward these parameters to our source file
    void setSrcPixelFormat(YUVCPixelFormatType newFormat) { p_srcFile->setSrcPixelFormat(newFormat); emit frameInformationChanged(); }
    void setInterpolationMode(InterpolationMode newMode) { p_srcFile->setInterpolationMode(newMode); emit frameInformationChanged(); }
    void setColorConversionMode(YUVCColorConversionType newMode) { p_colorConversionMode = newMode; emit frameInformationChanged(); }

    YUVCPixelFormatType pixelFormat() { return p_srcFile->pixelFormat(); }
    InterpolationMode interpolationMode() { return p_srcFile->interpolationMode(); }
    YUVCColorConversionType colorConversionMode() { return p_colorConversionMode; }

    void setLumaScale(int scale) {p_lumaScale = scale; emit frameInformationChanged(); }
    void setChromaUScale(int scale) {p_chromaUScale = scale; emit frameInformationChanged(); }
    void setChromaVScale(int scale) {p_chromaVScale = scale; emit frameInformationChanged(); }

    void setLumaOffset(int offset) {p_lumaOffset = offset; emit frameInformationChanged(); }
    void setChromaOffset(int offset) {p_chromaOffset = offset; emit frameInformationChanged(); }

    void setLumaInvert(bool invert) { p_lumaInvert = invert; emit frameInformationChanged(); }
    void setChromaInvert(bool invert) { p_chromaInvert = invert; emit frameInformationChanged(); }

    bool doApplyYUVMath() { return p_lumaScale!=1 || p_lumaOffset!=125 || p_chromaOffset!=128 || p_chromaUScale!=1 || p_chromaVScale!=1 || p_lumaInvert!=0 || p_chromaInvert!=0; }

    void loadImage(int frameIdx);

    ValuePairList getValuesAt(int x, int y);

    static QCache<CacheIdx, QPixmap> frameCache;

    YUVFile *getYUVFile() {return p_srcFile;}

    // Return the number of frames in the file
    int numFrames() { return p_srcFile ? p_srcFile->getNumberFrames(p_width, p_height) : INT_INVALID; }
signals:
    void frameInformationChanged();

public slots:

    void refreshDisplayImage() {clearCurrentCache(); loadImage(p_lastIdx);}
    void propagateParameterChanges() { emit informationChanged(); }

    void clearCompleteCache() { frameCache.clear(); }
    void clearCurrentCache();
protected:

    void applyYUVMath(QByteArray *sourceBuffer, int lumaWidth, int lumaHeight, YUVCPixelFormatType srcPixelFormat);
    void convertYUV2RGB(QByteArray *sourceBuffer, QByteArray *targetBuffer, YUVCPixelFormatType targetPixelFormat);

    YUVFile* p_srcFile;

    QByteArray p_PixmapConversionBuffer;
    QByteArray p_tmpBufferYUV444;

    int p_lumaScale;
    int p_lumaOffset;
    int p_chromaOffset;
    int p_chromaUScale;
    int p_chromaVScale;
    bool p_lumaInvert;
    bool p_chromaInvert;

    YUVCColorConversionType p_colorConversionMode;
};

#endif // FRAMEOBJECT_H
