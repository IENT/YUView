/*
* Copyright © 2018, VideoLAN and dav1d authors
* Copyright © 2018, Two Orioles, LLC
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DAV1D_BLOCKDATA_H__
#define __DAV1D_BLOCKDATA_H__

#include <stddef.h>
#include <stdint.h>

// bl is of this type
enum BlockLevel {
  BL_128X128,
  BL_64X64,
  BL_32X32,
  BL_16X16,
  BL_8X8,
  N_BL_LEVELS,
};

// bs is of this type
enum BlockSize {
  BS_128x128,
  BS_128x64,
  BS_64x128,
  BS_64x64,
  BS_64x32,
  BS_64x16,
  BS_32x64,
  BS_32x32,
  BS_32x16,
  BS_32x8,
  BS_16x64,
  BS_16x32,
  BS_16x16,
  BS_16x8,
  BS_16x4,
  BS_8x32,
  BS_8x16,
  BS_8x8,
  BS_8x4,
  BS_4x16,
  BS_4x8,
  BS_4x4,
  N_BS_SIZES,
};

// bp is of this type
enum BlockPartition {
  PARTITION_NONE,     // [ ] <-.
  PARTITION_H,        // [-]   |
  PARTITION_V,        // [|]   |
  PARTITION_SPLIT,    // [+] --'
  PARTITION_T_TOP_SPLIT,    // [] i.e. split top, H bottom
  PARTITION_T_BOTTOM_SPLIT, // [] i.e. H top, split bottom
  PARTITION_T_LEFT_SPLIT,   // [] i.e. split left, V right
  PARTITION_T_RIGHT_SPLIT,  // [] i.e. V left, split right
  PARTITION_H4,       //
  PARTITION_V4,       // 
  N_PARTITIONS,
  N_SUB8X8_PARTITIONS = PARTITION_T_TOP_SPLIT,
};

// y_mode and uv_mode are of this type
enum IntraPredMode {
  DC_PRED,
  VERT_PRED,
  HOR_PRED,
  DIAG_DOWN_LEFT_PRED,
  DIAG_DOWN_RIGHT_PRED,
  VERT_RIGHT_PRED,
  HOR_DOWN_PRED,
  HOR_UP_PRED,
  VERT_LEFT_PRED,
  SMOOTH_PRED,
  SMOOTH_V_PRED,
  SMOOTH_H_PRED,
  PAETH_PRED,
  N_INTRA_PRED_MODES,
  CFL_PRED = N_INTRA_PRED_MODES,
  N_UV_INTRA_PRED_MODES,
  N_IMPL_INTRA_PRED_MODES = N_UV_INTRA_PRED_MODES,
  LEFT_DC_PRED = DIAG_DOWN_LEFT_PRED,
  TOP_DC_PRED,
  DC_128_PRED,
  Z1_PRED,
  Z2_PRED,
  Z3_PRED,
  FILTER_PRED = N_INTRA_PRED_MODES,
};

enum InterPredMode {
  NEARESTMV,
  NEARMV,
  GLOBALMV,
  NEWMV,
  N_INTER_PRED_MODES,
};

typedef struct motionVector {
  int16_t y, x;
} motionVector;

// tx and max_ytx are of this type
enum TxfmSize {
  TX_4X4,
  TX_8X8,
  TX_16X16,
  TX_32X32,
  TX_64X64,
  RTX_4X8,
  RTX_8X4,
  RTX_8X16,
  RTX_16X8,
  RTX_16X32,
  RTX_32X16,
  RTX_32X64,
  RTX_64X32,
  RTX_4X16,
  RTX_16X4,
  RTX_8X32,
  RTX_32X8,
  RTX_16X64,
  RTX_64X16,
  N_TX_SIZES
};

// comp_type is of this type
enum CompInterType {
  COMP_INTER_NONE,
  COMP_INTER_WEIGHTED_AVG,
  COMP_INTER_AVG,
  COMP_INTER_SEG,
  COMP_INTER_WEDGE,
};

// inter_mode is of this type
enum CompInterPredMode {
  NEARESTMV_NEARESTMV,
  NEARMV_NEARMV,
  NEARESTMV_NEWMV,
  NEWMV_NEARESTMV,
  NEARMV_NEWMV,
  NEWMV_NEARMV,
  GLOBALMV_GLOBALMV,
  NEWMV_NEWMV,
  N_COMP_INTER_PRED_MODES,
};

// interintra_type is of this type
enum InterIntraType {
  INTER_INTRA_NONE,
  INTER_INTRA_BLEND,
  INTER_INTRA_WEDGE,
};

// interintra_mode is of this type
enum InterIntraPredMode {
  II_DC_PRED,
  II_VERT_PRED,
  II_HOR_PRED,
  II_SMOOTH_PRED,
  N_INTER_INTRA_PRED_MODES,
};

enum MotionMode {
  MM_TRANSLATION,
  MM_OBMC,
  MM_WARP,
};

typedef struct Av1Block {
  uint8_t bl, bs, bp;
  uint8_t intra, seg_id, skip_mode, skip, uvtx;
  union {
    struct {
      uint8_t y_mode, uv_mode, tx, pal_sz[2];
      int8_t y_angle, uv_angle, cfl_alpha[2];
    }; // intra
    struct {
      int8_t ref[2];
      uint8_t comp_type, wedge_idx, mask_sign, inter_mode, drl_idx;
      uint8_t interintra_type, interintra_mode, motion_mode;
      uint8_t max_ytx, filter2d;
      uint16_t tx_split[2];
      motionVector mv[2];
    }; // inter
  };
} Av1Block;

#endif // __DAV1D_BLOCKDATA_H__
