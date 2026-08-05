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
#include <QPainter>
#include <QByteArray>
#include <QRect>

namespace video::yuv
{

/**
 * @brief YUV pixel value renderer for zoom-in visualization
 * 
 * This class is responsible for drawing YUV pixel values on top of video frames
 * when zoomed in sufficiently. Extracted from videoHandlerYUV for SRP compliance.
 * 
 * Key responsibilities:
 * - Drawing Y, U, V values at pixel positions
 * - Handling chroma subsampling offset for proper positioning
 * - Supporting difference mode display
 * - Text color selection based on background luminance
 */
class YUVPixelRenderer
{
public:
  /**
   * @brief Constructor
   */
  YUVPixelRenderer() = default;

  /**
   * @brief Draw YUV pixel values on the painter
   * 
   * Draws the Y, U, V values of visible pixels when zoomed in.
   * Values are positioned correctly based on chroma subsampling.
   * 
   * @param painter QPainter to draw on
   * @param rawYUVData Raw YUV data buffer
   * @param format YUV pixel format
   * @param frameSize Frame dimensions
   * @param videoRect Video rectangle in painter coordinates
   * @param zoomFactor Current zoom level
   * @param settings Color conversion settings (for math parameters)
   * @param showAsDiff If true, display values as differences from mid-gray
   */
  void drawPixelValues(QPainter* painter,
                       const QByteArray& rawYUVData,
                       const PixelFormatYUV& format,
                       const Size& frameSize,
                       const QRect& videoRect,
                       double zoomFactor,
                       const ConversionSettings& settings,
                       bool showAsDiff = false);

  /**
   * @brief Draw YUV difference values between two frames
   * 
   * Compares two YUV frames and draws the difference values.
   * 
   * @param painter QPainter to draw on
   * @param rawYUVData1 First frame YUV data
   * @param rawYUVData2 Second frame YUV data
   * @param format1 First frame format
   * @param format2 Second frame format
   * @param frameSize1 First frame size
   * @param frameSize2 Second frame size
   * @param videoRect Video rectangle
   * @param zoomFactor Zoom level
   * @param settings Conversion settings
   * @param markDifference If true, highlight non-zero differences
   */
  void drawDifferencePixelValues(QPainter* painter,
                                 const QByteArray& rawYUVData1,
                                 const QByteArray& rawYUVData2,
                                 const PixelFormatYUV& format1,
                                 const PixelFormatYUV& format2,
                                 const Size& frameSize1,
                                 const Size& frameSize2,
                                 const QRect& videoRect,
                                 double zoomFactor,
                                 const ConversionSettings& settings,
                                 bool markDifference);

  /**
   * @brief Get YUV value at a specific pixel position
   * 
   * Reads Y, U, V values from raw data at the given pixel coordinates.
   * Handles different subsampling modes correctly.
   * 
   * @param rawYUVData Raw YUV data buffer
   * @param format YUV pixel format
   * @param frameSize Frame dimensions
   * @param pixelPos Pixel position (x, y)
   * @return yuv_t structure with Y, U, V values
   */
  static yuv_t getPixelValue(const QByteArray& rawYUVData,
                             const PixelFormatYUV& format,
                             const Size& frameSize,
                             const QPoint& pixelPos);

  /**
   * @brief Get YUV value from V210 packed format
   * 
   * Specialized pixel value extraction for V210 10-bit packed format.
   * 
   * @param sourceBuffer V210 packed data
   * @param frameSize Frame dimensions
   * @param pixelPos Pixel position
   * @return yuv_t structure with Y, U, V values
   */
  static yuv_t getPixelValueV210(const QByteArray& sourceBuffer,
                                 const Size& frameSize,
                                 const QPoint& pixelPos);

  /**
   * @brief Set whether to show values in hexadecimal
   * 
   * @param showHex true for hex display, false for decimal
   */
  void setShowHexValues(bool showHex) { m_showHexValues = showHex; }

  /**
   * @brief Get current hex display mode
   */
  bool getShowHexValues() const { return m_showHexValues; }

private:
  /**
   * @brief Determine if white text should be used based on Y value
   * 
   * @param yValue Luma value
   * @param bitsPerSample Bit depth
   * @param invert If Y math inversion is enabled
   * @return true if white text should be used
   */
  bool shouldUseWhiteText(int yValue, int bitsPerSample, bool invert) const;

  /**
   * @brief Format a YUV value as string
   * 
   * @param value The value to format
   * @return Formatted string (decimal or hex based on settings)
   */
  QString formatValue(int value) const;

  bool m_showHexValues{false};
};

} // namespace video::yuv
