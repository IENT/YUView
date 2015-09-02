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

de265File::de265File(const QString &fname, QObject *parent)
	: YUVSource(parent)
{
	// Init variables
	p_numFrames = -1;
	p_lastFrameDecoded = -1;
	p_cancelBackgroundDecoder = false;
	p_decoder = NULL;

	allocateNewDecoder();

	// Open the input file
	p_srcFile = new QFile(fname);
	p_srcFile->open(QIODevice::ReadOnly);

	// get some more information from file
	QFileInfo fileInfo(p_srcFile->fileName());
	p_path = fileInfo.path();
	p_createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
	p_modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
	p_fileSize = fileInfo.size();

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
	// Convert the error (p_decError) to text and return it.
	QString errText = QString(de265_get_error_text(p_decError));
	return errText;
}

void de265File::getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx)
{
	//printf("Request %d(%d)", frameIdx, p_numFrames);

	// Get pictures from the buffer until we find the right one
	QByteArray *pic = NULL;
	int poc = -1;
	while (poc != frameIdx) {
		// Rigth image not found yet. Get the next image from the decoder
		while (p_Buf_DecodedPictures.count() == 0) {
			// The buffer is still empty. Give the decoder some time (50ms)
			//QTime dieTime = QTime::currentTime().addMSecs(50);
			//while (QTime::currentTime() < dieTime)
			//	// Proccess all events for a maximum time of 100ms
			//	QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

			if (!p_backgroundDecodingFuture.isRunning()) {
				// The background process is not running. Start is back up or we will wait forever.
				p_backgroundDecodingFuture = QtConcurrent::run(this, &de265File::backgroundDecoder);
				//qDebug() << "Restart background process while waiting.";
			}
			//qDebug() << "Wait 50ms.";
			QThread::msleep(50);
			//printf("Wait...");
		}
		pic = popImageFromOutputBuffer(&poc);
		
		if (poc != frameIdx) {
			// Wrong poc. Discard the buffer.
			//printf("Discard %d...", poc);
			addEmptyBuffer(pic);
			//qDebug() << "This is not the frame we are looking for.";
		}
	}

	//if (frameIdx < poc) {
	//	// The requested frame is before all the frames in the buffer. 
	//	// We will have to restart decoding from the start.

	//	// TODO
	//	int i = 1233;
	//}
	// Actually we don't have to restart decoding. The decoder will restart 
	// decoding at the beginning when the end of the stream is reached.
	// This will just take a little longer.
	
	//qDebug() << "Recieved Frame " << poc;
	assert(frameIdx == poc && pic != NULL);
	*targetByteArray = *pic;

	// The buffer is now empty and can hold a new frame.
	addEmptyBuffer(pic);
	
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
	while (true) {
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
	while (emptyBuffer != NULL) {
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