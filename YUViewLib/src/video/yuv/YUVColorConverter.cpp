/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "YUVColorConverter.h"

namespace video::yuv
{

// Static member initialization
unsigned char YUVColorConverter::clp_buf[384 + 256 + 384];
bool YUVColorConverter::clp_buf_initialized = false;

void YUVColorConverter::initClippingTable()
{
  if (clp_buf_initialized)
    return;
  
  // Initialize the clipping table for fast 8-bit clamping
  // Values below 0 clip to 0, values above 255 clip to 255
  for (int i = -384; i < 640; ++i)
  {
    if (i < 0)
      clp_buf[i + 384] = 0;
    else if (i > 255)
      clp_buf[i + 384] = 255;
    else
      clp_buf[i + 384] = static_cast<unsigned char>(i);
  }
  
  clp_buf_initialized = true;
}

int YUVColorConverter::clip8Bit(int val)
{
  if (!clp_buf_initialized)
    initClippingTable();
  
  // Use lookup table for fast clamping
  if (val < -384)
    return 0;
  if (val >= 640)
    return 255;
  return clp_buf[val + 384];
}

int YUVColorConverter::transformYUV(bool invert, int scale, int offset, unsigned int value, int clipMax)
{
  int newValue = static_cast<int>(value);
  
  // Apply scale (value * scale / 100)
  if (scale != 100)
    newValue = (newValue * scale) / 100;
  
  // Apply offset
  newValue += offset;
  
  // Apply inversion around midpoint
  if (invert)
    newValue = clipMax - newValue;
  
  // Clip to valid range
  if (newValue < 0)
    return 0;
  if (newValue > clipMax)
    return clipMax;
  
  return newValue;
}

void YUVColorConverter::getColorConversionCoefficients(ColorConversion conversion, int coefficients[5])
{
  // YUV to RGB conversion coefficients
  // coefficients[0] = Y factor
  // coefficients[1] = RV (V contribution to R)
  // coefficients[2] = GU (U contribution to G)
  // coefficients[3] = GV (V contribution to G)
  // coefficients[4] = BU (U contribution to B)
  
  switch (conversion)
  {
  case ColorConversion::BT709_LimitedRange:
  case ColorConversion::BT709_FullRange:
    // ITU-R BT.709 coefficients
    coefficients[0] = 76309;   // 1.164 * 65536
    coefficients[1] = 117489;  // 1.793 * 65536
    coefficients[2] = -13975;  // -0.213 * 65536
    coefficients[3] = -34925;  // -0.533 * 65536
    coefficients[4] = 138438;  // 2.112 * 65536
    break;
    
  case ColorConversion::BT601_LimitedRange:
  case ColorConversion::BT601_FullRange:
    // ITU-R BT.601 coefficients
    coefficients[0] = 76309;   // 1.164 * 65536
    coefficients[1] = 104597;  // 1.596 * 65536
    coefficients[2] = -25675;  // -0.392 * 65536
    coefficients[3] = -53279;  // -0.813 * 65536
    coefficients[4] = 132201;  // 2.017 * 65536
    break;
    
  case ColorConversion::BT2020_LimitedRange:
  case ColorConversion::BT2020_FullRange:
    // ITU-R BT.2020 coefficients
    coefficients[0] = 76309;   // 1.164 * 65536
    coefficients[1] = 110013;  // 1.679 * 65536
    coefficients[2] = -12276;  // -0.187 * 65536
    coefficients[3] = -42626;  // -0.650 * 65536
    coefficients[4] = 140363;  // 2.142 * 65536
    break;
    
  default:
    // Default to BT.709
    coefficients[0] = 76309;
    coefficients[1] = 117489;
    coefficients[2] = -13975;
    coefficients[3] = -34925;
    coefficients[4] = 138438;
    break;
  }
}

bool YUVColorConverter::isFullRange(ColorConversion conversion)
{
  switch (conversion)
  {
  case ColorConversion::BT709_FullRange:
  case ColorConversion::BT601_FullRange:
  case ColorConversion::BT2020_FullRange:
    return true;
  default:
    return false;
  }
}

void YUVColorConverter::convertYUVToRGB8Bit(unsigned int valY, unsigned int valU, unsigned int valV,
                                            int& valR, int& valG, int& valB,
                                            const int RGBConv[5], bool fullRange, int bps)
{
  // For high bit depths, right-shift to avoid overflow
  if (bps > 14)
  {
    const int yOffset = (fullRange ? 0 : 16 << (bps - 10));
    const int cZero = 128 << (bps - 10);
    
    const int Y_tmp = ((static_cast<int>(valY) >> 2) - yOffset) * RGBConv[0];
    const int U_tmp = (static_cast<int>(valU) >> 2) - cZero;
    const int V_tmp = (static_cast<int>(valV) >> 2) - cZero;
    
    const int R_tmp = (Y_tmp + V_tmp * RGBConv[1]) >> (16 + bps - 10);
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 10);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]) >> (16 + bps - 10);
    
    valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
  }
  else
  {
    const int yOffset = (fullRange ? 0 : 16 << (bps - 8));
    const int cZero = 128 << (bps - 8);
    
    const int Y_tmp = (static_cast<int>(valY) - yOffset) * RGBConv[0];
    const int U_tmp = static_cast<int>(valU) - cZero;
    const int V_tmp = static_cast<int>(valV) - cZero;
    
    const int R_tmp = (Y_tmp + V_tmp * RGBConv[1]) >> (16 + bps - 8);
    const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 8);
    const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]) >> (16 + bps - 8);
    
    valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
    valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
    valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
  }
}

int YUVColorConverter::getValueFromSource(const unsigned char* src, int idx, int bps, bool bigEndian)
{
  if (bps > 8)
  {
    // Read two bytes in the right order
    return bigEndian ? (src[idx * 2] << 8 | src[idx * 2 + 1])
                     : (src[idx * 2] | src[idx * 2 + 1] << 8);
  }
  else
  {
    return src[idx];
  }
}

// DESIGN NOTE: The high-performance conversion functions (convertYUVToImage, 
// convertYUVPlanarToRGB, convertYUV420ToRGB) remain in videoHandlerYUV.cpp because they:
// 1. Use heavily-templatized SIMD-optimized code in YUVConversionCore.h/YUVConversionRGB.h
// 2. Require platform-specific QImage format selection (ARGB32 vs RGB32)
// 3. Support advanced features: chroma interpolation modes, component display modes
//
// This class provides the fundamental building blocks:
// - getColorConversionCoefficients(): BT.601/709/2020 matrix coefficients
// - isFullRange(): Limited vs full range detection
// - convertYUVToRGB8Bit(): Single-pixel YUV->RGB conversion
// - getValueFromSource(): Safe multi-byte sample reading
// - clip8Bit(): Fast 8-bit clamping with lookup table
// - transformYUV(): Math operations (scale, offset, invert)
//
// ConversionSettings is now defined in YUVConversionTypes.h for use by all YUV modules.

} // namespace video::yuv

