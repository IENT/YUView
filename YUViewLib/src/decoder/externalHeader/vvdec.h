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

#define VVDEC_DECL

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdint.h>

typedef enum
{
  VVDEC_BUFFERING_PERIOD                     = 0,
  VVDEC_PICTURE_TIMING                       = 1,
  VVDEC_FILLER_PAYLOAD                       = 3,
  VVDEC_USER_DATA_REGISTERED_ITU_T_T35       = 4,
  VVDEC_USER_DATA_UNREGISTERED               = 5,
  VVDEC_FILM_GRAIN_CHARACTERISTICS           = 19,
  VVDEC_FRAME_PACKING                        = 45,
  VVDEC_PARAMETER_SETS_INCLUSION_INDICATION  = 129,
  VVDEC_DECODING_UNIT_INFO                   = 130,
  VVDEC_DECODED_PICTURE_HASH                 = 132,
  VVDEC_SCALABLE_NESTING                     = 133,
  VVDEC_MASTERING_DISPLAY_COLOUR_VOLUME      = 137,
  VVDEC_DEPENDENT_RAP_INDICATION             = 145,
  VVDEC_EQUIRECTANGULAR_PROJECTION           = 150,
  VVDEC_SPHERE_ROTATION                      = 154,
  VVDEC_REGION_WISE_PACKING                  = 155,
  VVDEC_OMNI_VIEWPORT                        = 156,
  VVDEC_GENERALIZED_CUBEMAP_PROJECTION       = 153,
  VVDEC_FRAME_FIELD_INFO                     = 168,
  VVDEC_SUBPICTURE_LEVEL_INFO                = 203,
  VVDEC_SAMPLE_ASPECT_RATIO_INFO             = 204,
  VVDEC_CONTENT_LIGHT_LEVEL_INFO             = 144,
  VVDEC_ALTERNATIVE_TRANSFER_CHARACTERISTICS = 147,
  VVDEC_AMBIENT_VIEWING_ENVIRONMENT          = 148,
  VVDEC_CONTENT_COLOUR_VOLUME                = 149,
  VVDEC_SEI_UNKNOWN                          = -1,
}vvdecSEIPayloadType;

typedef enum
{
  VVDEC_LEVEL_NONE = 0,
  VVDEC_LEVEL1   = 16,
  VVDEC_LEVEL2   = 32,
  VVDEC_LEVEL2_1 = 35,
  VVDEC_LEVEL3   = 48,
  VVDEC_LEVEL3_1 = 51,
  VVDEC_LEVEL4   = 64,
  VVDEC_LEVEL4_1 = 67,
  VVDEC_LEVEL5   = 80,
  VVDEC_LEVEL5_1 = 83,
  VVDEC_LEVEL5_2 = 86,
  VVDEC_LEVEL6   = 96,
  VVDEC_LEVEL6_1 = 99,
  VVDEC_LEVEL6_2 = 102,
  VVDEC_LEVEL15_5 = 255,
}vvdecLevel;


typedef enum
{
  VVDEC_HASHTYPE_MD5             = 0,
  VVDEC_HASHTYPE_CRC             = 1,
  VVDEC_HASHTYPE_CHECKSUM        = 2,
  VVDEC_HASHTYPE_NONE            = 3,
  VVDEC_NUMBER_OF_HASHTYPES      = 4
}vvdecHashType;

/* vvdecSEI
  The struct vvdecSEI contains the payload of a SEI message.
  To get the data from the payload the SEI has to be type casted into target type
  e.g.
  vvdecSEIBufferingPeriod* s = reinterpret_cast<vvdecSEIBufferingPeriod *>(sei->payload);
*/
typedef struct vvdecSEI
{
  vvdecSEIPayloadType  payloadType;     /* payload type as defined in sei.h */
  unsigned int         size;            /* size of payload in bytes */
  void                *payload;         /* payload structure as defined in sei.h */
}vvdecSEI;

