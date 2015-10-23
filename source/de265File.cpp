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

#include "de265File.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QtConcurrent>
#include <assert.h>
#include <QThread>

#define BUFFER_SIZE 40960
#define SET_INTERNALERROR_RETURN(errTxt) { p_internalError = true; p_StatusText = errTxt; return; }
#define DISABLE_INTERNALS_RETURN()       { p_internalsSupported = false; return; }

// Conversion from intra prediction mode to vector.
// Coordinates are in x,y with the axes going right and down.
#define VECTOR_SCALING 0.25
const int de265File::p_vectorTable[35][2] = { {0,0}, {0,0}, 
	{32, -32}, 
	{32, -26}, {32, -21}, {32, -17}, { 32, -13}, { 32,  -9}, { 32, -5}, { 32, -2}, 
	{32,   0},
	{32,   2}, {32,   5}, {32,   9}, { 32,  13}, { 32,  17}, { 32, 21}, { 32, 26}, 
	{32,  32},
	{26,  32}, {21,  32}, {17,  32}, { 13,  32}, {  9,  32}, {  5, 32}, {  2, 32},
	{0,   32},
	{-2,  32}, {-5,  32}, {-9,  32}, {-13,  32}, {-17,  32}, {-21, 32}, {-26, 32},
	{-32, 32} };

de265File::de265File(const QString &fname, QObject *parent)
	: YUVSource(parent)
{
	// Init variables
	p_numFrames = -1;
	p_lastFrameDecoded = -1;
	p_cancelBackgroundDecoder = false;
	p_decoder = NULL;
	p_StatusText = "OK";
	p_internalError = false;
	p_decError = DE265_OK;
	p_internalsSupported = true;
	p_RetrieveStatistics = false;

	// Open the input file
	p_srcFile = new QFile(fname);
	p_srcFile->open(QIODevice::ReadOnly);

	// get some more information from file
	QFileInfo fileInfo(p_srcFile->fileName());
	p_path = fileInfo.path();
	p_createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
	p_modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
	p_fileSize = fileInfo.size();

	// The buffer holding the last requested frame. (Empty when contructing this)
	// When using the zoom box the getOneFrame function is called frequently so we
	// keep this buffer to not decode the same frame over and over again.
	p_Buf_CurrentOutputBuffer = NULL;
	p_Buf_CurrentOutputBufferFrameIndex = -1;
	
	loadDecoderLibrary();

	if (p_internalError)
		// There was an internal error while loading/initializing the decoder. Abort.
		return;

	allocateNewDecoder();

	// Fill the empty buffer list with new buffers
	for (size_t i = 0; i < DE265_BUFFER_SIZE; i++) {
		p_Buf_EmptyBuffers.append(new QByteArray());
	}

	// Decode one picture. After this the size of the sequence is correctly set.
	QByteArray *emptyBuffer = getEmptyBuffer();
	if (decodeOnePicture(emptyBuffer, false)) {
		// The frame is ready for output
		addImageToOutputBuffer(emptyBuffer, p_lastFrameDecoded);
	}

	// Fill the list of statistics that we can provide
	fillStatisticList();

	// Start the background decoding process
	p_backgroundDecodingFuture = QtConcurrent::run(this, &de265File::backgroundDecoder);
}

de265File::~de265File()
{
}

QString de265File::getName()
{
	QStringList components = p_srcFile->fileName().split(QDir::separator());
	return components.last();
}

QString de265File::getStatus()
{
	if (p_decError != DE265_OK) {
		// Convert the error (p_decError) to text and return it.
		QString errText = QString(de265_get_error_text(p_decError));
		errText += "\n" + p_StatusText;
		return errText;
	}

	return p_StatusText;
}

void de265File::getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx)
{
	//printf("Request %d(%d)", frameIdx, p_numFrames);
	if (p_internalError) 
		return;

	// At first check if the request is for the frame that has been requested in the 
	// last call to this function.
	if (frameIdx == p_Buf_CurrentOutputBufferFrameIndex) {
		assert(p_Buf_CurrentOutputBuffer != NULL);
		if (targetByteArray)
			*targetByteArray = *p_Buf_CurrentOutputBuffer;
		return;
	}

	// Get pictures from the buffer until we find the right one
	QByteArray *pic = NULL;
	int poc = -1;
	while (poc != frameIdx) {
		// Rigth image not found yet. Get the next image from the decoder
		while (p_Buf_DecodedPictures.count() == 0) {
			// The image was not found the the decoded picture queue.
			// Check if we are requesting a frame which lies before the current decoder state
			if ((int)frameIdx < p_Buf_CurrentOutputBufferFrameIndex && (int)frameIdx < p_lastFrameDecoded) {
				// Restart decoding at the beginning

				if (p_backgroundDecodingFuture.isRunning()) {
					// Stop the running decoder
					p_cancelBackgroundDecoder = true;
					p_backgroundDecodingFuture.waitForFinished();
					p_cancelBackgroundDecoder = false;
				}

				// Delete decoder
				de265_error err = de265_free_decoder(p_decoder);
				if (err != DE265_OK) {
					// Freeing the decoder failed.
					if (p_decError != err) {
						p_decError = err;
					}
					return;
				}
				p_lastFrameDecoded = -1;
				p_Buf_CurrentOutputBufferFrameIndex = -1;
				p_decoder = NULL;

				// Create new decoder
				allocateNewDecoder();

				// Rewind file
				p_srcFile->seek(0);
			}

			if (!p_backgroundDecodingFuture.isRunning()) {
				// The background process is not running. Start is back up or we will wait forever.
				p_backgroundDecodingFuture = QtConcurrent::run(this, &de265File::backgroundDecoder);
				//qDebug() << "Restart background process while waiting.";
			}
			// The buffer is still empty. Give the decoder some time (50ms)
			QThread::msleep(50);
		}
		pic = popImageFromOutputBuffer(&poc);
		
		if (poc != frameIdx) {
			// Wrong poc. Discard the buffer.
			//printf("Discard %d...", poc);
			addEmptyBuffer(pic);
			//qDebug() << "This is not the frame we are looking for.";
		}
	}

	assert(frameIdx == poc && pic != NULL);
	if (targetByteArray)
		*targetByteArray = *pic;

	// The frame that is in the p_Buf_CurrentOutputBuffer (if any) can now be overwritten by a new frame
	if (p_Buf_CurrentOutputBuffer != NULL)
		addEmptyBuffer(p_Buf_CurrentOutputBuffer);

	// Move the frame that was just returned to p_Buf_CurrentOutputBuffer
	p_Buf_CurrentOutputBuffer = pic;
	p_Buf_CurrentOutputBufferFrameIndex = frameIdx;
	
	if (!p_backgroundDecodingFuture.isRunning()) {
		// The background process is not running. Start is back up.
		p_backgroundDecodingFuture = QtConcurrent::run(this, &de265File::backgroundDecoder);
		//qDebug() << "Restart backgroudn process.";
	}
}

