/*  This file is part of YUView - The YUV player with advanced analytics toolset
*   <https://github.com/IENT/YUView>
*   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 3 of the License, or
*   (at your option) any later version.
*
*   In addition, as a special exception, the copyright holders give
*   permission to link the code of portions of this program with the
*   OpenSSL library under certain conditions as described in each
*   individual source file, and distribute linked combinations including
*   the two.
*   
*   You must obey the GNU General Public License in all respects for all
*   of the code used other than OpenSSL. If you modify file(s) with this
*   exception, you may extend this exception to your version of the
*   file(s), but you are not obligated to do so. If you do not wish to do
*   so, delete this exception statement from your version. If you delete
*   this exception statement from all source files in the program, then
*   also delete it here.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FFMPEGLIBRARYTYPES_H
#define FFMPEGLIBRARYTYPES_H

#include <stdint.h>
#include <errno.h>
#include <QString>

/* This file defines all FFmpeg specific defines like structs, enums and some common things like Rational.
 * These things do not change when a new major version of the libraries is released. (If they do, they will
 * have to be moved to the FFmpegVersionHandler).
 * All defines in here were taken from the FFmpeg public API headers. Please see (www.ffmpeg.org).
*/

namespace FFmpeg
{
  // Some structs/enums which actual definition does not interest us.
  struct AVFormatContext;
  struct AVClass;
  struct AVInputFormat;
  struct AVPacket;
  struct AVCodec;
  struct AVCodecContext;
  struct AVCodecParameters;
  struct AVDictionary;
  struct AVFrame;
  struct AVBufferRef;
  struct AVPacketSideData;
  struct AVIOContext;
  struct AVIndexEntry;
  struct AVStreamInternal;
  struct AVFrameSideData;
  struct AVMotionVector;
  struct AVStream;
  struct AVProgram;
  struct AVChapter;
  struct AVPixFmtDescriptor;

  #define AV_LOG_WARNING 24

  // How to convert the avXXX_getVersion() int to major, minor and micro
  #define AV_VERSION_MAJOR(a) ((a) >> 16)
  #define AV_VERSION_MINOR(a) (((a) & 0x00FF00) >> 8)
  #define AV_VERSION_MICRO(a) ((a) & 0xFF)

  #define AV_NUM_DATA_POINTERS 8
  #define AV_TIME_BASE            1000000
  #define AV_INPUT_BUFFER_PADDING_SIZE 32

  // How to construct error tags
  #define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
  #define FFERRTAG(a, b, c, d) (-(int)MKTAG(a, b, c, d))

#if EDOM > 0
  #define AVERROR(e) (-(e))
  #define AVUNERROR(e) (-(e))
#else
  /* Some platforms have E* and errno already negated. */
  #define AVERROR(e) (e)
  #define AVUNERROR(e) (e)
#endif

  // Some error tags
  #define AVERROR_BSF_NOT_FOUND      FFERRTAG(0xF8,'B','S','F') ///< Bitstream filter not found
  #define AVERROR_BUG                FFERRTAG( 'B','U','G','!') ///< Internal bug, also see AVERROR_BUG2
  #define AVERROR_BUFFER_TOO_SMALL   FFERRTAG( 'B','U','F','S') ///< Buffer too small
  #define AVERROR_DECODER_NOT_FOUND  FFERRTAG(0xF8,'D','E','C') ///< Decoder not found
  #define AVERROR_DEMUXER_NOT_FOUND  FFERRTAG(0xF8,'D','E','M') ///< Demuxer not found
  #define AVERROR_ENCODER_NOT_FOUND  FFERRTAG(0xF8,'E','N','C') ///< Encoder not found
  #define AVERROR_EOF                FFERRTAG( 'E','O','F',' ') ///< End of file
  #define AVERROR_EXIT               FFERRTAG( 'E','X','I','T') ///< Immediate exit was requested; the called function should not be restarted
  #define AVERROR_EXTERNAL           FFERRTAG( 'E','X','T',' ') ///< Generic error in an external library
  #define AVERROR_FILTER_NOT_FOUND   FFERRTAG(0xF8,'F','I','L') ///< Filter not found
  #define AVERROR_INVALIDDATA        FFERRTAG( 'I','N','D','A') ///< Invalid data found when processing input
  #define AVERROR_MUXER_NOT_FOUND    FFERRTAG(0xF8,'M','U','X') ///< Muxer not found
  #define AVERROR_OPTION_NOT_FOUND   FFERRTAG(0xF8,'O','P','T') ///< Option not found
  #define AVERROR_PATCHWELCOME       FFERRTAG( 'P','A','W','E') ///< Not yet implemented in FFmpeg, patches welcome
  #define AVERROR_PROTOCOL_NOT_FOUND FFERRTAG(0xF8,'P','R','O') ///< Protocol not found