typedef struct vvdecSEIBufferingPeriod
{
  bool        bpNalCpbParamsPresentFlag;
  bool        bpVclCpbParamsPresentFlag;
  uint32_t    initialCpbRemovalDelayLength;
  uint32_t    cpbRemovalDelayLength;
  uint32_t    dpbOutputDelayLength;
  int         bpCpbCnt;
  uint32_t    duCpbRemovalDelayIncrementLength;
  uint32_t    dpbOutputDelayDuLength;
  uint32_t    initialCpbRemovalDelay [7][32][2];
  uint32_t    initialCpbRemovalOffset[7][32][2];
  bool        concatenationFlag;
  uint32_t    auCpbRemovalDelayDelta;
  bool        cpbRemovalDelayDeltasPresentFlag;
  int         numCpbRemovalDelayDeltas;
  int         bpMaxSubLayers;
  uint32_t    cpbRemovalDelayDelta[15];
  bool        bpDecodingUnitHrdParamsPresentFlag;
  bool        decodingUnitCpbParamsInPicTimingSeiFlag;
  bool        decodingUnitDpbDuParamsInPicTimingSeiFlag;
  bool        sublayerInitialCpbRemovalDelayPresentFlag;
  bool        additionalConcatenationInfoPresentFlag;
  uint32_t    maxInitialRemovalDelayForConcatenation;
  bool        sublayerDpbOutputOffsetsPresentFlag;
  uint32_t    dpbOutputTidOffset[7];
  bool        altCpbParamsPresentFlag;
  bool        useAltCpbParamsFlag;
} vvdecSEIBufferingPeriod;


typedef struct vvdecSEIPictureTiming
{
  bool        ptSubLayerDelaysPresentFlag[7];
  bool        cpbRemovalDelayDeltaEnabledFlag[7];
  uint32_t    cpbRemovalDelayDeltaIdx[7];
  uint32_t    auCpbRemovalDelay[7];
  uint32_t    picDpbOutputDelay;
  uint32_t    picDpbOutputDuDelay;
  uint32_t    numDecodingUnits;
  bool        duCommonCpbRemovalDelayFlag;
  uint32_t    duCommonCpbRemovalDelay[7];
  uint32_t    numNalusInDu[32];
  uint32_t    duCpbRemovalDelay[32*7+7];
  bool        cpbAltTimingInfoPresentFlag;
  uint32_t    nalCpbAltInitialRemovalDelayDelta[7][32];
  uint32_t    nalCpbAltInitialRemovalOffsetDelta[7][32];
  uint32_t    nalCpbDelayOffset[7];
  uint32_t    nalDpbDelayOffset[7];
  uint32_t    vclCpbAltInitialRemovalDelayDelta[7][32];
  uint32_t    vclCpbAltInitialRemovalOffsetDelta[7][32];
  uint32_t    vclCpbDelayOffset[7];
  uint32_t    vclDpbDelayOffset[7];
  int         ptDisplayElementalPeriods;
} vvdecSEIPictureTiming;


typedef struct vvdecSEIUserDataRegistered
{
  uint16_t    ituCountryCode;
  uint32_t    userDataLength;
  uint8_t    *userData;
} vvdecSEIUserDataRegistered;

typedef struct vvdecSEIUserDataUnregistered
{
  uint8_t     uuid_iso_iec_11578[16];
  uint32_t    userDataLength;
  uint8_t    *userData;
} vvdecSEIUserDataUnregistered;


typedef struct vvdecCompModelIntensityValues
{
  uint8_t     intensityIntervalLowerBound;
  uint8_t     intensityIntervalUpperBound;
  int         compModelValue[6];
}vvdecCompModelIntensityValues;

typedef struct vvdecCompModel
{
  bool                          presentFlag;
  uint8_t                       numModelValues;
  vvdecCompModelIntensityValues intensityValues[265];
}vvdecCompModel;

typedef struct vvdecSEIFilmGrainCharacteristics
{
  bool             filmGrainCharacteristicsCancelFlag;
  uint8_t          filmGrainModelId;
  bool             separateColourDescriptionPresentFlag;
  uint8_t          filmGrainBitDepthLuma;
  uint8_t          filmGrainBitDepthChroma;
  bool             filmGrainFullRangeFlag;
  uint8_t          filmGrainColourPrimaries;
  uint8_t          filmGrainTransferCharacteristics;
  uint8_t          filmGrainMatrixCoeffs;
  uint8_t          blendingModeId;
  uint8_t          log2ScaleFactor;
  vvdecCompModel   compModel[3];
  bool             filmGrainCharacteristicsPersistenceFlag;
}vvdecSEIFilmGrainCharacteristics;

