/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDR_COLOR_CONVERSION_H
#define HDR_COLOR_CONVERSION_H

#include "HDRTypes.h"
#include "../yuv/PixelFormatYUV.h"

namespace video::hdr
{

/**
 * @brief HDR Color Conversion Utilities
 * 
 * This class provides static methods for building YUV->RGB color conversion
 * matrices used in HDR rendering. It handles BT.709, BT.601, and BT.2020
 * color spaces with both limited and full range support.
 * 
 * The matrices are formatted for GPU shader consumption with std140 layout.
 * 
 * Responsibilities:
 * - Build YUV->RGB color conversion matrices
 * - Handle limited/full range Y offset
 * - Support multiple color space standards
 * 
 * This class follows Single Responsibility Principle by focusing solely
 * on color space mathematics without any rendering or window dependencies.
 */
class HDRColorConversion
{
public:
  /**
   * @brief Build YUV->RGB color conversion matrix for HDR content
   * 
   * This function creates a 3x3 matrix that converts YUV to RGB using
   * the specified color primaries. The matrix only handles color space
   * conversion, NOT transfer function (PQ/HLG).
   * 
   * @param colorConversion Color conversion type (BT.709, BT.601, BT.2020)
   * @param matrix Output 4x4 matrix in column-major order (std140 mat4 layout)
   *               The 3x3 color matrix occupies columns 0-2, rows 0-2
   *               Column 3 and row 3 are set to identity/zero for padding
   * @param offsetVec Output offset vector (Y offset, UV offset, UV offset, padding)
   */
  static void buildColorMatrix(yuv::ColorConversion colorConversion,
                               float* matrix,
                               float* offsetVec);

  /**
   * @brief Get color conversion coefficients for a given color space
   * 
   * @param colorConversion Color conversion type
   * @return ColorCoefficients structure with pre-computed values
   */
  static ColorCoefficients getCoefficients(yuv::ColorConversion colorConversion);

  /**
   * @brief Check if color conversion uses full range
   * 
   * @param colorConversion Color conversion type
   * @return true if full range, false if limited range
   */
  static bool isFullRange(yuv::ColorConversion colorConversion);

  /**
   * @brief Get Y offset for limited range conversion
   * 
   * Limited range Y values start at 16 (8-bit) or 64 (10-bit).
   * This returns the normalized offset value.
   * 
   * @return Y offset normalized to [0,1] range
   */
  static float getYOffsetLimitedRange();

  /**
   * @brief Get chroma center offset
   * 
   * Chroma values are centered at 128 (8-bit) or 512 (10-bit).
   * This returns the normalized center value.
   * 
   * @return Chroma center offset normalized to [0,1] range
   */
  static float getChromaCenterOffset();

private:
  // Pre-computed coefficient table indexed by ColorConversion enum
  static const int kYuvRgbCoeffs[6][5];
};

} // namespace video::hdr

#endif // HDR_COLOR_CONVERSION_H
