/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "YUVDifferenceCalculator.h"
#include "YUVColorConverter.h"
#include <cmath>
#include <algorithm>

namespace video::yuv
{

namespace
{

/**
 * @brief Clip value to range [0, max]
 * @param value Value to clip
 * @param max Maximum allowed value
 * @return Clipped value
 */
inline int clipValue(int value, int max)
{
  if (value < 0) return 0;
  if (value > max) return max;
  return value;
}

/**
 * @brief Write a sample value to output buffer
 * @param dst Destination buffer pointer
 * @param value Value to write
 * @param bps Bits per sample (8-16)
 * @param bigEndian If true, write as big-endian
 */
inline void setValueInBuffer(unsigned char* dst, int value, int bps, bool bigEndian)
{
  if (bps > 8)
  {
    // 16-bit value
    if (bigEndian)
    {
      dst[0] = static_cast<unsigned char>((value >> 8) & 0xFF);
      dst[1] = static_cast<unsigned char>(value & 0xFF);
    }
    else
    {
      dst[0] = static_cast<unsigned char>(value & 0xFF);
      dst[1] = static_cast<unsigned char>((value >> 8) & 0xFF);
    }
  }
  else
  {
    dst[0] = static_cast<unsigned char>(value & 0xFF);
  }
}

} // anonymous namespace

/**
 * @brief Convert Mean Squared Error to Peak Signal-to-Noise Ratio
 */
double YUVDifferenceCalculator::mseToPSNR(double mse, int maxValue)
{
  if (mse <= 0.0)
    return std::numeric_limits<double>::infinity();
  
  return 10.0 * std::log10(static_cast<double>(maxValue * maxValue) / mse);
}

/**
 * @brief Calculate MSE for a single YUV plane
 */
double YUVDifferenceCalculator::calculatePlaneMSE(const unsigned char* plane1,
                                                  const unsigned char* plane2,
                                                  int width,
                                                  int height,
                                                  int bps,
                                                  bool bigEndian)
{
  double sumSquaredError = 0.0;
  const int totalPixels = width * height;
  
  if (totalPixels <= 0)
    return 0.0;
  
  if (bps <= 8)
  {
    for (int i = 0; i < totalPixels; ++i)
    {
      const int diff = static_cast<int>(plane1[i]) - static_cast<int>(plane2[i]);
      sumSquaredError += diff * diff;
    }
  }
  else
  {
    for (int i = 0; i < totalPixels; ++i)
    {
      int val1, val2;
      if (bigEndian)
      {
        val1 = (plane1[i*2] << 8) | plane1[i*2 + 1];
        val2 = (plane2[i*2] << 8) | plane2[i*2 + 1];
      }
      else
      {
        val1 = plane1[i*2] | (plane1[i*2 + 1] << 8);
        val2 = plane2[i*2] | (plane2[i*2 + 1] << 8);
      }
      const int diff = val1 - val2;
      sumSquaredError += diff * diff;
    }
  }
  
  return sumSquaredError / totalPixels;
}

/**
 * @brief Calculate YUV difference between two frames
 * 
 * Computes component-wise difference and stores result as planar YUV with
 * difference values centered at mid-gray (128 << (bps-8)).
 */
bool YUVDifferenceCalculator::calculateYUVDifference(
    const QByteArray& frame1,
    const QByteArray& frame2,
    const PixelFormatYUV& format1,
    const PixelFormatYUV& format2,
    const Size& size1,
    const Size& size2,
    QByteArray& outputBuffer,
    PixelFormatYUV& outputFormat,
    Size& outputSize)
{
  // Validate inputs
  if (frame1.isEmpty() || frame2.isEmpty())
    return false;
  if (!format1.isValid() || !format2.isValid())
    return false;
  
  // Only support same subsampling for direct YUV difference
  if (format1.getSubsampling() != format2.getSubsampling())
    return false;
  
  // Calculate output dimensions (minimum of both)
  const int w_out = std::min(size1.width, size2.width);
  const int h_out = std::min(size1.height, size2.height);
  outputSize = Size(w_out, h_out);
  
  if (w_out <= 0 || h_out <= 0)
    return false;
  
  // Get bit depths and calculate output bit depth
  const int bps1 = format1.getBitsPerSample();
  const int bps2 = format2.getBitsPerSample();
  const int bps_out = std::max(bps1, bps2);
  const int bitDepthScale1 = bps_out - bps1;
  const int bitDepthScale2 = bps_out - bps2;
  
  // Create output format (planar YUV, big-endian for consistency)
  outputFormat = PixelFormatYUV(format1.getSubsampling(), bps_out, PlaneOrder::YUV, true);
  
  // Get subsampling factors
  const int subH = format1.getSubsamplingHor();
  const int subV = format1.getSubsamplingVer();
  
  // Calculate buffer sizes
  const int bytesPerSample = (bps_out > 8) ? 2 : 1;
  const int lumaSize = w_out * h_out * bytesPerSample;
  const int chromaSize = (w_out / subH) * (h_out / subV) * bytesPerSample;
  const int totalSize = lumaSize + 2 * chromaSize;
  
  // Allocate output buffer
  outputBuffer.resize(totalSize);
  
  // Mid-gray value for difference representation
  const int diffZero = 128 << (bps_out - 8);
  const int maxVal = (1 << bps_out) - 1;
  
  // Get input plane pointers
  const int lumaBytes1 = size1.width * size1.height * ((bps1 > 8) ? 2 : 1);
  const int chromaBytes1 = (size1.width / subH) * (size1.height / subV) * ((bps1 > 8) ? 2 : 1);
  const int lumaBytes2 = size2.width * size2.height * ((bps2 > 8) ? 2 : 1);
  const int chromaBytes2 = (size2.width / subH) * (size2.height / subV) * ((bps2 > 8) ? 2 : 1);
  
  const unsigned char* srcY1 = reinterpret_cast<const unsigned char*>(frame1.constData());
  const unsigned char* srcU1 = srcY1 + lumaBytes1;
  const unsigned char* srcV1 = srcU1 + chromaBytes1;
  
  const unsigned char* srcY2 = reinterpret_cast<const unsigned char*>(frame2.constData());
  const unsigned char* srcU2 = srcY2 + lumaBytes2;
  const unsigned char* srcV2 = srcU2 + chromaBytes2;
  
  // Output pointers
  unsigned char* dstY = reinterpret_cast<unsigned char*>(outputBuffer.data());
  unsigned char* dstU = dstY + lumaSize;
  unsigned char* dstV = dstU + chromaSize;
  
  // Calculate strides
  const int strideY1 = size1.width * ((bps1 > 8) ? 2 : 1);
  const int strideY2 = size2.width * ((bps2 > 8) ? 2 : 1);
  const int strideC1 = (size1.width / subH) * ((bps1 > 8) ? 2 : 1);
  const int strideC2 = (size2.width / subH) * ((bps2 > 8) ? 2 : 1);
  
  // Process luma plane
  for (int y = 0; y < h_out; ++y)
  {
    for (int x = 0; x < w_out; ++x)
    {
      int val1 = YUVColorConverter::getValueFromSource(srcY1, x, bps1, format1.isBigEndian());
      int val2 = YUVColorConverter::getValueFromSource(srcY2, x, bps2, format2.isBigEndian());
      
      // Scale to output bit depth
      val1 <<= bitDepthScale1;
      val2 <<= bitDepthScale2;
      
      // Calculate difference and center at mid-gray
      int diff = clipValue(val1 - val2 + diffZero, maxVal);
      
      setValueInBuffer(dstY, diff, bps_out, true);
      dstY += bytesPerSample;
    }
    srcY1 += strideY1;
    srcY2 += strideY2;
  }
  
  // Process chroma planes if present
  if (format1.getSubsampling() != Subsampling::YUV_400)
  {
    const int chromaW = w_out / subH;
    const int chromaH = h_out / subV;
    
    for (int y = 0; y < chromaH; ++y)
    {
      for (int x = 0; x < chromaW; ++x)
      {
        int valU1 = YUVColorConverter::getValueFromSource(srcU1, x, bps1, format1.isBigEndian());
        int valU2 = YUVColorConverter::getValueFromSource(srcU2, x, bps2, format2.isBigEndian());
        int valV1 = YUVColorConverter::getValueFromSource(srcV1, x, bps1, format1.isBigEndian());
        int valV2 = YUVColorConverter::getValueFromSource(srcV2, x, bps2, format2.isBigEndian());
        
        // Scale
        valU1 <<= bitDepthScale1;
        valU2 <<= bitDepthScale2;
        valV1 <<= bitDepthScale1;
        valV2 <<= bitDepthScale2;
        
        // Calculate differences
        int diffU = clipValue(valU1 - valU2 + diffZero, maxVal);
        int diffV = clipValue(valV1 - valV2 + diffZero, maxVal);
        
        setValueInBuffer(dstU, diffU, bps_out, true);
        setValueInBuffer(dstV, diffV, bps_out, true);
        dstU += bytesPerSample;
        dstV += bytesPerSample;
      }
      srcU1 += strideC1;
      srcU2 += strideC2;
      srcV1 += strideC1;
      srcV2 += strideC2;
    }
  }
  
  return true;
}

/**
 * @brief Calculate difference and output directly to RGB buffer
 * 
 * Computes difference and converts to RGB in one pass, optionally marking
 * non-zero differences with a highlight color.
 */
bool YUVDifferenceCalculator::calculateDifferenceToRGB(
    const QByteArray& frame1,
    const QByteArray& frame2,
    const PixelFormatYUV& format1,
    const PixelFormatYUV& format2,
    const Size& size1,
    const Size& size2,
    unsigned char* outputRGB,
    Size& outputSize,
    int amplificationFactor,
    bool markDifference)
{
  // Validate inputs
  if (frame1.isEmpty() || frame2.isEmpty() || outputRGB == nullptr)
    return false;
  if (!format1.isValid() || !format2.isValid())
    return false;
  
  // Only support same subsampling
  if (format1.getSubsampling() != format2.getSubsampling())
    return false;
  
  // Calculate output dimensions
  const int w_out = std::min(size1.width, size2.width);
  const int h_out = std::min(size1.height, size2.height);
  outputSize = Size(w_out, h_out);
  
  if (w_out <= 0 || h_out <= 0)
    return false;
  
  // Get bit depths
  const int bps1 = format1.getBitsPerSample();
  const int bps2 = format2.getBitsPerSample();
  const int bps_out = std::max(bps1, bps2);
  const int bitDepthScale1 = bps_out - bps1;
  const int bitDepthScale2 = bps_out - bps2;
  
  // Mid-gray and max values
  const int diffZero = 128 << (bps_out - 8);
  const int maxVal = (1 << bps_out) - 1;
  
  // Get subsampling
  const int subH = format1.getSubsamplingHor();
  const int subV = format1.getSubsamplingVer();
  
  // Get input plane pointers  
  const int lumaBytes1 = size1.width * size1.height * ((bps1 > 8) ? 2 : 1);
  const int chromaBytes1 = (size1.width / subH) * (size1.height / subV) * ((bps1 > 8) ? 2 : 1);
  const int lumaBytes2 = size2.width * size2.height * ((bps2 > 8) ? 2 : 1);
  const int chromaBytes2 = (size2.width / subH) * (size2.height / subV) * ((bps2 > 8) ? 2 : 1);
  
  const unsigned char* srcY1 = reinterpret_cast<const unsigned char*>(frame1.constData());
  const unsigned char* srcU1 = srcY1 + lumaBytes1;
  const unsigned char* srcV1 = srcU1 + chromaBytes1;
  
  const unsigned char* srcY2 = reinterpret_cast<const unsigned char*>(frame2.constData());
  const unsigned char* srcU2 = srcY2 + lumaBytes2;
  const unsigned char* srcV2 = srcU2 + chromaBytes2;
  
  // Strides
  const int strideY1 = size1.width * ((bps1 > 8) ? 2 : 1);
  const int strideY2 = size2.width * ((bps2 > 8) ? 2 : 1);
  const int strideC1 = (size1.width / subH) * ((bps1 > 8) ? 2 : 1);
  const int strideC2 = (size2.width / subH) * ((bps2 > 8) ? 2 : 1);
  
  // BT.709 coefficients for YUV->RGB
  int RGBConv[5];
  YUVColorConverter::getColorConversionCoefficients(ColorConversion::BT709_LimitedRange, RGBConv);
  
  // Process each pixel
  unsigned char* dst = outputRGB;
  for (int y = 0; y < h_out; ++y)
  {
    for (int x = 0; x < w_out; ++x)
    {
      // Get Y difference
      int valY1 = YUVColorConverter::getValueFromSource(srcY1, x, bps1, format1.isBigEndian());
      int valY2 = YUVColorConverter::getValueFromSource(srcY2, x, bps2, format2.isBigEndian());
      valY1 <<= bitDepthScale1;
      valY2 <<= bitDepthScale2;
      int diffY = valY1 - valY2;
      
      // Get U/V differences (handle subsampling)
      int diffU = 0, diffV = 0;
      bool hasDiff = (diffY != 0);
      
      if (format1.getSubsampling() != Subsampling::YUV_400)
      {
        const int cx = x / subH;
        const int cy = y / subV;
        const int chromaOffset = cy * (size1.width / subH) + cx;
        
        int valU1 = YUVColorConverter::getValueFromSource(srcU1, cx, bps1, format1.isBigEndian());
        int valU2 = YUVColorConverter::getValueFromSource(srcU2, cx, bps2, format2.isBigEndian());
        int valV1 = YUVColorConverter::getValueFromSource(srcV1, cx, bps1, format1.isBigEndian());
        int valV2 = YUVColorConverter::getValueFromSource(srcV2, cx, bps2, format2.isBigEndian());
        
        valU1 <<= bitDepthScale1;
        valU2 <<= bitDepthScale2;
        valV1 <<= bitDepthScale1;
        valV2 <<= bitDepthScale2;
        
        diffU = valU1 - valU2;
        diffV = valV1 - valV2;
        
        hasDiff = hasDiff || (diffU != 0) || (diffV != 0);
      }
      
      // Apply amplification
      if (amplificationFactor != 1 && !markDifference)
      {
        diffY *= amplificationFactor;
        diffU *= amplificationFactor;
        diffV *= amplificationFactor;
      }
      
      // Convert to RGB
      int R, G, B;
      if (markDifference && hasDiff)
      {
        // Mark differences with red highlight
        R = 255;
        G = 0;
        B = 0;
      }
      else
      {
        // Convert difference YUV to RGB (difference centered at mid-gray)
        const int Y = clipValue(diffY + diffZero, maxVal);
        const int U = clipValue(diffU + diffZero, maxVal);
        const int V = clipValue(diffV + diffZero, maxVal);
        
        // Shift to 8-bit for display
        const int shift = bps_out - 8;
        const int Y8 = Y >> shift;
        const int U8 = U >> shift;
        const int V8 = V >> shift;
        
        // Simple YUV->RGB (treating mid-gray as neutral)
        R = clipValue(Y8 + ((V8 - 128) * 359 >> 8), 255);
        G = clipValue(Y8 - ((U8 - 128) * 88 >> 8) - ((V8 - 128) * 183 >> 8), 255);
        B = clipValue(Y8 + ((U8 - 128) * 454 >> 8), 255);
      }
      
      // Write BGRA
      dst[0] = static_cast<unsigned char>(B);
      dst[1] = static_cast<unsigned char>(G);
      dst[2] = static_cast<unsigned char>(R);
      dst[3] = 255;
      dst += 4;
    }
    
    srcY1 += strideY1;
    srcY2 += strideY2;
    
    // Move chroma pointers at appropriate intervals
    if ((y + 1) % subV == 0)
    {
      srcU1 += strideC1;
      srcV1 += strideC1;
      srcU2 += strideC2;
      srcV2 += strideC2;
    }
  }
  
  return true;
}

/**
 * @brief Calculate frame difference statistics
 */
FrameDifferenceResult YUVDifferenceCalculator::calculateStatistics(
    const QByteArray& frame1,
    const QByteArray& frame2,
    const PixelFormatYUV& format1,
    const PixelFormatYUV& format2,
    const Size& size1,
    const Size& size2)
{
  FrameDifferenceResult result;
  result.success = false;
  
  // Validate inputs
  if (frame1.isEmpty() || frame2.isEmpty())
    return result;
  if (!format1.isValid() || !format2.isValid())
    return result;
  
  // Only support same subsampling for now
  if (format1.getSubsampling() != format2.getSubsampling())
    return result;
  
  // Calculate output dimensions
  const int w_out = std::min(size1.width, size2.width);
  const int h_out = std::min(size1.height, size2.height);
  
  if (w_out <= 0 || h_out <= 0)
    return result;
  
  // Get bit depths
  const int bps1 = format1.getBitsPerSample();
  const int bps2 = format2.getBitsPerSample();
  const int bps_out = std::max(bps1, bps2);
  const int maxVal = (1 << bps_out) - 1;
  
  // Get subsampling
  const int subH = format1.getSubsamplingHor();
  const int subV = format1.getSubsamplingVer();
  
  // Get input plane pointers
  const int lumaBytes1 = size1.width * size1.height * ((bps1 > 8) ? 2 : 1);
  const int chromaBytes1 = (size1.width / subH) * (size1.height / subV) * ((bps1 > 8) ? 2 : 1);
  const int lumaBytes2 = size2.width * size2.height * ((bps2 > 8) ? 2 : 1);
  const int chromaBytes2 = (size2.width / subH) * (size2.height / subV) * ((bps2 > 8) ? 2 : 1);
  
  const unsigned char* srcY1 = reinterpret_cast<const unsigned char*>(frame1.constData());
  const unsigned char* srcU1 = srcY1 + lumaBytes1;
  const unsigned char* srcV1 = srcU1 + chromaBytes1;
  
  const unsigned char* srcY2 = reinterpret_cast<const unsigned char*>(frame2.constData());
  const unsigned char* srcU2 = srcY2 + lumaBytes2;
  const unsigned char* srcV2 = srcU2 + chromaBytes2;
  
  // Calculate Y plane statistics
  result.mseY = calculatePlaneMSE(srcY1, srcY2, w_out, h_out, bps1, format1.isBigEndian());
  result.psnrY = mseToPSNR(result.mseY, maxVal);
  
  // Calculate chroma statistics if present
  if (format1.getSubsampling() != Subsampling::YUV_400)
  {
    const int chromaW = w_out / subH;
    const int chromaH = h_out / subV;
    
    result.mseU = calculatePlaneMSE(srcU1, srcU2, chromaW, chromaH, bps1, format1.isBigEndian());
    result.mseV = calculatePlaneMSE(srcV1, srcV2, chromaW, chromaH, bps1, format1.isBigEndian());
    result.psnrU = mseToPSNR(result.mseU, maxVal);
    result.psnrV = mseToPSNR(result.mseV, maxVal);
  }
  
  // Count differing pixels (Y component only for simplicity)
  result.diffCount = 0;
  const int totalPixels = w_out * h_out;
  
  if (bps1 <= 8 && bps2 <= 8)
  {
    for (int i = 0; i < totalPixels; ++i)
    {
      if (srcY1[i] != srcY2[i])
        result.diffCount++;
    }
  }
  else
  {
    for (int i = 0; i < totalPixels; ++i)
    {
      int v1 = YUVColorConverter::getValueFromSource(srcY1, i, bps1, format1.isBigEndian());
      int v2 = YUVColorConverter::getValueFromSource(srcY2, i, bps2, format2.isBigEndian());
      if (v1 != v2)
        result.diffCount++;
    }
  }
  
  result.success = true;
  return result;
}

} // namespace video::yuv