bool de265File::decodeOnePicture(QByteArray *buffer, bool emitSinals)
{
	if (buffer == NULL)
		return false;

	de265_error err;
	while (!p_cancelBackgroundDecoder) {
		int more = 1;
		while (more) {
			more = 0;

			err = de265_decode(p_decoder, &more);
			while (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && !p_srcFile->atEnd()) {
				// The decoder needs more data. Get it from the file.
				char buf[BUFFER_SIZE];
				int64_t pos = p_srcFile->pos();
				int n = p_srcFile->read(buf, BUFFER_SIZE);

				// Push the data to the decoder
				if (n > 0) {
					err = de265_push_data(p_decoder, buf, n, pos, (void*)2);
					if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA) {
						// An error occured
						if (emitSinals && p_decError != err) {
							p_decError = err;
							emit signal_sourceStatusChanged();
						}
						return false;
					}
				}

				if (p_srcFile->atEnd()) {
					// The file ended.
					err = de265_flush_data(p_decoder);
				}
			}

			if (err == DE265_ERROR_WAITING_FOR_INPUT_DATA && p_srcFile->atEnd()) {
				// The decoder wants more data but there is no more file.
				// We found the end of the sequence. Get the remaininf frames from the decoder until
				// more is 0.
			}
			else if (err != DE265_OK) {
				// Something went wrong
				more = 0;
				break;
			}

			const de265_image* img = de265_get_next_picture(p_decoder);
			if (img) {
				// We have recieved an output image
				// Update the number of frames (that we know about). Unfortunately no header will tell us how many
				// frames there are in a sequence. So we can just return the number of frames that we know about.
				p_lastFrameDecoded++;
				if (more && p_lastFrameDecoded + 2 > getNumberFrames()) {
					// If more is true, we are not done decoding yet. So the current POC is not the last POC.
					// The next one, however, may be.
					p_numFrames = p_lastFrameDecoded + 2;
					// The number of frames changed in the background
					if (emitSinals) emit signal_sourceNrFramesChanged();
				}
				// Update size and format
				p_width = de265_get_image_width(img, 0);
				p_height = de265_get_image_height(img, 0);

				// Put array into buffer
				copyImgTo444Buffer(img, buffer);

				if (p_RetrieveStatistics)
					// Get the statistics from the image and put them into the statistics cache
					cacheStatistics(img, p_lastFrameDecoded);

				// Picture decoded
				return true;
			}
		}

		if (err != DE265_OK) {
			// The encoding loop ended becuase of an error
			if (emitSinals && p_decError != err) {
				p_decError = err;
				emit signal_sourceStatusChanged();
			}
			return false;
		}
		if (more == 0) {
			// The loop ended because there is nothing more to decode but no error occured.
			// We have found the real end of the sequence.
			p_numFrames = p_lastFrameDecoded + 1;
			
			// Reset the decoder and restart decoding from the beginning

			// Delete decoder
			err = de265_free_decoder(p_decoder);
			if (err != DE265_OK) {
				// Freeing the decoder failed.
				if (emitSinals && p_decError != err) {
					p_decError = err;
					emit signal_sourceStatusChanged();
				}
				return false;
			}
			p_lastFrameDecoded = -1;
			p_decoder = NULL;

			// Create new decoder
			allocateNewDecoder();
			
			// Rewind file
			p_srcFile->seek(0);
		}
	}

	// Background parser was canceled
	return false;
}

void de265File::backgroundDecoder()
{
	// Decode until there are no more empty buffers to put the decoded images.
	QByteArray *emptyBuffer = getEmptyBuffer();
	while (emptyBuffer != NULL && !p_cancelBackgroundDecoder) {
		if (decodeOnePicture(emptyBuffer)) {
			// The frame is ready for output
			addImageToOutputBuffer(emptyBuffer, p_lastFrameDecoded);

			// Get next empty buffer
			emptyBuffer = getEmptyBuffer();
		}
		else {
			// An error occured.
			addEmptyBuffer(emptyBuffer);
			return;
		}
	}
}

void de265File::copyImgTo444Buffer(const de265_image *src, QByteArray *dst)
{
	// First update the chroma format
	setDe265ChromaMode(src);

	// check if we need to do chroma upsampling
	if (p_srcPixelFormat != YUVC_444YpCbCr8PlanarPixelFormat && p_srcPixelFormat != YUVC_444YpCbCr12NativePlanarPixelFormat && p_srcPixelFormat != YUVC_444YpCbCr16NativePlanarPixelFormat && p_srcPixelFormat != YUVC_24RGBPixelFormat)
	{
		// copy one frame into temporary buffer
		copyImgToByteArray(src, &p_tmpBufferYUV);
		// convert original data format into YUV444 planar format
		convert2YUV444(&p_tmpBufferYUV, p_width, p_height, dst);
	}
	else    // source and target format are identical --> no conversion necessary
	{
		// read one frame into cached frame (already in YUV444 format)
		copyImgToByteArray(src, dst);
	}
}

