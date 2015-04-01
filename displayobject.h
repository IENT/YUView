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

#ifndef DISPLAYOBJECT_H
#define DISPLAYOBJECT_H

#include <QObject>
#include <QPixmap>
#include "typedef.h"

typedef QPair<QString,QString> ValuePair;
typedef QList<ValuePair> ValuePairList;

class DisplayObject : public QObject
{
    Q_OBJECT
public:
    explicit DisplayObject(QObject *parent = 0);
    ~DisplayObject();

    virtual void loadImage(int idx) = 0;       // needs to be implemented by subclasses
    QPixmap displayImage() { return p_displayImage; }

    QString name() { return p_name; }

    int width() { return p_width; }
    int height() { return p_height; }

    QSize size() { return QSize(width(), height()); }

    int internalScaleFactor() { return p_internalScaleFactor; }

    // The start and end frame of the object
    int startFrame() { return p_startFrame; }
    int endFrame() { return p_endFrame; }
    // The number of frames in the object. The children shall overload this.
    // For example a YUV file will return the number of frames in the file depending on the set size/fileSize
    // whil a text object has no specific number of frames.
    virtual int numFrames() = 0;

    float frameRate() { return p_frameRate; }
    int sampling() { return p_sampling; }

    virtual ValuePairList getValuesAt(int x, int y) = 0; // needs to be implemented by subclass

signals:

    void informationChanged();

public slots:

    void setName(QString& newName) { p_name = newName; emit informationChanged(); }

    virtual void setWidth(int newWidth) { p_width = newWidth; emit informationChanged(); }
    virtual void setHeight(int newHeight) { p_height = newHeight; emit informationChanged(); }

    // sub-classes have to decide how to handle the scaling
    virtual void setInternalScaleFactor(int internalScaleFactor) = 0;

    virtual void setFrameRate(double newRate) { p_frameRate = newRate; emit informationChanged(); }
    virtual void setStartFrame(int newStartFrame) { p_startFrame = newStartFrame; emit informationChanged(); }
    virtual void setEndFrame(int newEndFrame) { p_endFrame = newEndFrame; emit informationChanged(); }
    virtual void setSampling(int newSampling) { p_sampling = newSampling; emit informationChanged(); }

    virtual void refreshNumberOfFrames() { setEndFrame(numFrames()-1); }

    virtual void refreshDisplayImage() { loadImage(p_lastIdx); }

protected:
    QPixmap p_displayImage;
    unsigned int p_lastIdx;

    QString p_name;

    int p_width;
    int p_height;

    int p_internalScaleFactor;

    // timing related member variables
    int p_startFrame;
    int p_endFrame;
    double p_frameRate;
    int p_sampling;
};

#endif // DISPLAYOBJECT_H