typedef struct vvdecSEIFramePacking
{
  int         arrangementId;
  bool        arrangementCancelFlag;
  int         arrangementType;
  bool        quincunxSamplingFlag;
  int         contentInterpretationType;
  bool        spatialFlippingFlag;
  bool        frame0FlippedFlag;
  bool        fieldViewsFlag;
  bool        currentFrameIsFrame0Flag;
  bool        frame0SelfContainedFlag;
  bool        frame1SelfContainedFlag;
  int         frame0GridPositionX;
  int         frame0GridPositionY;
  int         frame1GridPositionX;
  int         frame1GridPositionY;
  int         arrangementReservedByte;
  bool        arrangementPersistenceFlag;
  bool        upsampledAspectRatio;
}vvdecSEIFramePacking;

typedef struct vvdecSEIParameterSetsInclusionIndication
{
  int         selfContainedClvsFlag;
}vvdecSEIParameterSetsInclusionIndication;

typedef struct vvdecSEIDecodingUnitInfo
{
  int         decodingUnitIdx;
  bool        duiSubLayerDelaysPresentFlag[7];
  int         duSptCpbRemovalDelayIncrement[7];
  bool        dpbOutputDuDelayPresentFlag;
  int         picSptDpbOutputDuDelay;
}vvdecSEIDecodingUnitInfo;


typedef struct vvdecSEIDecodedPictureHash
{
  vvdecHashType method;
  bool          singleCompFlag;
  int           digist_length;
  unsigned char digest[16*3];
}vvdecSEIDecodedPictureHash;


typedef struct vvdecSEIScalableNesting
{
  bool        snOlsFlag;
  bool        snSubpicFlag;
  uint32_t    snNumOlss;
  uint32_t    snOlsIdxDelta[64];
  uint32_t    snOlsIdx[64];
  bool        snAllLayersFlag;
  uint32_t    snNumLayers;
  uint8_t     snLayerId[64];
  uint32_t    snNumSubpics;
  uint8_t     snSubpicIdLen;
  uint16_t    snSubpicId[64];
  uint32_t    snNumSEIs;

  vvdecSEI* nestedSEIs[64];
}vvdecSEIScalableNesting;

typedef struct vvdecSEIMasteringDisplayColourVolume
{
  uint32_t    maxLuminance;
  uint32_t    minLuminance;
  uint16_t    primaries[3][2];
  uint16_t    whitePoint[2];
}vvdecSEIMasteringDisplayColourVolume;

typedef struct vvdecSEIDependentRapIndication
{
} vvdecSEIDependentRapIndication;

typedef struct vvdecSEIEquirectangularProjection
{
  bool        erpCancelFlag;
  bool        erpPersistenceFlag;
  bool        erpGuardBandFlag;
  uint8_t     erpGuardBandType;
  uint8_t     erpLeftGuardBandWidth;
  uint8_t     erpRightGuardBandWidth;
}vvdecSEIEquirectangularProjection;

typedef struct vvdecSEISphereRotation
{
  bool        sphereRotationCancelFlag;
  bool        sphereRotationPersistenceFlag;
  int         sphereRotationYaw;
  int         sphereRotationPitch;
  int         sphereRotationRoll;
}vvdecSEISphereRotation;

typedef struct vvdecSEIRegionWisePacking
{
  bool        rwpCancelFlag;
  bool        rwpPersistenceFlag;
  bool        constituentPictureMatchingFlag;
  int         numPackedRegions;
  int         projPictureWidth;
  int         projPictureHeight;
  int         packedPictureWidth;
  int         packedPictureHeight;
  uint8_t     rwpTransformType[265];
  bool        rwpGuardBandFlag[265];
  uint32_t    projRegionWidth[265];
  uint32_t    projRegionHeight[265];
  uint32_t    rwpProjRegionTop[265];
  uint32_t    projRegionLeft[265];
  uint16_t    packedRegionWidth[265];
  uint16_t    packedRegionHeight[265];
  uint16_t    packedRegionTop[265];
  uint16_t    packedRegionLeft[265];
  uint8_t     rwpLeftGuardBandWidth[265];
  uint8_t     rwpRightGuardBandWidth[265];
  uint8_t     rwpTopGuardBandHeight[265];
  uint8_t     rwpBottomGuardBandHeight[265];
  bool        rwpGuardBandNotUsedForPredFlag[265];
  uint8_t     rwpGuardBandType[4*265];
}vvdecSEIRegionWisePacking;