void de265File::copyImgToByteArray(const de265_image *src, QByteArray *dst)
{
	// How many image planes are there?
	de265_chroma cMode = de265_get_chroma_format(src);
	int nrPlanes = (cMode == de265_chroma_mono) ? 1 : 3;
	
	// At first get how many bytes we are going to write
	int nrBytes = 0;
	int stride;
	for (int c = 0; c < nrPlanes; c++)
	{
		int width = de265_get_image_width(src, c);
		int height = de265_get_image_height(src, c);
		int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;

		nrBytes += width * height * nrBytesPerSample;
	}

	// Is the output big enough?
	if (dst->capacity() < nrBytes) {
		dst->resize(nrBytes);
	}

	// We can now copy from src to dst
	char* dst_c = dst->data();
	for (int c = 0; c < nrPlanes; c++)
	{
		const uint8_t* img_c = de265_get_image_plane(src, c, &stride);
		int width = de265_get_image_width(src, c);
		int height = de265_get_image_height(src, c);
		int nrBytesPerSample = (de265_get_bits_per_pixel(src, c) > 8) ? 2 : 1;				
		size_t size = width * nrBytesPerSample;
		
		for (int y = 0; y < height; y++) {
			memcpy(dst_c, img_c, size);
			img_c += stride;
			dst_c += size;
		}
	}
}


/* Convert the de265_chroma format to a YUVCPixelFormatType and set it.
*/
void de265File::setDe265ChromaMode(const de265_image *img)
{
	de265_chroma cMode = de265_get_chroma_format(img);
	int nrBitsC0 = de265_get_bits_per_pixel(img, 0);
	if (cMode == de265_chroma_mono && nrBitsC0 == 8) {
		p_srcPixelFormat = YUVC_8GrayPixelFormat;
	}
	else if (cMode == de265_chroma_420 && nrBitsC0 == 8) {
		p_srcPixelFormat = YUVC_420YpCbCr8PlanarPixelFormat;
	}
	else if (cMode == de265_chroma_420 && nrBitsC0 == 10) {
		p_srcPixelFormat = YUVC_420YpCbCr10LEPlanarPixelFormat;
	}
	else if (cMode == de265_chroma_422 && nrBitsC0 == 8) {
		p_srcPixelFormat = YUVC_422YpCbCr8PlanarPixelFormat;
	}
	else if (cMode == de265_chroma_422 && nrBitsC0 == 10) {
		p_srcPixelFormat = YUVC_422YpCbCr10PixelFormat;
	}
	else if (cMode == de265_chroma_444 && nrBitsC0 == 8) {
		p_srcPixelFormat = YUVC_444YpCbCr8PlanarPixelFormat;
	}
	else if (cMode == de265_chroma_444 && nrBitsC0 == 10) {
		p_srcPixelFormat = YUVC_444YpCbCr10LEPlanarPixelFormat;
	}
	else if (cMode == de265_chroma_444 && nrBitsC0 == 12) {
		p_srcPixelFormat = YUVC_444YpCbCr12LEPlanarPixelFormat;
	}
	else if (cMode == de265_chroma_444 && nrBitsC0 == 16) {
		p_srcPixelFormat = YUVC_444YpCbCr16LEPlanarPixelFormat;
	}
	else {
		p_srcPixelFormat = YUVC_UnknownPixelFormat;
	}
}

void de265File::addEmptyBuffer(QByteArray *arr)
{
	QMutexLocker locker(&p_mutex);
	p_Buf_EmptyBuffers.append(arr);
}

QByteArray* de265File::getEmptyBuffer() 
{ 
	QMutexLocker locker(&p_mutex); 
	if (p_Buf_EmptyBuffers.isEmpty())
		return NULL;
	return p_Buf_EmptyBuffers.takeFirst();
}

void de265File::addImageToOutputBuffer(QByteArray *arr, int poc)
{
	QMutexLocker locker(&p_mutex);
	p_Buf_DecodedPictures.enqueue(queueItem(poc, arr));
}

QByteArray* de265File::popImageFromOutputBuffer(int *poc)
{
	QMutexLocker locker(&p_mutex);
	queueItem itm = p_Buf_DecodedPictures.dequeue();
	*poc = itm.first;
	return itm.second;
}