  #define AVSEEK_FLAG_BACKWARD 1 ///< seek backward
  #define AVSEEK_FLAG_BYTE     2 ///< seeking based on position in bytes
  #define AVSEEK_FLAG_ANY      4 ///< seek to any frame, even non-keyframes
  #define AVSEEK_FLAG_FRAME    8 ///< seeking based on frame number

  #define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))

  typedef struct AVRational
  {
    int num; ///< Numerator
    int den; ///< Denominator
  } AVRational;

  //The exact value of the fractional number is: 'val + num / den'. num is assumed to be 0 <= num < den.
  typedef struct AVFrac 
  {
    int64_t val, num, den;
  } AVFrac;

  typedef struct 
  {
   char *key;
   char *value;
  } AVDictionaryEntry;

  enum AVPictureType {
    AV_PICTURE_TYPE_NONE = 0, ///< Undefined
    AV_PICTURE_TYPE_I,     ///< Intra
    AV_PICTURE_TYPE_P,     ///< Predicted
    AV_PICTURE_TYPE_B,     ///< Bi-dir predicted
    AV_PICTURE_TYPE_S,     ///< S(GMC)-VOP MPEG4
    AV_PICTURE_TYPE_SI,    ///< Switching Intra
    AV_PICTURE_TYPE_SP,    ///< Switching Predicted
    AV_PICTURE_TYPE_BI,    ///< BI type
  };

  enum AVMediaType 
  {
    AVMEDIA_TYPE_UNKNOWN = -1,  ///< Usually treated as AVMEDIA_TYPE_DATA
    AVMEDIA_TYPE_VIDEO,
    AVMEDIA_TYPE_AUDIO,
    AVMEDIA_TYPE_DATA,          ///< Opaque data information usually continuous
    AVMEDIA_TYPE_SUBTITLE,
    AVMEDIA_TYPE_ATTACHMENT,    ///< Opaque data information usually sparse
    AVMEDIA_TYPE_NB
  };

  QString getAVMediaTypeName(AVMediaType type);

  enum AVFrameSideDataType 
  {
    AV_FRAME_DATA_PANSCAN,
    AV_FRAME_DATA_A53_CC,
    AV_FRAME_DATA_STEREO3D,
    AV_FRAME_DATA_MATRIXENCODING,
    AV_FRAME_DATA_DOWNMIX_INFO,
    AV_FRAME_DATA_REPLAYGAIN,
    AV_FRAME_DATA_DISPLAYMATRIX,
    AV_FRAME_DATA_AFD,
    AV_FRAME_DATA_MOTION_VECTORS,
    AV_FRAME_DATA_SKIP_SAMPLES,
    AV_FRAME_DATA_AUDIO_SERVICE_TYPE,
    // The ones below are new in avutil 55
    AV_FRAME_DATA_MASTERING_DISPLAY_METADATA,
    AV_FRAME_DATA_GOP_TIMECODE
  };

  enum AVPixelFormat
  {
    AV_PIX_FMT_NONE = -1
  };

  /* The order / numbering of the values in this enum changes depending on the libavcodec version number 
   * or even how the library was compiled. The id for AV_CODEC_ID_NONE is always 0.
   */
  enum AVCodecID
  {
    AV_CODEC_ID_NONE = 0
  };

  enum AVColorPrimaries 
  {
    AVCOL_PRI_RESERVED0   = 0,
    AVCOL_PRI_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
    AVCOL_PRI_UNSPECIFIED = 2,
    AVCOL_PRI_RESERVED    = 3,
    AVCOL_PRI_BT470M      = 4,  ///< also FCC Title 47 Code of Federal Regulations 73.682 (a)(20)

