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

    int scaleFactor() { return p_scaleFactor; }
    void setScaleFactor(int scaleFactor);

    QString name() { return p_name; }

    int width() { return p_width; }
    int height() { return p_height; }

    int startFrame() { return p_startFrame; }
    int numFrames() { return p_numFrames; }
    float frameRate() { return p_frameRate; }
    int sampling() { return p_sampling; }
    bool playUntilEnd() { return p_playUntilEnd; }

    virtual ValuePairList getValuesAt(int x, int y) = 0; // needs to be implemented by subclass

signals:

    void informationChanged();

public slots:

    void setName(QString& newName) { p_name = newName; emit informationChanged(); }

    virtual void setWidth(int newWidth) { p_width = newWidth; emit informationChanged(); }
    virtual void setHeight(int newHeight) { p_height = newHeight; emit informationChanged(); }

    virtual void setFrameRate(double newRate) { p_frameRate = newRate; emit informationChanged(); }
    virtual void setNumFrames(int newNumFrames) { p_numFrames = newNumFrames; emit informationChanged(); }
    virtual void setStartFrame(int newStartFrame) { p_startFrame = newStartFrame; emit informationChanged(); }
    virtual void setSampling(int newSampling) { p_sampling = newSampling; emit informationChanged(); }
    virtual void setPlayUntilEnd(bool play) { p_playUntilEnd = play; emit informationChanged(); }

    virtual void refreshDisplayImage() { loadImage(p_lastIdx); }

protected:
    QPixmap p_displayImage;
    unsigned int p_lastIdx;

    QString p_name;

    int p_width;
    int p_height;

    int p_scaleFactor;

    // timing related member variables
    int p_numFrames;
    int p_startFrame;
    double p_frameRate;
    bool p_playUntilEnd;
    int p_sampling;
};

#endif // DISPLAYOBJECT_H