void de265File::loadDecoderLibrary()
{
	// Try to load the libde265 library from the current working directory
	// Unfortunately relative paths like this do not work: (at leat on windows)
	//p_decLib.setFileName(".\\libde265");

#ifdef Q_OS_MAC
	// If the file name is not set explicitly, QLibrary will try to open
	// the libde265.so file first. Since this has been compiled for linux
	// it will fail and not even try to open the libde265.dylib
	QString libName = "/libde265.dylib";
#else
	// On windows and linux omnitting the extension works
	QString libName = "/libde265";
#endif

	QString libDir = QDir::currentPath() + "/" + libName;
	p_decLib.setFileName(libDir);
    bool bLibLoaded = p_decLib.load();
    if (!bLibLoaded) {
		// Loading failed. Try subdirectory libde265
		QString strErr = p_decLib.errorString();
		libDir = QDir::currentPath() + "/libde265/" + libName;
        p_decLib.setFileName(libDir);
        bLibLoaded = p_decLib.load();
	}

	if (!bLibLoaded) {
		// Loading failed. Try the directory that the executable is in.
		libDir = QCoreApplication::applicationDirPath() + "/" + libName;
		p_decLib.setFileName(libDir);
        bLibLoaded = p_decLib.load();
	}

	if (!bLibLoaded) {
		// Loading failed. Try the subdirector libde265 of the directory that the executable is in.
		libDir = QCoreApplication::applicationDirPath() + "/libde265/" + libName;
		p_decLib.setFileName(libDir);
        bLibLoaded = p_decLib.load();
	}

    if (!bLibLoaded) {
		// Loading failed. Try system directories.
        QString strErr = p_decLib.errorString();
		libDir = "libde265";
        p_decLib.setFileName(libDir);
        bLibLoaded = p_decLib.load();
	}

    if (!bLibLoaded)
		// Loading still failed.
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: " + p_decLib.errorString())
	
	// Get/check function pointers
	de265_new_decoder = (f_de265_new_decoder)p_decLib.resolve("de265_new_decoder");
	if (!de265_new_decoder)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_new_decoder was not found.")

	de265_set_parameter_bool = (f_de265_set_parameter_bool)p_decLib.resolve("de265_set_parameter_bool");
	if (!de265_set_parameter_bool)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_parameter_bool was not found.")

	de265_set_parameter_int = (f_de265_set_parameter_int)p_decLib.resolve("de265_set_parameter_int");
	if (!de265_set_parameter_int)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_parameter_int was not found.")

	de265_disable_logging = (f_de265_disable_logging)p_decLib.resolve("de265_disable_logging");
	if (!de265_disable_logging)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_disable_logging was not found.")

	de265_set_verbosity= (f_de265_set_verbosity)p_decLib.resolve("de265_set_verbosity");
	if (!de265_set_verbosity)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_verbosity was not found.")

	de265_start_worker_threads= (f_de265_start_worker_threads)p_decLib.resolve("de265_start_worker_threads");
	if (!de265_start_worker_threads)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_start_worker_threads was not found.")

	de265_set_limit_TID = (f_de265_set_limit_TID)p_decLib.resolve("de265_set_limit_TID");
	if (!de265_set_limit_TID)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_set_limit_TID was not found.")

	de265_get_error_text = (f_de265_get_error_text)p_decLib.resolve("de265_get_error_text");
	if (!de265_get_error_text)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_error_text was not found.")

	de265_get_chroma_format = (f_de265_get_chroma_format)p_decLib.resolve("de265_get_chroma_format");
	if (!de265_get_chroma_format)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_chroma_format was not found.")

	de265_get_image_width = (f_de265_get_image_width)p_decLib.resolve("de265_get_image_width");
	if (!de265_get_image_width)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_width was not found.")

	de265_get_image_height = (f_de265_get_image_height)p_decLib.resolve("de265_get_image_height");
	if (!de265_get_image_height)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_height was not found.")

	de265_get_image_plane = (f_de265_get_image_plane)p_decLib.resolve("de265_get_image_plane");
	if (!de265_get_image_plane)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_image_plane was not found.")

	de265_get_bits_per_pixel = (f_de265_get_bits_per_pixel)p_decLib.resolve("de265_get_bits_per_pixel");
	if (!de265_get_bits_per_pixel)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_bits_per_pixel was not found.")

	de265_decode = (f_de265_decode)p_decLib.resolve("de265_decode");
	if (!de265_decode)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_decode was not found.")

	de265_push_data = (f_de265_push_data)p_decLib.resolve("de265_push_data");
	if (!de265_push_data)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_push_data was not found.")

	de265_flush_data = (f_de265_flush_data)p_decLib.resolve("de265_flush_data");
	if (!de265_flush_data)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_flush_data was not found.")

	de265_get_next_picture = (f_de265_get_next_picture)p_decLib.resolve("de265_get_next_picture");
	if (!de265_get_next_picture)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_get_next_picture was not found.")

	de265_free_decoder = (f_de265_free_decoder)p_decLib.resolve("de265_free_decoder");
	if (!de265_free_decoder)
		SET_INTERNALERROR_RETURN("Error loading the libde265 library: The function de265_free_decoder was not found.")

	//// Get pointers to the internals/statistics functions (if present)
	//// If not, disable the statistics extraction. Normal decoding of the video will still work.

	de265_internals_get_CTB_Info_Layout = (f_de265_internals_get_CTB_Info_Layout)p_decLib.resolve("de265_internals_get_CTB_Info_Layout");
	if (!de265_internals_get_CTB_Info_Layout)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_CTB_sliceIdx = (f_de265_internals_get_CTB_sliceIdx)p_decLib.resolve("de265_internals_get_CTB_sliceIdx");
	if (!de265_internals_get_CTB_sliceIdx)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_CB_Info_Layout = (f_de265_internals_get_CB_Info_Layout)p_decLib.resolve("de265_internals_get_CB_Info_Layout");
	if (!de265_internals_get_CB_Info_Layout)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_CB_info = (f_de265_internals_get_CB_info)p_decLib.resolve("de265_internals_get_CB_info");
	if (!de265_internals_get_CB_info)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_PB_Info_layout = (f_de265_internals_get_PB_Info_layout)p_decLib.resolve("de265_internals_get_PB_Info_layout");
	if (!de265_internals_get_PB_Info_layout)
		DISABLE_INTERNALS_RETURN();
	
	de265_internals_get_PB_info = (f_de265_internals_get_PB_info)p_decLib.resolve("de265_internals_get_PB_info");
	if (!de265_internals_get_PB_info)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_IntraDir_Info_layout = (f_de265_internals_get_IntraDir_Info_layout)p_decLib.resolve("de265_internals_get_IntraDir_Info_layout");
	if (!de265_internals_get_IntraDir_Info_layout)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_intraDir_info = (f_de265_internals_get_intraDir_info)p_decLib.resolve("de265_internals_get_intraDir_info");
	if (!de265_internals_get_intraDir_info)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_TUInfo_Info_layout = (f_de265_internals_get_TUInfo_Info_layout)p_decLib.resolve("de265_internals_get_TUInfo_Info_layout");
	if (!de265_internals_get_TUInfo_Info_layout)
		DISABLE_INTERNALS_RETURN();

	de265_internals_get_TUInfo_info = (f_de265_internals_get_TUInfo_info)p_decLib.resolve("de265_internals_get_TUInfo_info");
	if (!de265_internals_get_TUInfo_info)
		DISABLE_INTERNALS_RETURN();
}

void de265File::allocateNewDecoder()
{
	if (p_decoder != NULL)
		return;

	// Create new decoder object
	p_decoder = de265_new_decoder();

	// Set some decoder parameters
	de265_set_parameter_bool(p_decoder, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH, false);
	de265_set_parameter_bool(p_decoder, DE265_DECODER_PARAM_SUPPRESS_FAULTY_PICTURES, false);

	de265_set_parameter_bool(p_decoder, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, false);
	de265_set_parameter_bool(p_decoder, DE265_DECODER_PARAM_DISABLE_SAO, false);

	// You could disanle SSE acceleration ... not really recommended
	//de265_set_parameter_int(p_decoder, DE265_DECODER_PARAM_ACCELERATION_CODE, de265_acceleration_SCALAR);

	de265_disable_logging();

	// Verbosity level (0...3(highest))
	de265_set_verbosity(0);

	// Set the number of decoder threads. Libde265 can use wavefronts to utilize these.
	p_decError = de265_start_worker_threads(p_decoder, 8);

	// The highest temporal ID to decode. Set this to very high (all) by default.
	de265_set_limit_TID(p_decoder, 100);
}

