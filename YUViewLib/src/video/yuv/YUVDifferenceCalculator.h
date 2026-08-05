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
#include <QList>

namespace video::yuv
{

/**
 * @brief Result structure for frame difference statistics
 */
struct FrameDifferenceResult
{
  double mseY{0.0};     ///< MSE for Y (luma) component
  double mseU{0.0};     ///< MSE for U (Cb) component
  double mseV{0.0};     ///< MSE for V (Cr) component
  double psnrY{0.0};    ///< PSNR for Y component in dB
  double psnrU{0.0};    ///< PSNR for U component in dB
  double psnrV{0.0};    ///< PSNR for V component in dB
  double ssim{0.0};     ///< SSIM (if calculated)
  int64_t diffCount{0}; ///< Number of differing pixels
  bool success{false};  ///< Whether calculation succeeded
};

/**
 * @brief YUV frame difference calculator
 * 
 * This class handles all frame difference calculations and visualizations,
 * extracted from videoHandlerYUV for Single Responsibility Principle compliance.
 * 
 * Key responsibilities:
 * - Calculate YUV frame differences
 * - Generate difference images
 * - Compute difference statistics (MSE, PSNR, SSIM)
 * - Mark differences in output images
 */
class YUVDifferenceCalculator
{
public:
  /**
   * @brief Calculate difference between two YUV frames
   * 
   * Computes the component-wise difference between two YUV frames.
   * The difference is stored as signed values centered at mid-gray.
   * 
   * @param frame1 First frame YUV data
   * @param frame2 Second frame YUV data
   * @param format1 First frame pixel format
   * @param format2 Second frame pixel format
   * @param size1 First frame size
   * @param size2 Second frame size
   * @param outputBuffer Output difference buffer (YUV format)
   * @param outputFormat Output format (will be set)
   * @param outputSize Output size (minimum of both)
   * @return true if calculation succeeded
   */
  static bool calculateYUVDifference(const QByteArray& frame1,
                                     const QByteArray& frame2,
                                     const PixelFormatYUV& format1,
                                     const PixelFormatYUV& format2,
                                     const Size& size1,
                                     const Size& size2,
                                     QByteArray& outputBuffer,
                                     PixelFormatYUV& outputFormat,
                                     Size& outputSize);

  /**
   * @brief Calculate difference and mark non-zero pixels in RGB output
   * 
   * Optimized combined difference calculation and RGB conversion.
   * Non-zero differences can optionally be marked with a highlight color.
   * 
   * @param frame1 First frame YUV data
   * @param frame2 Second frame YUV data
   * @param format1 First frame pixel format
   * @param format2 Second frame pixel format
   * @param size1 First frame size
   * @param size2 Second frame size
   * @param outputRGB Output RGB buffer (BGRA format)
   * @param outputSize Output image size
   * @param amplificationFactor Multiplier for difference visibility
   * @param markDifference If true, mark non-zero differences
   * @return true if calculation succeeded
   */
  static bool calculateDifferenceToRGB(const QByteArray& frame1,
                                       const QByteArray& frame2,
                                       const PixelFormatYUV& format1,
                                       const PixelFormatYUV& format2,
                                       const Size& size1,
                                       const Size& size2,
                                       unsigned char* outputRGB,
                                       Size& outputSize,
                                       int amplificationFactor = 1,
                                       bool markDifference = false);

  /**
   * @brief Calculate frame difference statistics
   * 
   * Computes MSE, PSNR, and optional SSIM metrics for two frames.
   * 
   * @param frame1 First frame YUV data
   * @param frame2 Second frame YUV data
   * @param format1 First frame pixel format
   * @param format2 Second frame pixel format
   * @param size1 First frame size
   * @param size2 Second frame size
   * @return FrameDifferenceResult with computed metrics
   */
  static FrameDifferenceResult calculateStatistics(const QByteArray& frame1,
                                                   const QByteArray& frame2,
                                                   const PixelFormatYUV& format1,
                                                   const PixelFormatYUV& format2,
                                                   const Size& size1,
                                                   const Size& size2);

private:
  /**
   * @brief Calculate MSE for a single component plane
   * 
   * @param plane1 First frame plane data
   * @param plane2 Second frame plane data
   * @param width Plane width
   * @param height Plane height
   * @param bps Bits per sample
   * @param bigEndian Byte order
   * @return MSE value
   */
  static double calculatePlaneMSE(const unsigned char* plane1,
                                  const unsigned char* plane2,
                                  int width,
                                  int height,
                                  int bps,
                                  bool bigEndian);

  /**
   * @brief Convert MSE to PSNR
   * 
   * @param mse Mean squared error
   * @param maxValue Maximum possible pixel value
   * @return PSNR in dB (infinity if MSE is 0)
   */
  static double mseToPSNR(double mse, int maxValue);
};

} // namespace video::yuv
