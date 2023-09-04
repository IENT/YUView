/* -----------------------------------------------------------------------------
The copyright in this software is being made available under the Clear BSD
License, included below. No patent rights, trademark rights and/or 
other Intellectual Property Rights other than the copyrights concerning 
the Software are granted under this license.

The Clear BSD License

Copyright (c) 2018-2023, Fraunhofer-Gesellschaft zur Förderung der angewandten Forschung e.V. & The VVdeC Authors.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted (subject to the limitations in the disclaimer below) provided that
the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

     * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

     * Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.


------------------------------------------------------------------------------------------- */

#ifndef VVDEC_SEI_H
#define VVDEC_SEI_H

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
  vvdecCompModelIntensityValues intensityValues[256];
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
  uint8_t     rwpTransformType[256];
  bool        rwpGuardBandFlag[256];
  uint32_t    projRegionWidth[256];
  uint32_t    projRegionHeight[256];
  uint32_t    rwpProjRegionTop[256];
  uint32_t    projRegionLeft[256];
  uint16_t    packedRegionWidth[256];
  uint16_t    packedRegionHeight[256];
  uint16_t    packedRegionTop[256];
  uint16_t    packedRegionLeft[256];
  uint8_t     rwpLeftGuardBandWidth[256];
  uint8_t     rwpRightGuardBandWidth[256];
  uint8_t     rwpTopGuardBandHeight[256];
  uint8_t     rwpBottomGuardBandHeight[256];
  bool        rwpGuardBandNotUsedForPredFlag[256];
  uint8_t     rwpGuardBandType[4*256];
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

#endif /*VVDEC_SEI_H*/
