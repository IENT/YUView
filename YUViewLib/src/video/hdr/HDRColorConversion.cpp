/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "HDRColorConversion.h"

namespace video::hdr
{

// Pre-computed color conversion coefficients table (compile-time constants)
// Layout: [Y, cRV, cGU, cGV, cBU] for each color space
// Index mapping: 0=BT709_Limited, 1=BT709_Full, 2=BT601_Limited, 
//                3=BT601_Full, 4=BT2020_Limited, 5=BT2020_Full
const int HDRColorConversion::kYuvRgbCoeffs[6][5] = {
  {76309, 117489, -13975, -34925, 138438},  // BT709_LimitedRange
  {65536, 103206, -12276, -30679, 121608},  // BT709_FullRange
  {76309, 104597, -25675, -53279, 132201},  // BT601_LimitedRange
  {65536, 91881, -22553, -46802, 116129},   // BT601_FullRange
  {76309, 110013, -12276, -42626, 140363},  // BT2020_LimitedRange
  {65536, 96638, -10783, -37444, 123299}    // BT2020_FullRange
};

void HDRColorConversion::buildColorMatrix(yuv::ColorConversion colorConversion,
                                          float* matrix,
                                          float* offsetVec)
{
  // Branchless index calculation using enum ordering
  // ColorConversion enum order: BT709_Limited(0), BT709_Full(1), BT601_Limited(2), 
  //                             BT601_Full(3), BT2020_Limited(4), BT2020_Full(5)
  const int idx = static_cast<int>(colorConversion);

  // Load coefficients with bounds check elimination (compiler can prove idx is in range)
  const int* coeffs = kYuvRgbCoeffs[idx];

  // Convert integer coefficients to float using pre-computed combined scale
  // This folds the division by 65536 and multiplication by inputScale into one operation
  const float coeffY  = static_cast<float>(coeffs[0]) * constants::COMBINED_SCALE;
  const float coeffRV = static_cast<float>(coeffs[1]) * constants::COMBINED_SCALE;
  const float coeffGU = static_cast<float>(coeffs[2]) * constants::COMBINED_SCALE;
  const float coeffGV = static_cast<float>(coeffs[3]) * constants::COMBINED_SCALE;
  const float coeffBU = static_cast<float>(coeffs[4]) * constants::COMBINED_SCALE;

  // Build 4x4 matrix in column-major order for std140 mat4 layout
  // Using direct assignment is faster than memset + individual writes
  
  // Column 0: Y coefficients
  matrix[0]  = coeffY;   // Y -> R
  matrix[1]  = coeffY;   // Y -> G
  matrix[2]  = coeffY;   // Y -> B
  matrix[3]  = 0.0f;     // row 3, unused

  // Column 1: U coefficients
  matrix[4]  = 0.0f;     // U -> R (no contribution)
  matrix[5]  = coeffGU;  // U -> G
  matrix[6]  = coeffBU;  // U -> B
  matrix[7]  = 0.0f;     // row 3, unused

  // Column 2: V coefficients
  matrix[8]  = coeffRV;  // V -> R
  matrix[9]  = coeffGV;  // V -> G
  matrix[10] = 0.0f;     // V -> B (no contribution)
  matrix[11] = 0.0f;     // row 3, unused

  // Column 3: padding (identity element at [3][3])
  matrix[12] = 0.0f;
  matrix[13] = 0.0f;
  matrix[14] = 0.0f;
  matrix[15] = 1.0f;     // identity element

  // Branchless full range detection using bitwise AND
  // Full range conversions have odd enum values (1, 3, 5)
  const bool fullRange = (idx & 1) != 0;

  // Branchless offset assignment using conditional move pattern
  // Compiler will optimize this to cmov instruction on modern CPUs
  const float yOffset = fullRange ? 0.0f : constants::Y_OFFSET_LIMITED_RANGE;

  offsetVec[0] = yOffset;
  offsetVec[1] = constants::CHROMA_CENTER_OFFSET;
  offsetVec[2] = constants::CHROMA_CENTER_OFFSET;
  offsetVec[3] = 0.0f;  // padding
}

ColorCoefficients HDRColorConversion::getCoefficients(yuv::ColorConversion colorConversion)
{
  switch (colorConversion)
  {
  case yuv::ColorConversion::BT709_LimitedRange:
    return color_coeffs::BT709_LIMITED;
  case yuv::ColorConversion::BT709_FullRange:
    return color_coeffs::BT709_FULL;
  case yuv::ColorConversion::BT601_LimitedRange:
    return color_coeffs::BT601_LIMITED;
  case yuv::ColorConversion::BT601_FullRange:
    return color_coeffs::BT601_FULL;
  case yuv::ColorConversion::BT2020_LimitedRange:
    return color_coeffs::BT2020_LIMITED;
  case yuv::ColorConversion::BT2020_FullRange:
    return color_coeffs::BT2020_FULL;
  default:
    return color_coeffs::BT2020_LIMITED;  // Default to BT.2020 for HDR
  }
}

bool HDRColorConversion::isFullRange(yuv::ColorConversion colorConversion)
{
  // Full range conversions have odd enum values
  return (static_cast<int>(colorConversion) & 1) != 0;
}

float HDRColorConversion::getYOffsetLimitedRange()
{
  return constants::Y_OFFSET_LIMITED_RANGE;
}

float HDRColorConversion::getChromaCenterOffset()
{
  return constants::CHROMA_CENTER_OFFSET;
}

} // namespace video::hdr
