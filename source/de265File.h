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

#ifndef DE265FILE_H
#define DE265FILE_H

#include "yuvsource.h"
#include "de265.h"
#include <QFile>

class de265File :
	public YUVSource
{
public:
	de265File(const QString &fname, QObject *parent = 0);
	~de265File();

	void getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx);

	// Get the number of frames (that we know about). Unfortunately no header will tell us how many
	// frames there are in a sequence. So we can just return the number of frames that we know about.
	qint64 getNumberFrames() { return p_nrKnownFrames;  }

	QString getName();

	virtual QString getPath() { return p_path; }
	virtual QString getCreatedtime() { return p_createdtime; }
	virtual QString getModifiedtime() { return p_modifiedtime; }
	virtual qint64  getNumberBytes() { return p_fileSize; }
	virtual QString getStatus();

protected:
	de265_decoder_context* p_decoder;

	// If everything is allright it will be DE265_OK
	de265_error p_decError;
	
	// The source file
	QFile *p_srcFile;

	// Info on the source file. Will be set when creating this object.
	QString p_path;
	QString p_createdtime;
	QString p_modifiedtime;
	qint64  p_fileSize;

	// Last decoded frame buffer (in YUV 444) and the POC of this frame
	QByteArray p_lastDecodedFrame;
	int p_lastDecodedPOC;

	int p_nrKnownFrames;

	// Get the YUVCPixelFormatType from the image and set it
	void setDe265ChromaMode(const de265_image *img);
	// Copy the de265_image data to the QByteArray
	void copyImgTo444Buffer(const de265_image *src, QByteArray *dst);
	// Copy the raw data from the src to the byte array
	void copyImgToByteArray(const de265_image *src, QByteArray *dst);
};

#endif