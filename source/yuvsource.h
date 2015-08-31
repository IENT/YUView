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

#ifndef YUVSOURCE_H
#define YUVSOURCE_H

#include <QObject>
#include "typedef.h"

class PixelFormat
{
public:
	PixelFormat()
	{
		p_name = "";
		p_bitsPerSample = 8;
		p_bitsPerPixelNominator = 32;
		p_bitsPerPixelDenominator = 1;
		p_subsamplingHorizontal = 1;
		p_subsamplingVertical = 1;
		p_bytePerComponentSample = 1;
		p_planar = true;
	}

	void setParams(QString name, int bitsPerSample, int bitsPerPixelNominator, int bitsPerPixelDenominator, int subsamplingHorizontal, int subsamplingVertical, bool isPlanar, int bytePerComponent = 1)
	{
		p_name = name;
		p_bitsPerSample = bitsPerSample;
		p_bitsPerPixelNominator = bitsPerPixelNominator;
		p_bitsPerPixelDenominator = bitsPerPixelDenominator;
		p_subsamplingHorizontal = subsamplingHorizontal;
		p_subsamplingVertical = subsamplingVertical;
		p_planar = isPlanar;
		p_bytePerComponentSample = bytePerComponent;
	}

	~PixelFormat() {}

	QString name() { return p_name; }
	int bitsPerSample() { return p_bitsPerSample; }
	int bitsPerPixelNominator() { return p_bitsPerPixelNominator; }
	int bitsPerPixelDenominator() { return p_bitsPerPixelDenominator; }
	int subsamplingHorizontal() { return p_subsamplingHorizontal; }
	int subsamplingVertical() { return p_subsamplingVertical; }
	int isPlanar() { return p_planar; }
	int bytePerComponent() { return p_bytePerComponentSample; }

private:
	QString p_name;
	int p_bitsPerSample;
	int p_bitsPerPixelNominator;
	int p_bitsPerPixelDenominator;
	int p_subsamplingHorizontal;
	int p_subsamplingVertical;
	int p_bytePerComponentSample;
	bool p_planar;
};

typedef std::map<YUVCPixelFormatType, PixelFormat> PixelFormatMapType;

/** Virtual class.
  * The YUVSource can be anything that provides raw YUV data. This can be a file or any kind of decoder or maybe a network source ...
  * All YUV sources support handling of YUV data and can return a specific frame int YUV 444 by calling getOneFrame.
  */
class YUVSource : public QObject
{
	Q_OBJECT
public:
	explicit YUVSource(QObject *parent = 0);

	~YUVSource();

	/// Get the format of the YUV if known.
	virtual void getFormat(int* width, int* height, int* numFrames, double* frameRate);
	virtual void setFormat(int  width, int  height, int  numFrames, double  frameRate);
	virtual bool isFormatValid() { return (p_width != -1 && p_height != -1 && p_numFrames != -1); }
	virtual void getSize(int *width, int *height) { *width = p_width; *height = p_height; };

	// Get the number of frames (as far as known)
	virtual qint64 getNumberFrames() = 0;

	// reads one frame in YUV444 into target byte array
	virtual void getOneFrame(QByteArray* targetByteArray, unsigned int frameIdx) = 0;

	// Get the name of this YUV source. For a file this is usually the file name. For a network source it might be something else.
	virtual QString getName() = 0;

	// Get path/dateCreated/modified/nrBytes if applicable
	virtual QString getPath() = 0;
	virtual QString getCreatedtime() = 0;
	virtual QString getModifiedtime() = 0;
	virtual qint64  getNumberBytes() = 0;
	
	// Get a status text. Is the source OK? Was there an error?
	virtual QString getStatus() { return QString("OK"); }

	// Set conversion function from YUV to RGB
	void setInterpolationMode(InterpolationMode newMode) { p_interpolationMode = newMode; }
	// Set other values
	virtual void setSize(int width, int height);
	virtual void setNumFrames(int numFrames);
	virtual void setFrameRate(double frameRate);
	virtual void setPixelFormat(YUVCPixelFormatType pixelFormat);

	// Get pixel format (YUV format) and interpolation mode
	YUVCPixelFormatType pixelFormat() { return p_srcPixelFormat; }
	InterpolationMode interpolationMode() { return p_interpolationMode; }

	static PixelFormatMapType pixelFormatList();

	// Some YUV info functions
	static int verticalSubSampling(YUVCPixelFormatType pixelFormat);
	static int horizontalSubSampling(YUVCPixelFormatType pixelFormat);
	static int bitsPerSample(YUVCPixelFormatType pixelFormat);
	static qint64 bytesPerFrame(int width, int height, YUVCPixelFormatType pixelFormat);
	static bool isPlanar(YUVCPixelFormatType pixelFormat);
	static int  bytePerComponent(YUVCPixelFormatType pixelFormat);
	
protected:
	// YUV to RGB conversion
	YUVCPixelFormatType p_srcPixelFormat;
	InterpolationMode p_interpolationMode;

	static PixelFormatMapType g_pixelFormatList;

	// Convert one frame from p_srcPixelFormat to YUV444 
	void convert2YUV444(QByteArray *sourceBuffer, int lumaWidth, int lumaHeight, QByteArray *targetBuffer);

	int p_width;
	int p_height;
	int p_numFrames;
	double p_frameRate;

	// Temporaray buffer for conversion to YUV444
	QByteArray p_tmpBufferYUV;
};

#endif