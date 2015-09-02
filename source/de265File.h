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
#include <QFuture>

#define DE265_BUFFER_SIZE 8		//< The number of pictures allowed in the decoding buffer

class de265File :
	public YUVSource
{
public:
	de265File(const QString &fname, QObject *parent = 0);
	~de265File();

	void getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx);

	QString getName();

	virtual QString getPath() { return p_path; }
	virtual QString getCreatedtime() { return p_createdtime; }
	virtual QString getModifiedtime() { return p_modifiedtime; }
	virtual qint64  getNumberBytes() { return p_fileSize; }
	virtual qint64  getNumberFrames() { return p_numFrames; }
	virtual QString getStatus();

	// Setting functions for size/nrFrames/frameRate/pixelFormat.
	// All of these are predefined by the stream an cannot be set by the user.
	virtual void setSize(int width, int height) {}
	virtual void setFrameRate(double frameRate) {}
	virtual void setPixelFormat(YUVCPixelFormatType pixelFormat) {}

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

	// The number of rames in the sequence (that we know about);
	int p_numFrames;

	// Get the YUVCPixelFormatType from the image and set it
	void setDe265ChromaMode(const de265_image *img);
	// Copy the de265_image data to the QByteArray
	void copyImgTo444Buffer(const de265_image *src, QByteArray *dst);
	// Copy the raw data from the src to the byte array
	void copyImgToByteArray(const de265_image *src, QByteArray *dst);

	/// ===== Buffering
	QMap<int, QByteArray*> p_Buf_DecodedPictures;	///< The finished decoded pictures
	QList<QByteArray*>     p_Buf_EmptyBuffers;      ///< The empty buffers go in here
	
	// Access functions to the buffer. Always use these to keep everything thread save.
	QByteArray* getEmptyBuffer();
	void addEmptyBuffer(QByteArray *arr);

	QByteArray* popImageFromOutputBuffer(int *poc);
	void addImageToOutputBuffer(QByteArray *arr, int poc);
	

	//< which frame did the background process decode last?
	int p_lastFrameDecoded;
	
	// The decoder 
	QFuture<void> p_backgroundDecodingFuture;
	bool p_cancelBackgroundDecoder;		//< Set to true to cancel the background decoding process
	mutable QMutex p_mutex;				//< The mutex for accessing the thread save variables (the buffer and last frame POC)

	void backgroundDecoder();					//< The background decoding function.
	bool decodeOnePicture(QByteArray *buffer, bool emitSinals = true);  //< Decode one picture into the buffer. Return true on success.
};

#endif