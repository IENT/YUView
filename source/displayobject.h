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
#include "FileInfoGroupBox.h"

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

	virtual int internalScaleFactor() { return 1; }

    // The start and end frame of the object
    int startFrame() { return p_startFrame; }
	int endFrame() { return p_endFrame; }
    // The number of frames in the object. The children shall overload this.
    // For example a YUV file will return the number of frames in the file depending on the set size/fileSize
    // whil a text object has no specific number of frames.
    virtual int numFrames() = 0;

	// Calculate the minimum/maxiumum frame index depending on p_startFrame, p_endFrame and numFrames()
	void frameIndexLimits(int &minIdx, int &maxIdx);

    float frameRate() { return p_frameRate; }
    int sampling() { return p_sampling; }

    virtual ValuePairList getValuesAt(int x, int y) = 0; // needs to be implemented by subclass

    QString getStatusAndInfo();

    // Set info. If "permanent" is set it will be kept forever. If not set the next call to setInfo will overwrite it.
    // Return if the info was added. Permanent info is not set multiple times.
	bool setInfo(QString s, bool permament = false);

	// Return the infoTitle and the info list. This is the info that will be shown in the fileInfo groupBox.
	// Subclasses can overload this to show specific info.
	virtual QString getInfoTitle() { return QString("Info"); };
	virtual QList<fileInfoItem> getInfoList() { QList<fileInfoItem> l; l.append(fileInfoItem("Info", getStatusAndInfo())); return l; }

signals:
	// Just emit if some property of the object changed without the user (the GUI) being the reason for it.
	// This could for example be a background process that updates the number of frames or the status text ...
	void signal_objectInformationChanged();

public slots:

    void setName(QString& newName) { p_name = newName; }

	virtual void setSize(int width, int height) { p_width = width; p_height = height; refreshDisplayImage(); }
    
    // sub-classes have to decide how to handle the scaling
    virtual void setInternalScaleFactor(int internalScaleFactor) = 0;

    virtual void setFrameRate(double newRate) { p_frameRate = newRate; }
    virtual void setStartFrame(int newStartFrame) { p_startFrame = newStartFrame; }
    virtual void setEndFrame(int newEndFrame) { p_endFrame = newEndFrame; }
    virtual void setSampling(int newSampling) { p_sampling = newSampling; }

    virtual void refreshNumberOfFrames() { setEndFrame(numFrames()-1); }

    virtual void refreshDisplayImage() { loadImage(p_lastIdx); }

protected:
    QPixmap p_displayImage;
    unsigned int p_lastIdx;

    QString p_name;

    QString p_status;   // Contains the status as text
    QString p_info;     // Contains info (like cannot be displayed because ...)
    QList<QString> p_info_permanent;  // Contains permanent info

    int p_width;
    int p_height;

    // timing related member variables
    int p_startFrame;
    int p_endFrame;			// The end frame. All frames if -1.
    double p_frameRate;
    int p_sampling;
};

#endif // DISPLAYOBJECT_H
