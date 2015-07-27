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

#ifndef DIFFERENCEOBJECT_H
#define DIFFERENCEOBJECT_H

#include <QObject>
#include "frameobject.h"

class DifferenceObject : public FrameObject
{
public:
    DifferenceObject(QObject* parent = NULL);
    ~DifferenceObject();

    void setFrameObjects(FrameObject* firstObject, FrameObject* secondObject);

    void loadImage(int frameIdx);
    ValuePairList getValuesAt(int x, int y);

    void setInternalScaleFactor(int) {}    // no internal scaling
    void refreshDisplayImage()  { p_frameObjects[0]->clearCurrentCache();p_frameObjects[1]->clearCurrentCache();loadImage(p_lastIdx);}
    int numFrames();
    bool markDifferences(bool mark, QColor color){p_markDifferences = mark;diffColor = color; emit frameInformationChanged();return differenceExists;} // Todo

    void removeFrameObject(int idx) { if (idx == 0) {p_frameObjects[0] = p_frameObjects[1];} p_frameObjects[1] = NULL; }

private:
    FrameObject* p_frameObjects[2];

    void subtractYUV444(QByteArray *srcBuffer0, QByteArray *srcBuffer1, QByteArray *outBuffer, YUVCPixelFormatType srcPixelFormat);
protected:
    int p_markDifferences;
    bool differenceExists;
    QColor diffColor;
};

#endif // DIFFERENCEOBJECT_H