void de265File::fillStatisticList()
{
	if (!p_internalsSupported)
		return;

	StatisticsType sliceIdx(0, "Slice Index", colorRangeType, 0, QColor(0, 0, 0), 10, QColor(255,0,0));
	p_statsTypeList.append(sliceIdx);

	StatisticsType partSize(1, "Part Size", "jet", 0, 7);
	partSize.valMap.insert(0, "PART_2Nx2N");
	partSize.valMap.insert(1, "PART_2NxN");
	partSize.valMap.insert(2, "PART_Nx2N");
	partSize.valMap.insert(3, "PART_NxN");
	partSize.valMap.insert(4, "PART_2NxnU");
	partSize.valMap.insert(5, "PART_2NxnD");
	partSize.valMap.insert(6, "PART_nLx2N");
	partSize.valMap.insert(7, "PART_nRx2N");
	p_statsTypeList.append(partSize);

	StatisticsType predMode(2, "Pred Mode", "jet", 0, 2);
	predMode.valMap.insert(0, "INTRA");
	predMode.valMap.insert(1, "INTER");
	predMode.valMap.insert(2, "SKIP");
	p_statsTypeList.append(predMode);

	StatisticsType pcmFlag(3, "PCM flag", colorRangeType, 0, QColor(0, 0, 0), 1, QColor(255,0,0));
	p_statsTypeList.append(pcmFlag);

	StatisticsType transQuantBypass(4, "Transquant Bypass Flag", colorRangeType, 0, QColor(0, 0, 0), 1, QColor(255,0,0));
	p_statsTypeList.append(transQuantBypass);

	StatisticsType refIdx0(5, "Ref POC 0", "col3_bblg", -16, 16);
	p_statsTypeList.append(refIdx0);

	StatisticsType refIdx1(6, "Ref POC 1", "col3_bblg", -16, 16);
	p_statsTypeList.append(refIdx1);

	StatisticsType motionVec0(7, "Motion Vector 0", vectorType);
	motionVec0.colorRange = new DefaultColorRange("col3_bblg", -16, 16);
	p_statsTypeList.append(motionVec0);

	StatisticsType motionVec1(8, "Motion Vector 1", vectorType);
	motionVec1.colorRange = new DefaultColorRange("col3_bblg", -16, 16);
	p_statsTypeList.append(motionVec1);

	StatisticsType intraDirY(9, "Intra Dir Luma", "jet", 0, 34);
	intraDirY.valMap.insert(0, "INTRA_PLANAR");
	intraDirY.valMap.insert(1, "INTRA_DC");
	intraDirY.valMap.insert(2, "INTRA_ANGULAR_2");
	intraDirY.valMap.insert(3, "INTRA_ANGULAR_3");
	intraDirY.valMap.insert(4, "INTRA_ANGULAR_4");
	intraDirY.valMap.insert(5, "INTRA_ANGULAR_5");
	intraDirY.valMap.insert(6, "INTRA_ANGULAR_6");
	intraDirY.valMap.insert(7, "INTRA_ANGULAR_7");
	intraDirY.valMap.insert(8, "INTRA_ANGULAR_8");
	intraDirY.valMap.insert(9, "INTRA_ANGULAR_9");
	intraDirY.valMap.insert(10, "INTRA_ANGULAR_10");
	intraDirY.valMap.insert(11, "INTRA_ANGULAR_11");
	intraDirY.valMap.insert(12, "INTRA_ANGULAR_12");
	intraDirY.valMap.insert(13, "INTRA_ANGULAR_13");
	intraDirY.valMap.insert(14, "INTRA_ANGULAR_14");
	intraDirY.valMap.insert(15, "INTRA_ANGULAR_15");
	intraDirY.valMap.insert(16, "INTRA_ANGULAR_16");
	intraDirY.valMap.insert(17, "INTRA_ANGULAR_17");
	intraDirY.valMap.insert(18, "INTRA_ANGULAR_18");
	intraDirY.valMap.insert(19, "INTRA_ANGULAR_19");
	intraDirY.valMap.insert(20, "INTRA_ANGULAR_20");
	intraDirY.valMap.insert(21, "INTRA_ANGULAR_21");
	intraDirY.valMap.insert(22, "INTRA_ANGULAR_22");
	intraDirY.valMap.insert(23, "INTRA_ANGULAR_23");
	intraDirY.valMap.insert(24, "INTRA_ANGULAR_24");
	intraDirY.valMap.insert(25, "INTRA_ANGULAR_25");
	intraDirY.valMap.insert(26, "INTRA_ANGULAR_26");
	intraDirY.valMap.insert(27, "INTRA_ANGULAR_27");
	intraDirY.valMap.insert(28, "INTRA_ANGULAR_28");
	intraDirY.valMap.insert(29, "INTRA_ANGULAR_29");
	intraDirY.valMap.insert(30, "INTRA_ANGULAR_30");
	intraDirY.valMap.insert(31, "INTRA_ANGULAR_31");
	intraDirY.valMap.insert(32, "INTRA_ANGULAR_32");
	intraDirY.valMap.insert(33, "INTRA_ANGULAR_33");
	intraDirY.valMap.insert(34, "INTRA_ANGULAR_34");
	p_statsTypeList.append(intraDirY);

	StatisticsType intraDirC(10, "Intra Dir Chroma", "jet", 0, 34);
	intraDirC.valMap.insert(0, "INTRA_PLANAR");
	intraDirC.valMap.insert(1, "INTRA_DC");
	intraDirC.valMap.insert(2, "INTRA_ANGULAR_2");
	intraDirC.valMap.insert(3, "INTRA_ANGULAR_3");
	intraDirC.valMap.insert(4, "INTRA_ANGULAR_4");
	intraDirC.valMap.insert(5, "INTRA_ANGULAR_5");
	intraDirC.valMap.insert(6, "INTRA_ANGULAR_6");
	intraDirC.valMap.insert(7, "INTRA_ANGULAR_7");
	intraDirC.valMap.insert(8, "INTRA_ANGULAR_8");
	intraDirC.valMap.insert(9, "INTRA_ANGULAR_9");
	intraDirC.valMap.insert(10, "INTRA_ANGULAR_10");
	intraDirC.valMap.insert(11, "INTRA_ANGULAR_11");
	intraDirC.valMap.insert(12, "INTRA_ANGULAR_12");
	intraDirC.valMap.insert(13, "INTRA_ANGULAR_13");
	intraDirC.valMap.insert(14, "INTRA_ANGULAR_14");
	intraDirC.valMap.insert(15, "INTRA_ANGULAR_15");
	intraDirC.valMap.insert(16, "INTRA_ANGULAR_16");
	intraDirC.valMap.insert(17, "INTRA_ANGULAR_17");
	intraDirC.valMap.insert(18, "INTRA_ANGULAR_18");
	intraDirC.valMap.insert(19, "INTRA_ANGULAR_19");
	intraDirC.valMap.insert(20, "INTRA_ANGULAR_20");
	intraDirC.valMap.insert(21, "INTRA_ANGULAR_21");
	intraDirC.valMap.insert(22, "INTRA_ANGULAR_22");
	intraDirC.valMap.insert(23, "INTRA_ANGULAR_23");
	intraDirC.valMap.insert(24, "INTRA_ANGULAR_24");
	intraDirC.valMap.insert(25, "INTRA_ANGULAR_25");
	intraDirC.valMap.insert(26, "INTRA_ANGULAR_26");
	intraDirC.valMap.insert(27, "INTRA_ANGULAR_27");
	intraDirC.valMap.insert(28, "INTRA_ANGULAR_28");
	intraDirC.valMap.insert(29, "INTRA_ANGULAR_29");
	intraDirC.valMap.insert(30, "INTRA_ANGULAR_30");
	intraDirC.valMap.insert(31, "INTRA_ANGULAR_31");
	intraDirC.valMap.insert(32, "INTRA_ANGULAR_32");
	intraDirC.valMap.insert(33, "INTRA_ANGULAR_33");
	intraDirC.valMap.insert(34, "INTRA_ANGULAR_34");
	p_statsTypeList.append(intraDirC);

	StatisticsType transformDepth(11, "Transform Depth", colorRangeType, 0, QColor(0, 0, 0), 3, QColor(0,255,0));
	p_statsTypeList.append(transformDepth);
}

