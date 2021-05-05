/* -----------------------------------------------------------------------------
The copyright in this software is being made available under the BSD
License, included below. No patent rights, trademark rights and/or 
other Intellectual Property Rights other than the copyrights concerning 
the Software are granted under this license.

For any license concerning other Intellectual Property rights than the software, 
especially patent licenses, a separate Agreement needs to be closed. 
For more information please contact:

Fraunhofer Heinrich Hertz Institute
Einsteinufer 37
10587 Berlin, Germany
www.hhi.fraunhofer.de/vvc
vvc@hhi.fraunhofer.de

Copyright (c) 2018-2021, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. 
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of Fraunhofer nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.


------------------------------------------------------------------------------------------- */

#ifndef _VVDEC_H_
#define _VVDEC_H_

#include "vvdecDecl.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "sei.h"

#define VVDEC_NAMESPACE_BEGIN
#define VVDEC_NAMESPACE_END

#ifdef __cplusplus
extern "C" {
#endif

VVDEC_NAMESPACE_BEGIN


/* vvdecDecoder:
 * opaque handler for the decoder */
typedef struct vvdecDecoder vvdecDecoder;

/* vvdecLoggingCallback:
   callback function to receive messages of the decoder library
*/
typedef void (*vvdecLoggingCallback)(void*, int, const char*, va_list);

/*
  \enum ErrorCodes
  The enum ErrorCodes enumerates error codes returned by the decoder.
*/
typedef enum
{
  VVDEC_OK                    = 0,     // success
  VVDEC_ERR_UNSPECIFIED       = -1,    // unspecified malfunction
  VVDEC_ERR_INITIALIZE        = -2,    // decoder not initialized or tried to initialize multiple times
  VVDEC_ERR_ALLOCATE          = -3,    // internal allocation error
  VVDEC_ERR_DEC_INPUT         = -4,    // decoder input error, decoder input data error
  VVDEC_NOT_ENOUGH_MEM        = -5,    // allocated memory to small to receive decoded data. After allocating sufficient memory the failed call can be repeated.
  VVDEC_ERR_PARAMETER         = -7,    // inconsistent or invalid parameters
  VVDEC_ERR_NOT_SUPPORTED     = -10,   // unsupported request
  VVDEC_ERR_RESTART_REQUIRED  = -11,   // decoder requires restart
  VVDEC_ERR_CPU               = -30,   // unsupported CPU SSE 4.1 needed
  VVDEC_TRY_AGAIN             = -40,   // decoder needs more input and cannot return a picture
  VVDEC_EOF                   = -50    // end of file
}vvdecErrorCodes;

/*
  \enum LogLevel
  The enum LogLevel enumerates supported log levels/verbosity.
*/
typedef enum
{
  VVDEC_SILENT  = 0,
  VVDEC_ERROR   = 1,
  VVDEC_WARNING = 2,
  VVDEC_INFO    = 3,
  VVDEC_NOTICE  = 4,
  VVDEC_VERBOSE = 5,
  VVDEC_DETAILS = 6
}vvdecLogLevel;

/*
  \enum SIMD_Extension
  The enum SIMD_Extension enumerates the supported simd optimizations.
*/
typedef enum
{
  VVDEC_SIMD_DEFAULT  = 0,
  VVDEC_SIMD_SCALAR   = 1,
  VVDEC_SIMD_SSE41    = 2,
  VVDEC_SIMD_SSE42    = 3,
  VVDEC_SIMD_AVX      = 4,
  VVDEC_SIMD_AVX2     = 5,
  VVDEC_SIMD_AVX512   = 6
}vvdecSIMD_Extension;

/*
  \enum vvdecRPRUpscaling
  The enum vvdecRPRUpscaling enumerates supported RPR upscaling handling
*/
typedef enum
{
  VVDEC_UPSCALING_OFF       = 0,     // no RPR scaling
  VVDEC_UPSCALING_COPY_ONLY = 1,     // copy picture into target resolution only
  VVDEC_UPSCALING_RESCALE   = 2      // auto rescale RPR pictures into target resolution
}vvdecRPRUpscaling;

/*
  \enum ColorFormat
  The enum ColorFormat enumerates supported input color formats.
*/
typedef enum
{
  VVDEC_CF_INVALID       = -1,         // invalid color format
  VVDEC_CF_YUV400_PLANAR =  0,         // YUV400 planar color format
  VVDEC_CF_YUV420_PLANAR =  1,         // YUV420 planar color format
  VVDEC_CF_YUV422_PLANAR =  2,         // YUV422 planar color format
  VVDEC_CF_YUV444_PLANAR =  3          // YUV444 planar color format
}vvdecColorFormat;

/*
  The class InterlaceFormat enumerates several supported picture formats.
  The enumeration InterlaceFormat is following the definition of the syntax element pic_struct defined in HEVC standard.
*/
typedef enum
{
  VVDEC_FF_INVALID     = -1,           // invalid interlace format
  VVDEC_FF_PROGRESSIVE = 0,            // progressive coding picture format
  VVDEC_FF_TOP_FIELD   = 1,            // top field picture
  VVDEC_FF_BOT_FIELD   = 2,            // bottom field picture
  VVDEC_FF_TOP_BOT     = 3,            // interlaced frame (top field first in display order)
  VVDEC_FF_BOT_TOP     = 4,            // interlaced frame (bottom field first in display order)
  VVDEC_FF_TOP_BOT_TOP = 5,            // NOT SUPPORTED (top field, bottom field, top field repeated)
  VVDEC_FF_BOT_TOP_BOT = 6,            // NOT SUPPORTED (bottom field, top field, bottom field repeated)
  VVDEC_FF_FRAME_DOUB  = 7,            // NOT SUPPORTED (frame doubling)
  VVDEC_FF_FRAME_TRIP  = 8,            // NOT SUPPORTED (frame tripling)
  VVDEC_FF_TOP_PW_PREV = 9,            // top field    (is paired with previous bottom field)
  VVDEC_FF_BOT_PW_PREV = 10,           // bottom field (is paired with previous top field)
  VVDEC_FF_TOP_PW_NEXT = 11,           // top field    (is paired with next bottom field)
  VVDEC_FF_BOT_PW_NEXT = 12,           // bottom field (is paired with next top field)
}vvdecFrameFormat;

/*
  The class SliceType enumerates several supported slice types.
*/
typedef enum
{
  VVDEC_SLICETYPE_I = 0,
  VVDEC_SLICETYPE_P,
  VVDEC_SLICETYPE_B,
  VVDEC_SLICETYPE_UNKNOWN
}vvdecSliceType;

typedef enum
{
  VVC_NAL_UNIT_CODED_SLICE_TRAIL = 0,  // 0
  VVC_NAL_UNIT_CODED_SLICE_STSA,       // 1
  VVC_NAL_UNIT_CODED_SLICE_RADL,       // 2
  VVC_NAL_UNIT_CODED_SLICE_RASL,       // 3

  VVC_NAL_UNIT_RESERVED_VCL_4,
  VVC_NAL_UNIT_RESERVED_VCL_5,
  VVC_NAL_UNIT_RESERVED_VCL_6,

  VVC_NAL_UNIT_CODED_SLICE_IDR_W_RADL, // 7
  VVC_NAL_UNIT_CODED_SLICE_IDR_N_LP,   // 8
  VVC_NAL_UNIT_CODED_SLICE_CRA,        // 9
  VVC_NAL_UNIT_CODED_SLICE_GDR,        // 10

  VVC_NAL_UNIT_RESERVED_IRAP_VCL_11,
  VVC_NAL_UNIT_RESERVED_IRAP_VCL_12,

  VVC_NAL_UNIT_DCI,                    // 13
  VVC_NAL_UNIT_VPS,                    // 14
  VVC_NAL_UNIT_SPS,                    // 15
  VVC_NAL_UNIT_PPS,                    // 16
  VVC_NAL_UNIT_PREFIX_APS,             // 17
  VVC_NAL_UNIT_SUFFIX_APS,             // 18
  VVC_NAL_UNIT_PH,                     // 19
  VVC_NAL_UNIT_ACCESS_UNIT_DELIMITER,  // 20
  VVC_NAL_UNIT_EOS,                    // 21
  VVC_NAL_UNIT_EOB,                    // 22
  VVC_NAL_UNIT_PREFIX_SEI,             // 23
  VVC_NAL_UNIT_SUFFIX_SEI,             // 24
  VVC_NAL_UNIT_FD,                     // 25

  VVC_NAL_UNIT_RESERVED_NVCL_26,
  VVC_NAL_UNIT_RESERVED_NVCL_27,

  VVC_NAL_UNIT_UNSPECIFIED_28,
  VVC_NAL_UNIT_UNSPECIFIED_29,
  VVC_NAL_UNIT_UNSPECIFIED_30,
  VVC_NAL_UNIT_UNSPECIFIED_31,
  VVC_NAL_UNIT_INVALID
}vvdecNalType;


typedef enum
{
  VVDEC_CT_Y = 0,                      // Y component
  VVDEC_CT_U = 1,                      // U component
  VVDEC_CT_V = 2                       // V component
}vvdecComponentType;


/* vvdecAccessUnit
  The struct vvdecAccessUnit contains attributes that are assigned to the compressed output of the decoder for a specific input picture.
  The structure contains buffer and size information of the compressed payload as well as timing and access information.
  The smallest output unit of VVC decoders are NalUnits. A set of NalUnits that belong to the same access unit are delivered in a continuous bitstream,
  where the NalUnits are separated by three byte start codes.
  The Buffer to retrieve the compressed video chunks has to be allocated by the caller.
  The related attribute payloadSize defines the size of allocated memory whereas payloadUsedSize only defines the size of valid bytes in the bitstream.
*/
typedef struct vvdecAccessUnit
{
  unsigned char*  payload;             // pointer to buffer that retrieves the coded data,
  int             payloadSize;         // size of the allocated buffer in bytes
  int             payloadUsedSize;     // length of the coded data in bytes
  uint64_t        cts;                 // composition time stamp in TicksPerSecond (see VVCDecoderParameter)
  uint64_t        dts;                 // decoding time stamp in TicksPerSecond (see VVCDecoderParameter)
  bool            ctsValid;            // composition time stamp valid flag (true: valid, false: CTS not set)
  bool            dtsValid;            // decoding time stamp valid flag (true: valid, false: DTS not set)
  bool            rap;                 // random access point flag (true: AU is random access point, false: sequential access)
} vvdecAccessUnit;

/* vvdec_accessUnit_alloc:
   Allocates an vvdecAccessUnit instance.
   The returned accessUnit is set to default values.
   The payload memory must be allocated seperately by using vvdec_accessUnit_alloc_payload.
   To free the memory use vvdecAccessUnit_free.
*/
VVDEC_DECL vvdecAccessUnit* vvdec_accessUnit_alloc();

/* vvdec_accessUnit_free:
   release storage of an vvdecAccessUnit instance.
   The payload memory is also released if not done yet.
*/
VVDEC_DECL void vvdec_accessUnit_free(vvdecAccessUnit *accessUnit );

/* vvdec_accessUnit_alloc_payload:
   Allocates the memory for an accessUnit payload.
   To free the memory use vvdecAccessUnit_free_payload.
   When the vvdecAccessUnit memory is released the payload memory is also released.
*/
VVDEC_DECL void vvdec_accessUnit_alloc_payload(vvdecAccessUnit *accessUnit, int payload_size );

/* vvdec_accessUnit_free_payload:
   release storage of the payload in an vvdecAccessUnit instance.
*/
VVDEC_DECL void vvdec_accessUnit_free_payload(vvdecAccessUnit *accessUnit );

/* vvdec_accessUnit_default:
  Initialize vvdecAccessUnit structure to default values
*/
VVDEC_DECL void vvdec_accessUnit_default(vvdecAccessUnit *accessUnit );

/*
  The struct vvdecVui contains Video Usage Inforamtion
*/
typedef struct vvdecVui
{
  bool       aspectRatioInfoPresentFlag;
  bool       aspectRatioConstantFlag;
  bool       nonPackedFlag;
  bool       nonProjectedFlag;
  int        aspectRatioIdc;
  int        sarWidth;
  int        sarHeight;
  bool       colourDescriptionPresentFlag;
  int        colourPrimaries;
  int        transferCharacteristics;
  int        matrixCoefficients;
  bool       progressiveSourceFlag;
  bool       interlacedSourceFlag;
  bool       chromaLocInfoPresentFlag;
  int        chromaSampleLocTypeTopField;
  int        chromaSampleLocTypeBottomField;
  int        chromaSampleLocType;
  bool       overscanInfoPresentFlag;
  bool       overscanAppropriateFlag;
  bool       videoSignalTypePresentFlag;
  bool       videoFullRangeFlag;
}vvdecVui;

/*
  The struct vvdecHrd contains information about the Hypothetical Reference Decoder
*/
typedef struct vvdecHrd
{
  uint32_t   numUnitsInTick;
  uint32_t   timeScale;
  bool       generalNalHrdParamsPresentFlag;
  bool       generalVclHrdParamsPresentFlag;
  bool       generalSamePicTimingInAllOlsFlag;
  uint32_t   tickDivisor;
  bool       generalDecodingUnitHrdParamsPresentFlag;
  uint32_t   bitRateScale;
  uint32_t   cpbSizeScale;
  uint32_t   cpbSizeDuScale;
  uint32_t   hrdCpbCnt;
}vvdecHrd;


/*
  The struct vvdecPicAttributes contains additional picture side information
*/
typedef struct vvdecPicAttributes
{
  vvdecNalType    nalType;             // nal unit type
  vvdecSliceType  sliceType;           // slice type (I/P/B) */
  bool            isRefPic;            // reference picture
  uint32_t        temporalLayer;       // temporal layer
  uint64_t        poc;                 // picture order count
  uint32_t        bits;                // bits of the compr. image packet
  vvdecVui       *vui;                 // if available, pointer to VUI (Video Usability Information)
  vvdecHrd       *hrd;                 // if available, pointer to HRD (Hypothetical Reference Decoder)
} vvdecPicAttributes;

/*
  The struct vvdecPlane contains information about a plane (component) of a frame
  that has been return from the decoder.
*/
typedef struct vvdecPlane
{
  unsigned char* ptr;                  // pointer to plane buffer
  uint32_t       width;                // width of the plane
  uint32_t       height;               // height of the plane
  uint32_t       stride;               // stride (width + left margin + right margins) of plane in samples
  uint32_t       bytesPerSample;       // number of bytes per sample
} vvdecPlane;

/*
  The struct vvdecFrame contains a decoded frame and consists of the picture planes and basic picture information.
  The vvdecPicAttributes struct holds additional picture information.
*/
typedef struct vvdecFrame
{
  vvdecPlane          planes[ 3 ];     // component plane for yuv
  uint32_t            numPlanes;       // number of color components
  uint32_t            width;           // width of the luminance plane
  uint32_t            height;          // height of the luminance plane
  uint32_t            bitDepth;        // bit depth of input signal (8: depth 8 bit, 10: depth 10 bit  )
  vvdecFrameFormat    frameFormat;     // frame format (progressive/interlaced)
  vvdecColorFormat    colorFormat;     // color format
  uint64_t            sequenceNumber;  // sequence number of the picture
  uint64_t            cts;             // composition time stamp in TicksPerSecond
  bool                ctsValid;        // composition time stamp valid flag (true: valid, false: CTS not set)
  vvdecPicAttributes *picAttributes;   // pointer to vvdecPicAttributes that might be NULL, containing decoder side information
}vvdecFrame;

/*
  The struct vvdecParams is a container for decoder configuration parameters.
  Use vvdec_default_params() to set default values.
*/
typedef struct vvdecParams
{
  int                 threads;           // thread count        ( default: -1 )
  int                 parseThreads;      // parser thread count ( default: -1 )
  vvdecRPRUpscaling   upscaleOutput;     // do internal upscaling of rpl pictures to dest. resolution ( default: 0 )
  vvdecLogLevel       logLevel;          // verbosity level
  bool                verifyPictureHash; // verify picture, if digest is available, true: check hash in SEI messages if available, false: ignore SEI message
  vvdecSIMD_Extension simd;              // set specific simd optimization (default: max. availalbe)
} vvdecParams;

/* vvdec_params_default:
  Initialize vvdec_params structure to default values
*/
VVDEC_DECL void vvdec_params_default(vvdecParams *param);

/* vvdec_params_alloc:
   Allocates an vvdec_params_alloc instance.
   The returned params struct is set to default values.
*/
VVDEC_DECL vvdecParams* vvdec_params_alloc();

/* vvdec_params_free:
   release storage of an vvdec_params instance.
*/
VVDEC_DECL void vvdec_params_free(vvdecParams *params );

/* vvdec_get_version
  This method returns the the decoder version as string.
  \param[in]  none
  \retval[ ]  const char* version number as string
*/
VVDEC_DECL const char* vvdec_get_version();

/* vvdec_decoder_open
  This method initializes the decoder instance.
  This method is used to initially set up the decoder with the assigned decoder parameter struct.
  The method fails if the assigned parameter struct does not pass the consistency check.
  Other possibilities for an unsuccessful memory initialization, or an machine with
  insufficient CPU-capabilities.
  \param[in]  vvdec_params_t pointer of vvdec_params struct that holds initial decoder parameters.
  \retval     vvdec_params_t pointer of the decoder handler if successful, otherwise NULL
  \pre        The decoder must not be initialized (pointer of decoder handler must be null).
*/
VVDEC_DECL vvdecDecoder* vvdec_decoder_open( vvdecParams *);

/* vvdec_decoder_close
 This method resets the decoder instance.
 This method clears the decoder and releases all internally allocated memory.
 Calling uninit cancels all pending decoding calls. In order to finish pending pictures use the flush method.
 \param[in]  vvdecDecoder pointer of decoder handler
 \retval     int if non-zero an error occurred (see ErrorCodes), otherwise VVDEC_OK indicates success.
 \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL int vvdec_decoder_close(vvdecDecoder *);

/* vvdec_set_logging_callback
  Set a logging callback. To disable set callback NULL.
  \param[in]   vvdecDecoder pointer of decoder handler
  \param[in]   vvdecLoggingCallback implementation of the callback that is called when logging messages are written
  \retval      int if non-zero an error occurred (see ErrorCodes), otherwise VVDEC_OK indicates success.
  \pre         The decoder has to be initialized successfully.
 */
VVDEC_DECL int vvdec_set_logging_callback(vvdecDecoder*, vvdecLoggingCallback callback );

/* vvdec_decode
  This method decodes a compressed image packet (bitstream).
  Compressed image packet are passed to the decoder in decoder order. A picture is returned by filling the assigned Picture struct.
  A picture is valid if the decoder call returns success and the Picture is not null.
  If the AccessUnit  m_iBufSize = 0, the decoder just returns a pending pictures chunk if available.
  \param[in]   vvdecDecoder pointer of decoder handler
  \param[in]   vvdecAccessUnit_t pointer of AccessUnit that retrieves compressed access units and side information, data are valid if UsedSize attribute is non-zero and the call was successful.
  \param[out]  vvdecFrame pointer to pointer of frame structure containing a uncompressed picture and meta information.
  \retval      int if non-zero an error occurred or more data is needed, otherwise the retval indicates success VVDEC_OK
  \pre         The decoder has to be initialized successfully.
*/
VVDEC_DECL int vvdec_decode( vvdecDecoder *, vvdecAccessUnit *accessUnit, vvdecFrame **frame );

/* vvdec_flush
  This method flushes the decoder.
  This call is used to get outstanding pictures after all compressed packets have been passed over into the decoder using the decode call.
  Using the flush method the decoder is signaled that there are no further compressed packets to decode.
  The caller should repeat the flush call until all pending pictures has been delivered to the caller, which is when the the function returns VVDEC_EOF or no picture.
  \param[in]  vvdecDecoder pointer to decoder handler
  \param[out] vvdecFrame pointer to pointer of frame structure containing a uncompressed picture and meta information.
  \retval     int if non-zero an error occurred, otherwise the retval indicates success VVDEC_OK
  \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL int vvdec_flush( vvdecDecoder *, vvdecFrame **frame );

/* vvdec_find_frame_sei
  This method finds SEI message in a given picture.
  To get the correct sei data for a given SEIPayloadType the payload have to be casted to the payload type.
  \param[in]  vvdecDecoder pointer of decoder handler
  \param[in]  SEIPayloadType payload type to search for
  \param[in]  vvdecFrame pointer of frame to search for sei
  \param[out] vvdecSEI pointer to found sei message, NULL if not found
  \retval     int if non-zero an error occurred, otherwise the retval indicates success VVDEC_OK
  \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL vvdecSEI* vvdec_find_frame_sei( vvdecDecoder *, vvdecSEIPayloadType seiPayloadType, vvdecFrame *frame );

/* vvdec_frame_unref
  This method unreference an picture and frees the memory.
  This call is used to free the memory of an picture which is not used anymore.
  \param[in]  vvdecDecoder pointer of decoder handler
  \param[out] vvdecFrame pointer of frame to delete
  \retval     int if non-zero an error occurred, otherwise the retval indicates success VVDEC_OK
  \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL int vvdec_frame_unref( vvdecDecoder *, vvdecFrame *frame );

/* vvdec_get_hash_error_count
 This method returns the number of found errors if PictureHash SEI is enabled.
 \param[in]  vvdecDecoder pointer of decoder handler
 \retval     int if non-zero an error occurred, otherwise 0 indicates success.
 \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL int vvdec_get_hash_error_count( vvdecDecoder * );


/* vvdec_get_dec_information
 This method returns general decoder information
 \param[in]  vvdecDecoder pointer of decoder handler
 \retval     const char* decoder information
 \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL const char* vvdec_get_dec_information( vvdecDecoder * );


/* vvdec_get_last_error
 This method returns the last occurred error as a string.
 \param[in]  vvdecDecoder pointer of decoder handler
 \retval     const char* empty string for no error assigned
 \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL const char* vvdec_get_last_error( vvdecDecoder * );

/* vvdec_get_last_additional_error
 This method returns additional information about the last occurred error as a string (if availalbe).
 \param[in]  vvdecDecoder pointer of decoder handler
 \retval     const char* empty string for no error assigned
 \pre        The decoder has to be initialized successfully.
*/
VVDEC_DECL const char* vvdec_get_last_additional_error( vvdecDecoder * );

/* vvdec_get_error_msg
 This function returns a string according to the passed parameter nRet.
 \param[in]  nRet return value code (see ErrorCodes) to translate
 \retval[ ]  const char* empty string for no error
*/
VVDEC_DECL const char* vvdec_get_error_msg( int nRet );

/* vvdec_get_nal_unit_type
 This function returns the NalType of a given AccessUnit.
 \param[in]  vvdecAccessUnit_t pointer of accessUnit that retrieves compressed access units and
             side information, data are valid if UsedSize attribute is non-zero and the call was successful.
 \retval[ ]  NalType found Nal Unit type
*/
VVDEC_DECL vvdecNalType vvdec_get_nal_unit_type ( vvdecAccessUnit *accessUnit );

/* vvdec_get_nal_unit_type_name
 This function returns the name of a given NalType
 \param[in]  NalType value of enum NalType
 \retval[ ]  const char* NalType as string
*/
VVDEC_DECL const char* vvdec_get_nal_unit_type_name( vvdecNalType t );

/* vvdec_is_nal_unit_slice
 This function returns true if a given NalType is of type picture or slice
 \param[in]  NalType value of enum NalType
 \retval[ ]  bool true if slice/picture, else false
*/
VVDEC_DECL bool vvdec_is_nal_unit_slice ( vvdecNalType t );

VVDEC_NAMESPACE_END

#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_VVDEC_H_*/
