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

#define BUFFER_SIZE 40960

de265File::de265File(const QString &fname, QObject *parent)
	: YUVSource(parent)
{
	// Init variables
	p_nrKnownFrames = 0;
	p_lastDecodedPOC = -1;

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

	// Open the input file
	p_srcFile = new QFile(fname);
	p_srcFile->open(QIODevice::ReadOnly);

	// get some more information from file
	QFileInfo fileInfo(p_srcFile->fileName());
	p_path = fileInfo.path();
	p_createdtime = fileInfo.created().toString("yyyy-MM-dd hh:mm:ss");
	p_modifiedtime = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
	p_fileSize = fileInfo.size();

	// Try to decoder the first frame
	QByteArray tmpArray;
	getOneFrame(&tmpArray, 0);
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
	// Check if we decoded this already
	if (frameIdx == p_lastDecodedPOC) {
		(*targetByteArray) = p_lastDecodedFrame;
		return;
	}

	// Decode until we have decoded the right frame
	bool stop = false;
	de265_error err;
	while (!stop) {

		int more = 1;
		while (more) {
			more = 0;
		
			err = de265_decode(p_decoder, &more);
			while (err == DE265_ERROR_WAITING_FOR_INPUT_DATA) {
				// The decoder needs more data. Get it from the file.
				char buf[BUFFER_SIZE];
				int64_t pos = p_srcFile->pos();
				int n = p_srcFile->read(buf, BUFFER_SIZE);

				// Push the data to the decoder
				if (n > 0) {
					err = de265_push_data(p_decoder, buf, n, pos, (void*)2);
					if (err != DE265_OK && err != DE265_ERROR_WAITING_FOR_INPUT_DATA) {
						// An error occured
						return;
					}
				}
			}
			
			if (err != DE265_OK) {
				// Something went wrong
				more = 0;
				break;
			}

			const de265_image* img = de265_get_next_picture(p_decoder);
			if (img) {
				// We have recieved an output image
				p_lastDecodedPOC++;
				if (p_lastDecodedPOC + 1 > getNumberFrames())
					p_nrKnownFrames = p_lastDecodedPOC + 1;
				// Update size and format
				p_width = de265_get_image_width(img, 0);
				p_height = de265_get_image_height(img, 0);

				// Update the chroma format and copy (convert if nevessary) the image into the buffer
				copyImgTo444Buffer(img, &p_lastDecodedFrame);
				
				if (frameIdx == p_lastDecodedPOC) {
					// We found the image. Return it.
					(*targetByteArray) = p_lastDecodedFrame;
					return;
				}
			}
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