    AVCOL_PRI_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
    AVCOL_PRI_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    AVCOL_PRI_SMPTE240M   = 7,  ///< functionally identical to above
    AVCOL_PRI_FILM        = 8,  ///< colour filters using Illuminant C
    AVCOL_PRI_BT2020      = 9,  ///< ITU-R BT2020
    AVCOL_PRI_SMPTEST428_1 = 10, ///< SMPTE ST 428-1 (CIE 1931 XYZ)
    AVCOL_PRI_SMPTE431    = 11, ///< SMPTE ST 431-2 (2011)
    AVCOL_PRI_SMPTE432    = 12, ///< SMPTE ST 432-1 D65 (2010)
    AVCOL_PRI_NB                ///< Not part of ABI
  };

  enum AVColorTransferCharacteristic 
  {
    AVCOL_TRC_RESERVED0    = 0,
    AVCOL_TRC_BT709        = 1,  ///< also ITU-R BT1361
    AVCOL_TRC_UNSPECIFIED  = 2,
    AVCOL_TRC_RESERVED     = 3,
    AVCOL_TRC_GAMMA22      = 4,  ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
    AVCOL_TRC_GAMMA28      = 5,  ///< also ITU-R BT470BG
    AVCOL_TRC_SMPTE170M    = 6,  ///< also ITU-R BT601-6 525 or 625 / ITU-R BT1358 525 or 625 / ITU-R BT1700 NTSC
    AVCOL_TRC_SMPTE240M    = 7,
    AVCOL_TRC_LINEAR       = 8,  ///< "Linear transfer characteristics"
    AVCOL_TRC_LOG          = 9,  ///< "Logarithmic transfer characteristic (100:1 range)"
    AVCOL_TRC_LOG_SQRT     = 10, ///< "Logarithmic transfer characteristic (100 * Sqrt(10) : 1 range)"
    AVCOL_TRC_IEC61966_2_4 = 11, ///< IEC 61966-2-4
    AVCOL_TRC_BT1361_ECG   = 12, ///< ITU-R BT1361 Extended Colour Gamut
    AVCOL_TRC_IEC61966_2_1 = 13, ///< IEC 61966-2-1 (sRGB or sYCC)
    AVCOL_TRC_BT2020_10    = 14, ///< ITU-R BT2020 for 10-bit system
    AVCOL_TRC_BT2020_12    = 15, ///< ITU-R BT2020 for 12-bit system
    AVCOL_TRC_SMPTEST2084  = 16, ///< SMPTE ST 2084 for 10-, 12-, 14- and 16-bit systems
    AVCOL_TRC_SMPTEST428_1 = 17, ///< SMPTE ST 428-1
    AVCOL_TRC_ARIB_STD_B67 = 18, ///< ARIB STD-B67, known as "Hybrid log-gamma"
    AVCOL_TRC_NB                 ///< Not part of ABI
  };

  enum AVColorRange 
  {
    AVCOL_RANGE_UNSPECIFIED = 0,
    AVCOL_RANGE_MPEG        = 1, ///< the normal 219*2^(n-8) "MPEG" YUV ranges
    AVCOL_RANGE_JPEG        = 2, ///< the normal     2^n-1   "JPEG" YUV ranges
    AVCOL_RANGE_NB               ///< Not part of ABI
  };

  /**
  * Location of chroma samples.
  *
  * Illustration showing the location of the first (top left) chroma sample of the
  * image, the left shows only luma, the right
  * shows the location of the chroma sample, the 2 could be imagined to overlay
  * each other but are drawn separately due to limitations of ASCII
  *
  *                1st 2nd       1st 2nd horizontal luma sample positions
  *                 v   v         v   v
  *                 ______        ______
  *1st luma line > |X   X ...    |3 4 X ...     X are luma samples,
  *                |             |1 2           1-6 are possible chroma positions
  *2nd luma line > |X   X ...    |5 6 X ...     0 is undefined/unknown position
  */
  enum AVChromaLocation 
  {
    AVCHROMA_LOC_UNSPECIFIED = 0,
    AVCHROMA_LOC_LEFT        = 1, ///< MPEG-2/4 4:2:0, H.264 default for 4:2:0
    AVCHROMA_LOC_CENTER      = 2, ///< MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0
    AVCHROMA_LOC_TOPLEFT     = 3, ///< ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2
    AVCHROMA_LOC_TOP         = 4,
    AVCHROMA_LOC_BOTTOMLEFT  = 5,
    AVCHROMA_LOC_BOTTOM      = 6,
    AVCHROMA_LOC_NB               ///< Not part of ABI
  };

