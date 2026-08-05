/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "YUVPixelRenderer.h"
#include "YUVColorConverter.h"

namespace video::yuv
{

/**
 * @brief Determine if white text should be used based on luma value
 * 
 * Uses the midpoint of the bit depth range to determine if the background
 * is dark (requiring white text) or light (requiring black text).
 * 
 * @param yValue Luma sample value
 * @param bitsPerSample Bit depth of the format
 * @param invert Whether Y math inversion is enabled
 * @return true if white text should be used
 */
bool YUVPixelRenderer::shouldUseWhiteText(int yValue, int bitsPerSample, bool invert) const
{
  // Determine text color based on background brightness
  // Use white text on dark backgrounds, black text on light backgrounds
  const int midPoint = 1 << (bitsPerSample - 1);
  const bool isDark = invert ? (yValue > midPoint) : (yValue < midPoint);
  return isDark;
}

/**
 * @brief Format a YUV value as string (decimal or hexadecimal)
 * 
 * @param value The value to format
 * @return Formatted string representation
 */
QString YUVPixelRenderer::formatValue(int value) const
{
  if (m_showHexValues)
    return QString("0x%1").arg(value, 0, 16);
  else
    return QString::number(value);
}

/**
 * @brief Get YUV pixel value at a specific position from raw data
 * 
 * Handles planar, packed, and semi-planar YUV formats.
 * Returns Y, U, V component values for the given pixel position.
 * 
 * @param rawYUVData Raw YUV data buffer
 * @param format YUV pixel format specification
 * @param frameSize Frame dimensions
 * @param pixelPos Pixel position (x, y)
 * @return yuv_t structure with Y, U, V values
 */
yuv_t YUVPixelRenderer::getPixelValue(const QByteArray& rawYUVData,
                                      const PixelFormatYUV& format,
                                      const Size& frameSize,
                                      const QPoint& pixelPos)
{
  const int w = frameSize.width;
  const int h = frameSize.height;
  
  yuv_t value = {0, 0, 0};
  
  // Handle V210 predefined format
  if (auto predefinedFormat = format.getPredefinedFormat())
  {
    if (predefinedFormat == PredefinedPixelFormat::V210)
      return getPixelValueV210(rawYUVData, frameSize, pixelPos);
  }
  
  if (format.isPlanar())
  {
    // Calculate component sizes based on subsampling
    const int componentSizeLuma = w * h;
    const int componentSizeChroma = (w / format.getSubsamplingHor()) * 
                                    (h / format.getSubsamplingVer());
    
    // Calculate bytes per plane based on bit depth
    const int nrBytesLumaPlane = (format.getBitsPerSample() > 8) 
        ? componentSizeLuma * 2 : componentSizeLuma;
    const int nrBytesChromaPlane = (format.getBitsPerSample() > 8) 
        ? componentSizeChroma * 2 : componentSizeChroma;
    
    // Read luma value
    const unsigned char* srcY = reinterpret_cast<const unsigned char*>(rawYUVData.data());
    const unsigned int offsetY = w * pixelPos.y() + pixelPos.x();
    value.Y = YUVColorConverter::getValueFromSource(
        srcY, offsetY, format.getBitsPerSample(), format.isBigEndian());
    
    // Read chroma if not YUV_400 (luma-only)
    if (format.getSubsampling() != Subsampling::YUV_400)
    {
      const bool uFirst = (format.getPlaneOrder() == PlaneOrder::YUV || 
                           format.getPlaneOrder() == PlaneOrder::YUVA);
      const bool hasAlpha = (format.getPlaneOrder() == PlaneOrder::YUVA ||
                             format.getPlaneOrder() == PlaneOrder::YVUA);
      
      if (format.isUVInterleaved())
      {
        // Semi-planar: U and V are interleaved
        const unsigned char* srcUVA = srcY + nrBytesLumaPlane;
        const unsigned int mult = hasAlpha ? 3 : 2;
        const unsigned int offsetUV = 
            ((w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) +
             pixelPos.x() / format.getSubsamplingHor()) * mult;
        
        value.U = YUVColorConverter::getValueFromSource(
            srcUVA, offsetUV + (uFirst ? 0 : 1), 
            format.getBitsPerSample(), format.isBigEndian());
        value.V = YUVColorConverter::getValueFromSource(
            srcUVA, offsetUV + (uFirst ? 1 : 0), 
            format.getBitsPerSample(), format.isBigEndian());
      }
      else
      {
        // Fully planar: separate U and V planes
        const unsigned char* srcU = uFirst 
            ? srcY + nrBytesLumaPlane 
            : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
        const unsigned char* srcV = uFirst 
            ? srcY + nrBytesLumaPlane + nrBytesChromaPlane 
            : srcY + nrBytesLumaPlane;
        
        const unsigned int offsetUV = 
            (w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) +
            pixelPos.x() / format.getSubsamplingHor();
        
        value.U = YUVColorConverter::getValueFromSource(
            srcU, offsetUV, format.getBitsPerSample(), format.isBigEndian());
        value.V = YUVColorConverter::getValueFromSource(
            srcV, offsetUV, format.getBitsPerSample(), format.isBigEndian());
      }
    }
  }
  else
  {
    // Packed format handling
    const auto packing = format.getPackingOrder();
    
    if (format.getSubsampling() == Subsampling::YUV_422)
    {
      // Packed 4:2:2 - samples arranged in blocks of 4
      const int oY = (packing == PackingOrder::YUYV || packing == PackingOrder::YVYU) ? 0 : 1;
      const int oU = (packing == PackingOrder::UYVY) ? 0
                   : (packing == PackingOrder::YUYV) ? 1
                   : (packing == PackingOrder::VYUY) ? 2 : 3;
      const int oV = (packing == PackingOrder::VYUY) ? 0
                   : (packing == PackingOrder::YVYU) ? 1
                   : (packing == PackingOrder::UYVY) ? 2 : 3;
      
      const unsigned offsetCoordinate4Block = 
          (w * 2 * pixelPos.y() + (pixelPos.x() / 2 * 4)) *
          (format.getBitsPerSample() > 8 ? 2 : 1);
      const unsigned char* src = 
          reinterpret_cast<const unsigned char*>(rawYUVData.data()) + offsetCoordinate4Block;
      
      value.Y = YUVColorConverter::getValueFromSource(
          src, (pixelPos.x() % 2 == 0) ? oY : oY + 2,
          format.getBitsPerSample(), format.isBigEndian());
      value.U = YUVColorConverter::getValueFromSource(
          src, oU, format.getBitsPerSample(), format.isBigEndian());
      value.V = YUVColorConverter::getValueFromSource(
          src, oV, format.getBitsPerSample(), format.isBigEndian());
    }
    else if (format.getSubsampling() == Subsampling::YUV_444)
    {
      // Packed 4:4:4 - 3 or 4 bytes per pixel
      const int oY = (packing == PackingOrder::AYUV) ? 1 
                   : (packing == PackingOrder::VUYA) ? 2 : 0;
      const int oU = (packing == PackingOrder::YUV || packing == PackingOrder::YUVA ||
                      packing == PackingOrder::VUYA) ? 1 : 2;
      const int oV = (packing == PackingOrder::YVU) ? 1
                   : (packing == PackingOrder::AYUV) ? 3
                   : (packing == PackingOrder::VUYA) ? 0 : 2;
      
      const int offsetNext = 
          (packing == PackingOrder::YUV || packing == PackingOrder::YVU ? 3 : 4) *
          (format.getBitsPerSample() > 8 ? 2 : 1);
      const int offsetSrc = (w * pixelPos.y() + pixelPos.x()) * offsetNext;
      const unsigned char* src = 
          reinterpret_cast<const unsigned char*>(rawYUVData.data()) + offsetSrc;
      
      value.Y = YUVColorConverter::getValueFromSource(
          src, oY, format.getBitsPerSample(), format.isBigEndian());
      value.U = YUVColorConverter::getValueFromSource(
          src, oU, format.getBitsPerSample(), format.isBigEndian());
      value.V = YUVColorConverter::getValueFromSource(
          src, oV, format.getBitsPerSample(), format.isBigEndian());
    }
  }
  
  return value;
}

/**
 * @brief Get YUV pixel value from V210 packed format
 * 
 * V210 is a 10-bit 4:2:2 packed format where 6 pixels are stored in 16 bytes.
 * Each 32-bit word contains 3 10-bit samples with 2 bits unused.
 * 
 * @param sourceBuffer V210 packed data
 * @param frameSize Frame dimensions
 * @param pixelPos Pixel position
 * @return yuv_t structure with Y, U, V values
 */
yuv_t YUVPixelRenderer::getPixelValueV210(const QByteArray& sourceBuffer,
                                          const Size& frameSize,
                                          const QPoint& pixelPos)
{
  // V210 format: 6 pixels in 16 bytes (128 bits)
  // Width is rounded up to multiple of 48 for proper alignment
  auto widthRoundUp = (((frameSize.width + 48 - 1) / 48) * 48);
  auto strideIn = widthRoundUp / 6 * 16;
  
  auto startInBuffer = (static_cast<unsigned>(pixelPos.y()) * strideIn) + 
                       static_cast<unsigned>(pixelPos.x()) / 6 * 16;
  
  const unsigned char* src = reinterpret_cast<const unsigned char*>(sourceBuffer.data());
  
  yuv_t ret;
  auto xSub = static_cast<unsigned>(pixelPos.x()) % 6;
  
  // Extract Y value based on position within 6-pixel block
  if (xSub == 0)
    ret.Y = ((src[startInBuffer + 1] >> 2) & 0x3f) + ((src[startInBuffer + 2] & 0x0f) << 6);
  else if (xSub == 1)
    ret.Y = src[startInBuffer + 4] + ((src[startInBuffer + 4 + 1] & 0x03) << 8);
  else if (xSub == 2)
    ret.Y = (src[startInBuffer + 4 + 2] >> 4) + ((src[startInBuffer + 4 + 3] & 0x3f) << 4);
  else if (xSub == 3)
    ret.Y = ((src[startInBuffer + 8 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 8 + 2] & 0x0f) << 6);
  else if (xSub == 4)
    ret.Y = src[startInBuffer + 12] + ((src[startInBuffer + 12 + 1] & 0x03) << 8);
  else
    ret.Y = (src[startInBuffer + 12 + 2] >> 4) + ((src[startInBuffer + 12 + 3] & 0x3f) << 4);
  
  // Extract U and V values (shared by pairs of pixels)
  if (xSub == 0 || xSub == 1)
  {
    ret.U = src[startInBuffer] + ((src[startInBuffer + 1] & 0x03) << 8);
    ret.V = ((src[startInBuffer + 2] >> 4) & 0x0f) + ((src[startInBuffer + 3] & 0x3f) << 4);
  }
  else if (xSub == 2 || xSub == 3)
  {
    ret.U = ((src[startInBuffer + 4 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 4 + 2] & 0x0f) << 6);
    ret.V = src[startInBuffer + 8] + ((src[startInBuffer + 8 + 1] & 0x03) << 8);
  }
  else // xSub == 4 || xSub == 5
  {
    ret.U = ((src[startInBuffer + 8 + 2] >> 4) & 0x0f) + ((src[startInBuffer + 8 + 3] & 0x3f) << 4);
    ret.V = ((src[startInBuffer + 12 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 12 + 2] & 0x0f) << 6);
  }
  
  return ret;
}

// DESIGN NOTE: drawPixelValues and drawDifferencePixelValues are non-static methods that
// require integration with QPainter and rendering state. They remain in videoHandlerYUV.cpp
// because they need access to:
// - QPainter drawing context
// - Widget viewport calculations
// - Real-time zoom state
// - Raw data frame index validation
//
// The core pixel value extraction (getPixelValue, getPixelValueV210) is fully implemented
// here as static methods that can be called from anywhere.
//
// To use pixel value extraction:
//   yuv_t pixel = YUVPixelRenderer::getPixelValue(rawData, format, frameSize, QPoint(x, y));

} // namespace video::yuv
