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

#if !YUVIEW_DISABLE_LIBDE265

#include "de265File.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QtConcurrent>
#include <assert.h>
#include <QThread>

#define BUFFER_SIZE 40960
#define SET_INTERNALERROR_RETURN(errTxt) { p_internalError = true; p_StatusText = errTxt; return; }

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
					// The next one however may be.
					p_numFrames = p_lastFrameDecoded + 2;
					// The number of frames changed in the background
					if (emitSinals) emit signal_sourceNrFramesChanged();
				}
				// Update size and format
				p_width = de265_get_image_width(img, 0);
				p_height = de265_get_image_height(img, 0);

				// Put array into buffer
				copyImgTo444Buffer(img, buffer);

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

	QString libDir = QDir::currentPath() + "/libde265";
	p_decLib.setFileName(libDir);
	if (!p_decLib.load()) {
		// Loading failed. Try subdirectory libde265
		libDir = QDir::currentPath() + "/libde265/libde265";
	}

	if (!p_decLib.load()) {
		// Loading failed. Try system directories.
		libDir = "libde265";
	}

	if (!p_decLib.load()) 
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

#endif // #if !YUVIEW_DISABLE_LIBDE265