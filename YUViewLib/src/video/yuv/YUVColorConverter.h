/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include "PixelFormatYUV.h"
#include "YUVConversionTypes.h"
#include <QByteArray>
#include <QImage>

namespace video::yuv
{

/**
 * @brief YUV to RGB color conversion utility class
 * 
 * This class encapsulates all YUV->RGB color conversion algorithms,
 * separated from videoHandlerYUV to follow Single Responsibility Principle.
 * 
 * Key responsibilities:
 * - YUV planar to RGB conversion
 * - YUV packed to planar conversion
 * - Color space transformations (BT.601, BT.709, BT.2020)
 * - Chroma interpolation
 * - Component display modes (Y only, Cb only, etc.)
 * 
 * NOTE: This class is part of the SRP refactoring effort. The actual conversion
 * implementations remain in videoHandlerYUV.cpp for now, and will be migrated
 * incrementally to avoid breaking changes.
 */
class YUVColorConverter
{
public:
  /**
   * @brief Convert YUV data to QImage
   * 
   * Main entry point for YUV->RGB conversion. Handles both planar and packed formats.
   * 
   * @param sourceBuffer Raw YUV data buffer
   * @param outputImage Output QImage (will be resized if needed)
   * @param yuvFormat YUV pixel format specification
   * @param frameSize Frame dimensions
   * @param settings Color conversion settings
   * @return true if conversion succeeded, false otherwise
   */
  static bool convertYUVToImage(const QByteArray& sourceBuffer,
                                QImage& outputImage,
                                const PixelFormatYUV& yuvFormat,
                                const Size& frameSize,
                                const ConversionSettings& settings);

  /**
   * @brief Convert YUV planar data to RGB
   * 
   * Optimized conversion for planar YUV formats (most common case).
   * 
   * @param sourceBuffer Raw YUV planar data
   * @param targetBuffer Output RGB buffer (BGRA format)
   * @param frameSize Frame dimensions
   * @param format YUV format specification
   * @param settings Conversion settings
   * @return true if conversion succeeded
   */
  static bool convertYUVPlanarToRGB(const QByteArray& sourceBuffer,
                                    unsigned char* targetBuffer,
                                    const Size& frameSize,
                                    const PixelFormatYUV& format,
                                    const ConversionSettings& settings);

  /**
   * @brief Convert packed YUV to planar format
   * 
   * Converts packed formats (YUYV, UYVY, etc.) to planar for processing.
   * 
   * @param sourceBuffer Packed YUV data
   * @param targetBuffer Output planar buffer
   * @param frameSize Frame dimensions
   * @param format Packed format specification
   * @return Pair of (success, new planar format)
   */
  static std::pair<bool, PixelFormatYUV> convertYUVPackedToPlanar(
      const QByteArray& sourceBuffer,
      QByteArray& targetBuffer,
      const Size& frameSize,
      const PixelFormatYUV& format);

  /**
   * @brief Convert V210 packed format to planar
   * 
   * Specialized conversion for V210 10-bit packed format.
   * 
   * @param sourceBuffer V210 packed data
   * @param targetBuffer Output planar buffer
   * @param frameSize Frame dimensions
   * @return Pair of (success, planar format)
   */
  static std::pair<bool, PixelFormatYUV> convertV210PackedToPlanar(
      const QByteArray& sourceBuffer,
      QByteArray& targetBuffer,
      const Size& frameSize);

  /**
   * @brief Get color conversion coefficients for a specific color space
   * 
   * Returns the YUV->RGB matrix coefficients for the specified color conversion.
   * 
   * @param conversion Color conversion type
   * @param coefficients Output array of 5 coefficients [Y, RV, GU, GV, BU]
   */
  static void getColorConversionCoefficients(ColorConversion conversion, int coefficients[5]);

  /**
   * @brief Check if color conversion uses full range
   * 
   * @param conversion Color conversion type
   * @return true if full range (0-255), false if limited range (16-235)
   */
  static bool isFullRange(ColorConversion conversion);

  /**
   * @brief Optimized YUV420 to RGB conversion
   * 
   * Specialized fast path for 8-bit and 10-bit YUV420 with nearest neighbor interpolation.
   * 
   * @tparam bitDepth 8 or 10 bit depth
   * @param sourceBuffer YUV420 data
   * @param targetBuffer Output RGB buffer
   * @param size Frame dimensions
   * @param format YUV format
   * @param settings Conversion settings
   * @return true if conversion succeeded
   */
  template <int bitDepth>
  static bool convertYUV420ToRGB(const QByteArray& sourceBuffer,
                                 unsigned char* targetBuffer,
                                 const Size& size,
                                 const PixelFormatYUV& format,
                                 const ConversionSettings& settings);

  /**
   * @brief Read a sample value from YUV source buffer
   * 
   * Handles both 8-bit and high bit depth formats with proper endianness.
   * This function is public to allow use by YUVPixelRenderer.
   * 
   * @param src Source data pointer
   * @param idx Sample index (not byte index)
   * @param bps Bits per sample
   * @param bigEndian True if big-endian byte order
   * @return Sample value
   */
  static int getValueFromSource(const unsigned char* src, int idx, int bps, bool bigEndian);

  static void convertYUVToRGB8Bit(unsigned int valY, unsigned int valU, unsigned int valV,
                                  int& valR, int& valG, int& valB,
                                  const int RGBConv[5], bool fullRange, int bps);

private:
  // Internal helper functions
  static void initClippingTable();
  static int clip8Bit(int val);
  static int transformYUV(bool invert, int scale, int offset, unsigned int value, int clipMax);
  
  // Clipping table for fast 8-bit clamping
  static unsigned char clp_buf[384 + 256 + 384];
  static bool clp_buf_initialized;
};

} // namespace video::yuv