void de265File::loadStatisticToCache(int frameIdx, int)
{
	if (!p_internalsSupported)
		return;

	p_RetrieveStatistics = true;

	// We will have to decode the current frame again to get the internals/statistics
	// This can be done like this:
	int curIdx = p_Buf_CurrentOutputBufferFrameIndex;
	if (frameIdx == p_Buf_CurrentOutputBufferFrameIndex)
		p_Buf_CurrentOutputBufferFrameIndex ++;
	getOneFrame(NULL, frameIdx);

	// The statistics should now be in the cache
}

void de265File::cacheStatistics(const de265_image *img, int iPOC)
{
	if (!p_internalsSupported)
		return;

	if (p_statsCache.contains(iPOC))
		// Statistics for this poc were already cached
		return;

	StatisticsItem anItem;
	anItem.rawValues[1] = 0;

	/// --- CTB internals/statistics
	int widthInCTB, heightInCTB, log2CTBSize;
	de265_internals_get_CTB_Info_Layout(img, &widthInCTB, &heightInCTB, &log2CTBSize);
	int ctb_size = 1 << log2CTBSize;	// width and height of each ctb

	// Save Slice index
	StatisticsType *statsTypeSliceIdx = getStatisticsType(0);
	anItem.type = blockType;
	uint16_t *tmpArr = new uint16_t[ widthInCTB * heightInCTB ];
	de265_internals_get_CTB_sliceIdx(img, tmpArr);
	for (int y = 0; y < heightInCTB; y++) {
		for (int x = 0; x < widthInCTB; x++) {
			uint16_t val = tmpArr[ y * widthInCTB + x ];
			anItem.color = statsTypeSliceIdx->colorRange->getColor((float)val);
			anItem.rawValues[0] = (int)val;
			anItem.positionRect = QRect(x*ctb_size, y*ctb_size, ctb_size, ctb_size);

			p_statsCache[iPOC][0].append(anItem);
		}
	}
	
	delete tmpArr;
	tmpArr = NULL;

	/// --- CB internals/statistics (part Size, prediction mode, pcm flag, CU trans quant bypass flag)
	
	// Get CB info array layout from image
	int widthInCB, heightInCB, log2CBInfoUnitSize;
	de265_internals_get_CB_Info_Layout(img, &widthInCB, &heightInCB, &log2CBInfoUnitSize);
	int cb_infoUnit_size = 1 << log2CBInfoUnitSize;
	// Get CB info from image
	uint16_t *cbInfoArr = new uint16_t[widthInCB * heightInCB];
	de265_internals_get_CB_info(img, cbInfoArr);

	// Get PB array layout from image
	int widthInPB, heightInPB, log2PBInfoUnitSize;
	de265_internals_get_PB_Info_layout(img, &widthInPB, &heightInPB, &log2PBInfoUnitSize);
	int pb_infoUnit_size = 1 << log2PBInfoUnitSize;
	
	// Get PB info from image
	int16_t *refPOC0 = new int16_t[widthInPB*heightInPB];
	int16_t *refPOC1 = new int16_t[widthInPB*heightInPB];
	int16_t *vec0_x = new int16_t[widthInPB*heightInPB];
	int16_t *vec0_y = new int16_t[widthInPB*heightInPB];
	int16_t *vec1_x = new int16_t[widthInPB*heightInPB];
	int16_t *vec1_y = new int16_t[widthInPB*heightInPB];
	de265_internals_get_PB_info(img, refPOC0, refPOC1, vec0_x, vec0_y, vec1_x, vec1_y);

	// Get intra pred mode (intra dir) layout from image
	int widthInIntraDirUnits, heightInIntraDirUnits, log2IntraDirUnitsSize;
	de265_internals_get_IntraDir_Info_layout(img, &widthInIntraDirUnits, &heightInIntraDirUnits, &log2IntraDirUnitsSize);
	int intraDir_infoUnit_size = 1 << log2IntraDirUnitsSize;

	// Get intra pred mode (intra dir) from image
	uint8_t *intraDirY = new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits];
	uint8_t *intraDirC = new uint8_t[widthInIntraDirUnits*heightInIntraDirUnits];
	de265_internals_get_intraDir_info(img, intraDirY, intraDirC);

	// Get tu info array layou
	int widthInTUInfoUnits, heightInTUInfoUnits, log2TUInfoUnitSize;
	de265_internals_get_TUInfo_Info_layout(img, &widthInTUInfoUnits, &heightInTUInfoUnits, &log2TUInfoUnitSize);
	int tuInfo_unit_size = 1 << log2TUInfoUnitSize;

	// Get TU info
	uint8_t *tuInfo = new uint8_t[widthInTUInfoUnits*heightInTUInfoUnits];
	de265_internals_get_TUInfo_info(img, tuInfo);

	for (int y = 0; y < heightInCB; y++) {
		for (int x = 0; x < widthInCB; x++) {
			uint16_t val = cbInfoArr[ y * widthInCB + x ];

			uint8_t log2_cbSize = (val & 7);	 // Extract lowest 3 bits;
			
			if (log2_cbSize > 0) {
				// We are in the top left position of a CB.

				// Get values of this CB
				uint8_t cbSizePix = 1 << log2_cbSize;  // Size (w,h) in pixels
				int cbPosX = x * cb_infoUnit_size;	   // Position of this CB in pixels
				int cbPosY = y * cb_infoUnit_size;
				uint8_t partMode = ((val >> 3) & 7);   // Extract next 3 bits (part size);
				uint8_t predMode = ((val >> 6) & 3);   // Extract next 2 bits (prediction mode);
				bool    pcmFlag  = (val & 256);		   // Next bit (PCM flag)
				bool    tqBypass = (val & 512);        // Next bit (Transquant bypass flag)

				// Set CB position
				anItem.positionRect = QRect(cbPosX, cbPosY, cbSizePix, cbSizePix);

				// Set part mode (ID 1)
				anItem.rawValues[0] = partMode;
				anItem.color = getStatisticsType(1)->colorRange->getColor(partMode);
				p_statsCache[iPOC][1].append(anItem);
				
				// Set pred mode (ID 2)
				anItem.rawValues[0] = predMode;
				anItem.color = getStatisticsType(2)->colorRange->getColor(predMode);
				p_statsCache[iPOC][2].append(anItem);

				// Set PCM flag (ID 3)
				anItem.rawValues[0] = pcmFlag;
				anItem.color = getStatisticsType(3)->colorRange->getColor(pcmFlag);
				p_statsCache[iPOC][3].append(anItem);

				// Set transquant bypass flag (ID 4)
				anItem.rawValues[0] = tqBypass;
				anItem.color = getStatisticsType(4)->colorRange->getColor(tqBypass);
				p_statsCache[iPOC][4].append(anItem);

				if (predMode != 0) {
					// For each of the prediction blocks set some info

					int numPB = (partMode == 0) ? 1 : (partMode == 3) ? 4 : 2;
					for (int i=0; i<numPB; i++) {
						
						// Get pb position/size
						int pbSubX, pbSubY, pbW, pbH;
						getPBSubPosition(partMode, cbSizePix, i, &pbSubX, &pbSubY, &pbW, &pbH);
						int pbX = cbPosX + pbSubX;
						int pbY = cbPosY + pbSubY;

						// Get index for this xy position in pb_info array
						int pbIdx = (pbY / pb_infoUnit_size) * widthInPB + (pbX / pb_infoUnit_size);

						StatisticsItem pbItem;
						pbItem.type = blockType;
						pbItem.positionRect = QRect(pbX, pbY, pbW, pbH);

						// Add ref POC 0 (ID 5)
						int8_t ref0 = refPOC0[pbIdx];
						if (ref0 != -1) {
							pbItem.rawValues[0] = ref0;
							pbItem.color = getStatisticsType(5)->colorRange->getColor(ref0-iPOC);
							p_statsCache[iPOC][5].append(pbItem);
						}

						// Add ref index 1 (ID 6)
						int8_t ref1 = refPOC1[pbIdx];
						if (ref1 != -1) {
							pbItem.rawValues[0] = ref1;
							pbItem.color = getStatisticsType(6)->colorRange->getColor(ref1-iPOC);
							p_statsCache[iPOC][6].append(pbItem);
						}

						// Add motion vector 0 (ID 7)
						pbItem.type = arrowType;
						if (ref0 != -1) {
							pbItem.vector[0] = (float)(vec0_x[pbIdx]) / 4;
							pbItem.vector[1] = (float)(vec0_y[pbIdx]) / 4;
							pbItem.color = getStatisticsType(7)->colorRange->getColor(ref0-iPOC);	// Color vector according to referecen POC
							p_statsCache[iPOC][7].append(pbItem);
						}

						// Add motion vector 1 (ID 8)
						if (ref0 != -1) {
							pbItem.vector[0] = (float)(vec1_x[pbIdx]) / 4;
							pbItem.vector[1] = (float)(vec1_y[pbIdx]) / 4;
							pbItem.color = getStatisticsType(8)->colorRange->getColor(ref1-iPOC);	// Color vector according to referecen POC
							p_statsCache[iPOC][8].append(pbItem);
						}

					}
				}
				else if (predMode == 0) {
					// Get index for this xy position in the intraDir array
					int intraDirIdx = (cbPosY / intraDir_infoUnit_size) * widthInIntraDirUnits + (cbPosX / intraDir_infoUnit_size);
					
					// For setting the vector
					StatisticsItem intraDirVec;
					intraDirVec.positionRect = anItem.positionRect;
					intraDirVec.type = arrowType;

					// Set Intra prediction direction Luma (ID 9)
					int intraDirLuma = intraDirY[intraDirIdx];
					if (intraDirLuma <= 34) {
						anItem.rawValues[0] = intraDirLuma;
						anItem.color = getStatisticsType(9)->colorRange->getColor(intraDirLuma);
						p_statsCache[iPOC][9].append(anItem);

						// Set Intra prediction direction Luma (ID 9) as vecotr
						intraDirVec.vector[0] = (float)p_vectorTable[intraDirLuma][0] * VECTOR_SCALING;
						intraDirVec.vector[1] = (float)p_vectorTable[intraDirLuma][1] * VECTOR_SCALING;
						intraDirVec.color = QColor(0, 0, 0);
						p_statsCache[iPOC][9].append(intraDirVec);
					}

					// Set Intra prediction direction Chroma (ID 10)
					int intraDirChroma = intraDirC[intraDirIdx];
					if (intraDirChroma <= 34) {
						anItem.rawValues[0] = intraDirChroma;
						anItem.color = getStatisticsType(10)->colorRange->getColor(intraDirChroma);
						p_statsCache[iPOC][10].append(anItem);

						// Set Intra prediction direction Chroma (ID 10) as vector
						intraDirVec.vector[0] = (float)p_vectorTable[intraDirChroma][0] * VECTOR_SCALING;
						intraDirVec.vector[1] = (float)p_vectorTable[intraDirChroma][1] * VECTOR_SCALING;
						intraDirVec.color = QColor(0, 0, 0);
						p_statsCache[iPOC][10].append(intraDirVec);
					}
				}
				
				// Walk into the TU tree
				int tuIdx = (cbPosY / tuInfo_unit_size) * widthInTUInfoUnits + (cbPosX / tuInfo_unit_size);
				cacheStatistics_TUTree_recursive(tuInfo, widthInTUInfoUnits, tuInfo_unit_size, iPOC, tuIdx, cbSizePix / tuInfo_unit_size, 0);
			}
		}
	}

	// Delete all temporary array
	delete cbInfoArr;
	cbInfoArr = NULL;

	delete refPOC0; refPOC0 = NULL;
	delete refPOC1;	refPOC1 = NULL;
	delete vec0_x;	vec0_x  = NULL;
	delete vec0_y;	vec0_y  = NULL;
	delete vec1_x;	vec1_x  = NULL;
	delete vec1_y;	vec1_y  = NULL;
}