typedef struct vvdecOmniViewportRegion
{
  int         azimuthCentre;
  int         elevationCentre;
  int         tiltCentre;
  uint32_t    horRange;
  uint32_t    verRange;
}vvdecOmniViewportRegion;

typedef struct vvdecSEIOmniViewport
{
  uint32_t                omniViewportId;
  bool                    omniViewportCancelFlag;
  bool                    omniViewportPersistenceFlag;
  uint8_t                 omniViewportCnt;
  vvdecOmniViewportRegion omniViewportRegions[16];
}vvdecSEIOmniViewport;


typedef struct vvdecSEIGeneralizedCubemapProjection
{
  bool        gcmpCancelFlag;
  bool        gcmpPersistenceFlag;
  uint8_t     gcmpPackingType;
  uint8_t     gcmpMappingFunctionType;
  uint8_t     gcmpFaceIndex[6];
  uint8_t     gcmpFaceRotation[6];
  uint8_t     gcmpFunctionCoeffU[6];
  bool        gcmpFunctionUAffectedByVFlag[6];
  uint8_t     gcmpFunctionCoeffV[6];
  bool        gcmpFunctionVAffectedByUFlag[6];
  bool        gcmpGuardBandFlag;
  uint8_t     gcmpGuardBandType;
  bool        gcmpGuardBandBoundaryExteriorFlag;
  uint8_t     gcmpGuardBandSamples;
}vvdecSEIGeneralizedCubemapProjection;

typedef struct vvdecSEIFrameFieldInfo
{
  bool        fieldPicFlag;
  bool        bottomFieldFlag;
  bool        pairingIndicatedFlag;
  bool        pairedWithNextFieldFlag;
  bool        displayFieldsFromFrameFlag;
  bool        topFieldFirstFlag;
  int         displayElementalPeriods;
  int         sourceScanType;
  bool        duplicateFlag;
}vvdecSEIFrameFieldInfo;

typedef struct vvdecSEISubpictureLevelInfo
{
  int         numRefLevels;
  bool        explicitFractionPresentFlag;
  bool        cbrConstraintFlag;
  int         numSubpics;
  int         sliMaxSublayers;
  bool        sliSublayerInfoPresentFlag;
  int         nonSubpicLayersFraction[6][6];
  vvdecLevel  refLevelIdc[6][6];
  int         refLevelFraction[6][64][6];
}vvdecSEISubpictureLevelInfo;

typedef struct vvdecSEISampleAspectRatioInfo
{
  bool        sariCancelFlag;
  bool        sariPersistenceFlag;
  int         sariAspectRatioIdc;
  int         sariSarWidth;
  int         sariSarHeight;
}vvdecSEISampleAspectRatioInfo;

typedef struct vvdecSEIContentLightLevelInfo
{
  uint16_t    maxContentLightLevel;
  uint16_t    maxPicAverageLightLevel;
}vvdecSEIContentLightLevelInfo;

typedef struct vvdecSEIAlternativeTransferCharacteristics
{
  uint8_t     preferred_transfer_characteristics;
}vvdecSEIAlternativeTransferCharacteristics;

typedef struct vvdecSEIAmbientViewingEnvironment
{
  uint32_t    ambientIlluminance;
  uint16_t    ambientLightX;
  uint16_t    ambientLightY;
}vvdecSEIAmbientViewingEnvironment;

typedef struct vvdecSEIContentColourVolume
{
  bool        ccvCancelFlag;
  bool        ccvPersistenceFlag;
  bool        ccvPrimariesPresentFlag;
  bool        ccvMinLuminanceValuePresentFlag;
  bool        ccvMaxLuminanceValuePresentFlag;
  bool        ccvAvgLuminanceValuePresentFlag;
  int         ccvPrimariesX[3];
  int         ccvPrimariesY[3];
  uint32_t    ccvMinLuminanceValue;
  uint32_t    ccvMaxLuminanceValue;
  uint32_t    ccvAvgLuminanceValue;
}vvdecSEIContentColourVolume;

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


#ifdef __cplusplus
}
#endif /*__cplusplus */

#endif /*_VVDEC_H_*/
