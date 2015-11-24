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
#include "statisticSource.h"
#include "de265File_BitstreamHandler.h"
#include "de265.h"
#include <QFile>
#include <QFuture>
#include <QQueue>
#include <QLibrary>

#define DE265_BUFFER_SIZE 8		//< The number of pictures allowed in the decoding buffer

class de265File :
  public YUVSource,
  public statisticSource
{
  // typedefs for libde265 decoder library function pointers
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

  // libde265 decoder library function pointers for internals
  typedef void (*f_de265_internals_get_CTB_Info_Layout)		(const de265_image*, int*, int*, int*);
  typedef void (*f_de265_internals_get_CTB_sliceIdx)			(const de265_image*, uint16_t*);
  typedef void (*f_de265_internals_get_CB_Info_Layout)		(const de265_image*, int*, int*, int*);
  typedef void (*f_de265_internals_get_CB_info)				(const de265_image*, uint16_t*);
  typedef void (*f_de265_internals_get_PB_Info_layout)		(const de265_image*, int*, int*, int*);
  typedef void (*f_de265_internals_get_PB_info)				(const de265_image*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*, int16_t*);
  typedef void (*f_de265_internals_get_IntraDir_Info_layout)  (const de265_image*, int*, int*, int*);
  typedef void (*f_de265_internals_get_intraDir_info)			(const de265_image*, uint8_t*, uint8_t*);
  typedef void (*f_de265_internals_get_TUInfo_Info_layout)	(const de265_image*, int*, int*, int*);
  typedef void (*f_de265_internals_get_TUInfo_info)			(const de265_image*, uint8_t*);
  
public:
  de265File(const QString &fname);
  ~de265File();

  void getOneFrame(QByteArray &targetByteArray, unsigned int frameIdx);

  QString getName();

  virtual QString getType() { return QString("de265File"); }

  virtual QString getPath() { return p_path; }
  virtual QString getCreatedtime() { return p_createdtime; }
  virtual QString getModifiedtime() { return p_modifiedtime; }
  virtual qint64  getNumberBytes() { return p_fileSize; }
  virtual qint64  getNumberFrames() { return p_numFrames; }
  virtual QString getStatus();

  // from statisticSource
  virtual QSize getSize() { return QSize(p_width, p_height); }
  virtual int getFrameRate() { return p_frameRate; }

  // Setting functions for size/nrFrames/frameRate/pixelFormat.
  // All of these are predefined by the stream an cannot be set by the user.
  virtual void setSize(int, int) {}
  virtual void setFrameRate(double) {}
  virtual void setPixelFormat(YUVCPixelFormatType) {}

  // Can we get internals/statistic using the loaded library?
  bool getStatisticsEnabled() { return p_internalsSupported; }

protected:
  de265_decoder_context* p_decoder;

  // Byte pointers into the file. Each value is the position of a NAL unit header.
  QList<quint64> p_startPosList;		

  // Decoder library
  void loadDecoderLibrary();
  void allocateNewDecoder();
  QLibrary p_decLib;
  
  // Decoder library function pointers
  f_de265_new_decoder			     de265_new_decoder;
  f_de265_set_parameter_bool   de265_set_parameter_bool;
  f_de265_set_parameter_int	   de265_set_parameter_int;
  f_de265_disable_logging		   de265_disable_logging;
  f_de265_set_verbosity		     de265_set_verbosity;
  f_de265_start_worker_threads de265_start_worker_threads;
  f_de265_set_limit_TID		     de265_set_limit_TID;
  f_de265_get_error_text		   de265_get_error_text;
  f_de265_get_chroma_format    de265_get_chroma_format;
  f_de265_get_image_width      de265_get_image_width;
  f_de265_get_image_height	   de265_get_image_height;
  f_de265_get_image_plane   	 de265_get_image_plane;
  f_de265_get_bits_per_pixel	 de265_get_bits_per_pixel;
  f_de265_decode 				       de265_decode;
  f_de265_push_data			       de265_push_data;
  f_de265_flush_data			     de265_flush_data;
  f_de265_get_next_picture	   de265_get_next_picture;
  f_de265_free_decoder    	   de265_free_decoder;

  // Decoder library function pointers for internals
  f_de265_internals_get_CTB_Info_Layout		    de265_internals_get_CTB_Info_Layout;
  f_de265_internals_get_CTB_sliceIdx			    de265_internals_get_CTB_sliceIdx;
  f_de265_internals_get_CB_Info_Layout		    de265_internals_get_CB_Info_Layout;
  f_de265_internals_get_CB_info				        de265_internals_get_CB_info;
  f_de265_internals_get_PB_Info_layout		    de265_internals_get_PB_Info_layout;
  f_de265_internals_get_PB_info				        de265_internals_get_PB_info;
  f_de265_internals_get_IntraDir_Info_layout  de265_internals_get_IntraDir_Info_layout;
  f_de265_internals_get_intraDir_info         de265_internals_get_intraDir_info;
  f_de265_internals_get_TUInfo_Info_layout	  de265_internals_get_TUInfo_Info_layout;
  f_de265_internals_get_TUInfo_info           de265_internals_get_TUInfo_info;

  // If everything is allright it will be DE265_OK
  de265_error p_decError;
  
  // The source file
  de265File_FileHandler p_srcFile;

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
  void copyImgTo444Buffer(const de265_image *src, QByteArray &dst);
  // Copy the raw data from the src to the byte array
  void copyImgToByteArray(const de265_image *src, QByteArray &dst);

  /// ===== Buffering
  QByteArray  p_Buf_CurrentOutputBuffer;			      ///< The buffer that was requested in the last call to getOneFrame
  int         p_Buf_CurrentOutputBufferFrameIndex;	///< The frame index of the buffer in p_Buf_CurrentOutputBuffer
  
  // Decode one picture into the buffer. Return true on success.
  bool decodeOnePicture(QByteArray &buffer);

  // Status reporting
  QString p_StatusText;
  bool p_internalError;		///< There was an internal error and the decoder can not be used.

  //// ----------- Statistics extraction

  void fillStatisticList();	    ///< fill the list of statistic types that we can provide
  bool p_internalsSupported;		///< does the loaded library support the extraction of internals/statistics?
  bool p_RetrieveStatistics;		///< if set to true the decoder will also get statistics from each decoded frame and put them into the cache

  // The statistic with the given frameIdx/typeIdx could not be found in the cache. Load it.
  virtual void loadStatisticToCache(int frameIdx, int typeIdx);

  // Get the statistics from the frame and put them into the cache
  void cacheStatistics(const de265_image *img, int iPOC);
  
  // With the given partitioning mode, the size of the CU and the prediction block index, calculate the 
  // sub-position and size of the prediction block
  void getPBSubPosition(int partMode, int CUSizePix, int pbIdx, int *pbX, int *pbY, int *pbW, int *pbH);

  // 
  void cacheStatistics_TUTree_recursive(uint8_t *tuInfo, int tuInfoWidth, int tuUnitSizePix, int iPOC, int tuIdx, int log2TUSize, int trDepth);

  // Convert intra direction mode into vector
  static const int p_vectorTable[35][2];

};

#endif