void de265File::getPBSubPosition(int partMode, int cbSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH)
{
	// Get the position/width/height of the PB
	if (partMode == 0) // PART_2Nx2N
	{	
		*pbW = cbSizePix;
		*pbH = cbSizePix;
		*pbX = 0;
		*pbY = 0;
	}
	else if (partMode == 1) // PART_2NxN
	{
		*pbW = cbSizePix;
		*pbH = cbSizePix / 2;
		*pbX = 0;
		*pbY = (pbIdx == 0) ? 0 : cbSizePix / 2;
	}
	else if (partMode == 2) // PART_Nx2N
	{
		*pbW = cbSizePix / 2;
		*pbH = cbSizePix;
		*pbX = (pbIdx == 0) ? 0 : cbSizePix / 2;
		*pbY = 0;
	}
	else if (partMode == 3) // PART_NxN
	{
		*pbW = cbSizePix / 2;
		*pbH = cbSizePix / 2;
		*pbX = (pbIdx == 0 || pbIdx == 2) ? 0 : cbSizePix / 2;
		*pbY = (pbIdx == 0 || pbIdx == 1) ? 0 : cbSizePix / 2;
	}
	else if (partMode == 4) // PART_2NxnU
	{
		*pbW = cbSizePix;
		*pbH = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
		*pbX = 0;
		*pbY = (pbIdx == 0) ? 0 : cbSizePix / 4;
	}
	else if (partMode == 5) // PART_2NxnD
	{
		*pbW = cbSizePix;
		*pbH = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
		*pbX = 0;
		*pbY = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
	}
	else if (partMode == 6) // PART_nLx2N
	{
		*pbW = (pbIdx == 0) ? cbSizePix / 4 : cbSizePix / 4 * 3;
		*pbH = cbSizePix;
		*pbX = (pbIdx == 0) ? 0 : cbSizePix / 4;
		*pbY = 0;
	}
	else if (partMode == 7) // PART_nRx2N
	{
		*pbW = (pbIdx == 0) ? cbSizePix / 4 * 3 : cbSizePix / 4;
		*pbH = cbSizePix;
		*pbX = (pbIdx == 0) ? 0 : cbSizePix / 4 * 3;
		*pbY = 0;
	}
}

