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
#include <QtEndian>
#include <QFileInfo>
#include <QString>
#include <QDateTime>
#include <QCache>
#include "typedef.h"
#include <map>
#include "yuvsource.h"

class YUVFile : public YUVSource
{
    Q_OBJECT
public:
    explicit YUVFile(const QString &fname, QObject *parent = 0);

    ~YUVFile();

    void extractFormat(int* width, int* height, int* numFrames, double* frameRate);
	
    // reads one frame in YUV444 into target byte array
    virtual void getOneFrame( QByteArray* targetByteArray, unsigned int frameIdx, int width, int height );

	// Return the source file name
	virtual QString getName();

    //  methods for querying file information
    virtual QString getPath() {return p_path;}
    virtual QString getCreatedtime() {return p_createdtime;}
    virtual QString getModifiedtime() {return p_modifiedtime;}
    virtual qint64  getNumberBytes() {return getFileSize();}
    virtual QString getStatus(int width, int height);

	// Set the source pixel format. 
    void setSrcPixelFormat(YUVCPixelFormatType newFormat) { p_srcPixelFormat = newFormat; emit yuvInformationChanged(); }

	// Try to get the format from the given file name
	static void formatFromFilename(QString filePath, int* width, int* height, double* frameRate, int* numFrames,int* bitDepth, YUVCPixelFormatType* cFormat , bool isYUV=true);

	// Get the number of frames from the file size for the given size
	qint64 getNumberFrames(int width, int height);

private:

    QFile *p_srcFile;

    QString p_path;
    QString p_createdtime;
    QString p_modifiedtime;

    virtual qint64 getFileSize();

    qint64 readFrame( QByteArray *targetBuffer, unsigned int frameIdx, int width, int height );

    // method tries to guess format information.
    void formatFromCorrelation(int* width, int* height, YUVCPixelFormatType* cFormat, int* numFrames);

    void readBytes( char* targetBuffer, qint64 startPos, qint64 length );

	// Temporaray buffer for conversion to YUV444
	QByteArray p_tmpBufferYUV;

};

#endif // YUVFILE_H
