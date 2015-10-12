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

#if !YUVIEW_DISABLE_LIBDE265

#include "yuvsource.h"
#include "de265.h"
#include <QFile>
#include <QFuture>
#include <QQueue>
#include <QLibrary>

#define DE265_BUFFER_SIZE 8		//< The number of pictures allowed in the decoding buffer

typedef QPair<int, QByteArray*> queueItem;

// libde265 decoder library function pointers
typedef de265_decoder_context *(*f_de265_new_decoder)          ();
typedef void                   (*f_de265_set_parameter_bool)   (de265_decoder_context*, de265_param, int);
typedef void                   (*f_de265_set_parameter_int)    (de265_decoder_context*, de265_param, int);
typedef void                   (*f_de265_disable_logging)      ();
typedef void                   (*f_de265_set_verbosity)        (int);
typedef de265_error            (*f_de265_start_worker_threads) (de265_decoder_context*, int);
typedef void                   (*f_de265_set_limit_TID)        (de265_decoder_context*, int);
typedef const char*            (*f_de265_get_error_text)       (de265_error);
typedef de265_chroma           (*f_de265_get_chroma_format)    (const de265_image*);
typedef int                    (*f_de265_get_image_width)      (const de265_image*, int);
typedef int                    (*f_de265_get_image_height)     (const de265_image*, int);
typedef const uint8_t*         (*f_de265_get_image_plane)      (const de265_image*, int, int*);
typedef int                    (*f_de265_get_bits_per_pixel)   (const de265_image*, int);
typedef de265_error            (*f_de265_decode)               (de265_decoder_context*, int*);
typedef de265_error            (*f_de265_push_data)            (de265_decoder_context*, const void*, int, de265_PTS, void*);
typedef de265_error            (*f_de265_flush_data)           (de265_decoder_context*);
typedef const de265_image*     (*f_de265_get_next_picture)     (de265_decoder_context*);
typedef de265_error            (*f_de265_free_decoder)         (de265_decoder_context*);

class de265File :
	public YUVSource
{
public:
	de265File(const QString &fname, QObject *parent = 0);
	~de265File();

	void getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx);

	QString getName();

	virtual QString getType() { return QString("de265File"); }

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

	// Decoder library
	void loadDecoderLibrary();
	void allocateNewDecoder();
	QLibrary p_decLib;
	
	// Decoder library function pointers
	f_de265_new_decoder			  de265_new_decoder;
	f_de265_set_parameter_bool    de265_set_parameter_bool;
	f_de265_set_parameter_int	  de265_set_parameter_int;
	f_de265_disable_logging		  de265_disable_logging;
	f_de265_set_verbosity		  de265_set_verbosity;
	f_de265_start_worker_threads  de265_start_worker_threads;
	f_de265_set_limit_TID		  de265_set_limit_TID;
	f_de265_get_error_text		  de265_get_error_text;
	f_de265_get_chroma_format     de265_get_chroma_format;
	f_de265_get_image_width       de265_get_image_width;
	f_de265_get_image_height	  de265_get_image_height;
	f_de265_get_image_plane   	  de265_get_image_plane;
	f_de265_get_bits_per_pixel	  de265_get_bits_per_pixel;
	f_de265_decode 				  de265_decode;
	f_de265_push_data			  de265_push_data;
	f_de265_flush_data			  de265_flush_data;
	f_de265_get_next_picture	  de265_get_next_picture;
	f_de265_free_decoder    	  de265_free_decoder;

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
	QQueue<queueItem>  p_Buf_DecodedPictures;				///< The finished decoded pictures
	QList<QByteArray*> p_Buf_EmptyBuffers;					///< The empty buffers go in here
	QByteArray*        p_Buf_CurrentOutputBuffer;			///< The buffer that was requested in the last call to getOneFrame
	int                p_Buf_CurrentOutputBufferFrameIndex;	///< The frame index of the buffer in p_Buf_CurrentOutputBuffer
	
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

	// Status reporting
	QString p_StatusText;
	bool p_internalError;
};

#endif // !YUVIEW_DISABLE_LIBDE265

#endif