/* Walk into the TU tree and set the tree depth as a statistic value if the TU is not further split
 * \param tuInfo: The tuInfo array
 * \param tuInfoWidth: The number of TU units per line in the tuInfo array
 * \param tuUnitSizePix: The size of ont TU unit in pixels
 * \param iPOC: The current POC
 * \param tuIdx: The top left index of the currently handled TU in tuInfo
 * \param tuWidth_units: The WIdth of the TU in units
 * \param trDepth: The current transform tree depth
*/
void de265File::cacheStatistics_TUTree_recursive(uint8_t *tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int tuWidth_units, int trDepth)
{
	// Check if the TU is further split.
	if (tuInfo[tuIdx] & (1 << trDepth)) {
		// The transform is split further
		int yOffset = (tuWidth_units / 2) * tuInfoWidth;
		cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx                              , tuWidth_units / 2, trDepth+1);
		cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx           + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
		cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset                    , tuWidth_units / 2, trDepth+1);
		cacheStatistics_TUTree_recursive(tuInfo, tuInfoWidth, tuUnitSizePix, iPOC, tuIdx + yOffset + tuWidth_units / 2, tuWidth_units / 2, trDepth+1);
	}
	else {
		// The transform is not split any further. Add the TU depth to the statistics (ID 11)
		StatisticsItem tuDepth;
		int tuWidth = tuWidth_units * tuUnitSizePix;
		int posX_units = tuIdx % tuInfoWidth;
		int posY_units = tuIdx / tuInfoWidth;
		tuDepth.positionRect = QRect(posX_units * tuUnitSizePix, posY_units * tuUnitSizePix, tuWidth, tuWidth);
		tuDepth.type = blockType;
		tuDepth.rawValues[0] = trDepth;
		tuDepth.color = getStatisticsType(11)->colorRange->getColor(trDepth);
		p_statsCache[iPOC][11].append(tuDepth);
	}
}