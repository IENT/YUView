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

#include "frameobject.h"

#include "yuvfile.h"
#include <QPainter>

QCache<CacheIdx, QPixmap> FrameObject::frameCache;

FrameObject::FrameObject(const QString& srcFileName, QObject* parent) : DisplayObject(parent)
{
    QFileInfo fi(srcFileName);
    QString ext = fi.suffix();
    ext = ext.toLower();
    if( ext == "yuv" )
    {
        p_srcFile = new YUVFile(srcFileName);
        QObject::connect(p_srcFile, SIGNAL(informationChanged()), this, SLOT(propagateParameterChanges()));
        QObject::connect(p_srcFile, SIGNAL(informationChanged()), this, SLOT(refreshDisplayImage()));
    }
    else
    {
        exit(1);
    }

    // try to extract format information
    p_srcFile->extractFormat(&p_width, &p_height, &p_numFrames, &p_frameRate);

    // check returned values
    if(p_width < 0)
        p_width = 640;
    if(p_height < 0)
        p_height = 480;
    if(p_numFrames < 0)
        p_numFrames = 1;
    if(p_frameRate < 0)
        p_frameRate = 30.0;

    // set our name (remove file extension)
    int lastPoint = p_srcFile->fileName().lastIndexOf(".");
    p_name = p_srcFile->fileName().left(lastPoint);
}

FrameObject::~FrameObject()
{
    delete p_srcFile;
}

void FrameObject::loadImage(unsigned int frameIdx)
{
    if( p_srcFile == NULL )
        return;

    // check if we have this frame index in our cache already
    CacheIdx cIdx(p_srcFile->fileName(), frameIdx);
    QPixmap* cachedFrame = frameCache.object(cIdx);
    if( cachedFrame == NULL )    // load the corresponding frame from yuv file into the frame buffer
    {
        // add new QPixmap to cache and use its data buffer
        cachedFrame = new QPixmap();

        p_srcFile->getOneFrame(&p_PixmapConversionBuffer, frameIdx, p_width, p_height);

        // add this frame into our cache, use MBytes as cost
        int sizeInMB = p_PixmapConversionBuffer.size() >> 20;

        // Convert the image in p_PixmapConversionBuffer to a QPixmap
        QImage tmpImage((unsigned char*)p_PixmapConversionBuffer.data(),p_width,p_height,QImage::Format_RGB888);
        cachedFrame->convertFromImage(tmpImage);

        frameCache.insert(cIdx, cachedFrame, sizeInMB);
    }

    p_lastIdx = frameIdx;

    // TODO: do we need to check this here?
    if( cachedFrame->isNull() )
        return;

    // update our QImage with frame buffer
    p_displayImage = *cachedFrame;
}

// this slot is called when some parameters of the frame change
void FrameObject::refreshDisplayImage()
{
    clearCache();
    loadImage(p_lastIdx);
}

ValuePairList FrameObject::getValuesAt(int x, int y)
{
    if ( (p_srcFile == NULL) || (x < 0) || (y < 0) || (x >= p_width) || (y >= p_height) )
        return ValuePairList();

    QByteArray yuvByteArray;
    p_srcFile->getOneFrame(&yuvByteArray, p_lastIdx, p_width, p_height, YUVC_444YpCbCr8PlanarPixelFormat);

    const unsigned int planeLength = p_width*p_height;

    const unsigned char valY = yuvByteArray.data()[y*p_width+x];
    const unsigned char valU = yuvByteArray.data()[planeLength+(y*p_width+x)];
    const unsigned char valV = yuvByteArray.data()[2*planeLength+(y*p_width+x)];

    ValuePairList values;

    values.append( ValuePair("Y", QString::number(valY)) );
    values.append( ValuePair("U", QString::number(valU)) );
    values.append( ValuePair("V", QString::number(valV)) );

    return values;
}


