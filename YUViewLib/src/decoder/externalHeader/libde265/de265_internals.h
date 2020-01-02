/*
* H.265 video codec.
* Copyright (c) 2013-2014 struktur AG, Dirk Farin <farin@struktur.de>
*
* This file is part of libde265.
*
* libde265 is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation, either version 3 of
* the License, or (at your option) any later version.
*
* libde265 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with libde265.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DE265_DEBUG_H
#define DE265_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "de265-version.h"
#include "de265.h"

#if defined(_MSC_VER) && !defined(LIBDE265_STATIC_BUILD)
#ifdef LIBDE265_EXPORTS
#define LIBDE265_API __declspec(dllexport)
#else
#define LIBDE265_API __declspec(dllimport)
#endif
#elif HAVE_VISIBILITY
#ifdef LIBDE265_EXPORTS
#define LIBDE265_API __attribute__((__visibility__("default")))
#else
#define LIBDE265_API
#endif
#else
#define LIBDE265_API
#endif

#if __GNUC__
#define LIBDE265_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define LIBDE265_DEPRECATED __declspec(deprecated)
#else
#define LIBDE265_DEPRECATED
#endif

#if defined(_MSC_VER)
#define LIBDE265_INLINE __inline
#else
#define LIBDE265_INLINE inline
#endif

enum de265_internals_param {
  DE265_INTERNALS_DECODER_PARAM_SAVE_PREDICTION=0,  // If set, the prediction signal will be saved alongside the reconstruction
  DE265_INTERNALS_DECODER_PARAM_SAVE_RESIDUAL,      // If set, the residual signal will be saved alongside the reconstruction
  DE265_INTERNALS_DECODER_PARAM_SAVE_TR_COEFF       // If set, the transform coefficients will be saved alongside the reconstruction
};

LIBDE265_API void de265_internals_set_parameter_bool(de265_decoder_context*, enum de265_internals_param param, int value);

LIBDE265_API const uint8_t* de265_internals_get_image_plane(const struct de265_image* img, de265_internals_param signal, int channel, int* out_stride);

/// Get the number of CTBs in the image
LIBDE265_API void de265_internals_get_CTB_Info_Layout(const struct de265_image *img, int *widthInUnits, int *heightInUnits, int *log2UnitSize);

/* Get the slice index of each CTB in the image. Take cake that the array idxArray is at least
 * as long as there are CTBs (see de265_internals_get_numCTB()).
 */
LIBDE265_API void de265_internals_get_CTB_sliceIdx(const struct de265_image *img, uint16_t *idxArray);

/// Get the number of CBs in the image
LIBDE265_API void de265_internals_get_CB_Info_Layout(const struct de265_image *img, int *widthInUnits, int *heightInUnits, int *log2UnitSize);

/* Get the coding block info. The values are stored in the following way:
   * This function returns an array of values, sorted in a grid.
   * You can get the layout of the grid by calling internals_get_CB_Info_Layout.
   * The values are then arranged in a raster scan so the conversion from 
   * a unit position (x,y) to an index is: y*widthInUnits+x

   * Each value in this array contains the following infomation:
   * Bits 0:2 - The cb size in log2 pixels. Every CB can span multiple values in this array.
                Only the top left most unit contains a value. All others are set to 0. (3 Bits)
   * Bits 3:5 - The part size (3 Bits)
   * Bits 6:7 - The prediction mode (0: INTRA, 1: INTER, 2: SKIP) (2 Bits)
   * Bit  8   - PCM flag (1 Bit)
   * Bit  9   - CU Transquant bypass flag (1 Bit)
  */
LIBDE265_API void de265_internals_get_CB_info(const struct de265_image *img, uint16_t *idxArray);

/// Get the pb info array layout
LIBDE265_API void de265_internals_get_PB_Info_layout(const struct de265_image *img, int *widthInUnits, int *heightInUnits, int *log2UnitSize);

/// Get the prediction block info (motion information) 
LIBDE265_API void de265_internals_get_PB_info(const struct de265_image *img, int16_t *refPOC0, int16_t *refPOC1, int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);

/// Get intra direction array layout
LIBDE265_API void de265_internals_get_IntraDir_Info_layout(const struct de265_image *img, int *widthInUnits, int *heightInUnits, int *log2UnitSize);

/// Get intra prediction direction array
LIBDE265_API void de265_internals_get_intraDir_info(const struct de265_image *img, uint8_t *intraDir, uint8_t *intraDirChroma);

/// Get TU info array layout
LIBDE265_API void de265_internals_get_TUInfo_Info_layout(const struct de265_image *img, int *widthInUnits, int *heightInUnits, int *log2UnitSize);

/// Get TU info
LIBDE265_API void de265_internals_get_TUInfo_info(const struct de265_image *img, uint8_t *tuInfo);

#ifdef __cplusplus
}
#endif

#endif