  enum AVColorSpace 
  {
    AVCOL_SPC_RGB         = 0,  ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
    AVCOL_SPC_BT709       = 1,  ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
    AVCOL_SPC_UNSPECIFIED = 2,
    AVCOL_SPC_RESERVED    = 3,
    AVCOL_SPC_FCC         = 4,  ///< FCC Title 47 Code of Federal Regulations 73.682 (a)(20)
    AVCOL_SPC_BT470BG     = 5,  ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
    AVCOL_SPC_SMPTE170M   = 6,  ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    AVCOL_SPC_SMPTE240M   = 7,  ///< functionally identical to above
    AVCOL_SPC_YCOCG       = 8,  ///< Used by Dirac / VC-2 and H.264 FRext, see ITU-T SG16
    AVCOL_SPC_BT2020_NCL  = 9,  ///< ITU-R BT2020 non-constant luminance system
    AVCOL_SPC_BT2020_CL   = 10, ///< ITU-R BT2020 constant luminance system
    AVCOL_SPC_SMPTE2085   = 11, ///< SMPTE 2085, Y'D'zD'x
    AVCOL_SPC_NB                ///< Not part of ABI
  };

  enum AVDiscard
  {
    /* We leave some space between them for extensions (drop some
    * keyframes for intra-only or drop just some bidir frames). */
    AVDISCARD_NONE    =-16, ///< discard nothing
    AVDISCARD_DEFAULT =  0, ///< discard useless packets like 0 size packets in avi
    AVDISCARD_NONREF  =  8, ///< discard all non reference
    AVDISCARD_BIDIR   = 16, ///< discard all bidirectional frames
    AVDISCARD_NONINTRA= 24, ///< discard all non intra frames
    AVDISCARD_NONKEY  = 32, ///< discard all frames except keyframes
    AVDISCARD_ALL     = 48, ///< discard all
  };

  enum AVStreamParseType 
  {
    AVSTREAM_PARSE_NONE,
    AVSTREAM_PARSE_FULL,       /**< full parsing and repack */
    AVSTREAM_PARSE_HEADERS,    /**< Only parse headers, do not repack. */
    AVSTREAM_PARSE_TIMESTAMPS, /**< full parsing and interpolation of timestamps for frames not starting on a packet boundary */
    AVSTREAM_PARSE_FULL_ONCE,  /**< full parsing and repack of the first frame only, only implemented for H.264 currently */
    AVSTREAM_PARSE_FULL_RAW=MKTAG(0,'R','A','W'),       /**< full parsing and repack with timestamp and position generation by parser for raw
                                                        this assumes that each packet in the file contains no demuxer level headers and
                                                        just codec level data, otherwise position generation would fail */
  };

  enum AVFieldOrder 
  {
    AV_FIELD_UNKNOWN,
    AV_FIELD_PROGRESSIVE,
    AV_FIELD_TT,          //< Top coded_first, top displayed first
    AV_FIELD_BB,          //< Bottom coded first, bottom displayed first
    AV_FIELD_TB,          //< Top coded first, bottom displayed first
    AV_FIELD_BT,          //< Bottom coded first, top displayed first
  };

  #define AV_PKT_FLAG_KEY     0x0001 ///< The packet contains a keyframe
  #define AV_PKT_FLAG_CORRUPT 0x0002 ///< The packet content is corrupted
  #define AV_PKT_FLAG_DISCARD 0x0004 ///< Not required for output and should be discarded

  enum AVSampleFormat {
    AV_SAMPLE_FMT_NONE = -1,
    AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
    AV_SAMPLE_FMT_S16,         ///< signed 16 bits
    AV_SAMPLE_FMT_S32,         ///< signed 32 bits
    AV_SAMPLE_FMT_FLT,         ///< float
    AV_SAMPLE_FMT_DBL,         ///< double

    AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
    AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
    AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
    AV_SAMPLE_FMT_FLTP,        ///< float, planar
    AV_SAMPLE_FMT_DBLP,        ///< double, planar
    AV_SAMPLE_FMT_S64,         ///< signed 64 bits
    AV_SAMPLE_FMT_S64P,        ///< signed 64 bits, planar

    AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
  };
}

#endif // FFMPEGLIBRARYTYPES_H
