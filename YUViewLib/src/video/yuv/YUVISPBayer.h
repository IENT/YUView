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

/**
 * @file YUVISPBayer.h
 * @brief ISP-mode Bayer false-color preview for YUV400 (luma-only) frames.
 *
 * Maps each mosaic sample along a black → primary (R/G/B) → white ramp based
 * on DN and the selected Bayer pattern / cell size. High DN approaches white;
 * low DN approaches black. A display gamma of 2.2 is applied by default.
 */

#include "YUVConversionCore.h"
#include "YUVConversionTypes.h"

#include <cmath>

namespace video::yuv::conversion
{

/**
 * @brief Display gamma applied in ISP Bayer preview (sRGB-like encoding).
 *
 * Linear sensor values are raised to (1/gamma) before 8-bit display so the
 * mosaic looks closer to what an ISP preview would show.
 */
constexpr double kISPPreviewGamma = 2.2;

/**
 * @brief Primary color channel assigned to one Bayer mosaic cell.
 */
enum class BayerChannel
{
  Red,   ///< Emit intensity on the red channel only.
  Green, ///< Emit intensity on the green channel only.
  Blue   ///< Emit intensity on the blue channel only.
};

/**
 * @brief Resolve the Bayer mosaic channel for a pixel coordinate.
 * @param x Horizontal pixel index.
 * @param y Vertical pixel index.
 * @param pattern Camera Bayer layout (RGGB / BGGR / GRBG / GBRG).
 * @param cellSize Edge length of one same-color cell (1, 2, or 3).
 * @return The primary channel that should visualize this sample.
 */
inline BayerChannel resolveBayerChannel(const int x,
                                        const int y,
                                        const BayerPattern pattern,
                                        const int cellSize)
{
  const int mosaicX = (x / cellSize) & 1;
  const int mosaicY = (y / cellSize) & 1;
  const int index   = (mosaicY << 1) | mosaicX;

  switch (pattern)
  {
  case BayerPattern::RGGB:
  {
    // R G / G B
    static constexpr BayerChannel kMap[4] = {
        BayerChannel::Red, BayerChannel::Green, BayerChannel::Green, BayerChannel::Blue};
    return kMap[index];
  }
  case BayerPattern::BGGR:
  {
    // B G / G R
    static constexpr BayerChannel kMap[4] = {
        BayerChannel::Blue, BayerChannel::Green, BayerChannel::Green, BayerChannel::Red};
    return kMap[index];
  }
  case BayerPattern::GRBG:
  {
    // G R / B G
    static constexpr BayerChannel kMap[4] = {
        BayerChannel::Green, BayerChannel::Red, BayerChannel::Blue, BayerChannel::Green};
    return kMap[index];
  }
  case BayerPattern::GBRG:
  {
    // G B / R G
    static constexpr BayerChannel kMap[4] = {
        BayerChannel::Green, BayerChannel::Blue, BayerChannel::Red, BayerChannel::Green};
    return kMap[index];
  }
  }
  return BayerChannel::Green;
}

/**
 * @brief Build (once) an 8-bit LUT for display gamma encoding.
 * @param gamma Display gamma (e.g. 2.2); LUT stores pow(i/255, 1/gamma)*255.
 * @return Pointer to a 256-entry lookup table.
 */
inline const unsigned char *getISPGammaLUT(const double gamma = kISPPreviewGamma)
{
  static unsigned char lut[256];
  static bool          initialized = false;
  static double        cachedGamma = 0.0;

  if (!initialized || cachedGamma != gamma)
  {
    const double invGamma = 1.0 / gamma;
    for (int i = 0; i < 256; ++i)
    {
      const double encoded = std::pow(static_cast<double>(i) / 255.0, invGamma);
      lut[i] = static_cast<unsigned char>(clip8Bit(static_cast<int>(encoded * 255.0 + 0.5)));
    }
    cachedGamma = gamma;
    initialized = true;
  }
  return lut;
}

/**
 * @brief Map DN intensity to BGRA along black → primary → white for one Bayer site.
 *
 * Low DN stays near black, mid DN shows the mosaic primary (R/G/B), and high DN
 * desaturates toward white so bright sensor values read as near-white.
 *
 * @param dst Destination BGRA buffer base pointer.
 * @param pixelIndex Linear pixel index into the frame.
 * @param value8 Normalized 8-bit intensity (already gamma-encoded).
 * @param channel Bayer channel that owns this mosaic site.
 */
inline void writeBayerFalseColorBGRA(unsigned char *dst,
                                     const int pixelIndex,
                                     const unsigned char value8,
                                     const BayerChannel channel)
{
  // Two-segment ramp on [0,255]:
  //   [0, 127]  : black → saturated primary
  //   [128,255] : saturated primary → white
  const int  v        = static_cast<int>(value8);
  const bool toPrimary = (v < 128);
  const int  segment   = toPrimary ? (v * 2) : ((v - 128) * 2); // 0..254(~255)

  unsigned char r = 0;
  unsigned char g = 0;
  unsigned char b = 0;

  switch (channel)
  {
  case BayerChannel::Red:
    if (toPrimary)
    {
      r = static_cast<unsigned char>(segment);
    }
    else
    {
      r = 255;
      g = static_cast<unsigned char>(segment);
      b = static_cast<unsigned char>(segment);
    }
    break;
  case BayerChannel::Green:
    if (toPrimary)
    {
      g = static_cast<unsigned char>(segment);
    }
    else
    {
      r = static_cast<unsigned char>(segment);
      g = 255;
      b = static_cast<unsigned char>(segment);
    }
    break;
  case BayerChannel::Blue:
    if (toPrimary)
    {
      b = static_cast<unsigned char>(segment);
    }
    else
    {
      r = static_cast<unsigned char>(segment);
      g = static_cast<unsigned char>(segment);
      b = 255;
    }
    break;
  }

  const int offset = pixelIndex * 4;
  dst[offset + 0]  = b;
  dst[offset + 1]  = g;
  dst[offset + 2]  = r;
  dst[offset + 3]  = 255;
}

/**
 * @brief Convert a YUV400 luma plane to Bayer false-color BGRA with gamma 2.2.
 *
 * Each mosaic site is colored by its Bayer channel using a black→primary→white
 * ramp driven by DN (after gamma). Quad / nine-in-one enlarge each mosaic cell
 * to 2x2 / 3x3 identical colors.
 *
 * @param width Frame width in pixels.
 * @param height Frame height in pixels.
 * @param math Optional luma scale/offset/invert transform.
 * @param src Source luma plane.
 * @param dst Destination BGRA buffer (width * height * 4 bytes).
 * @param inMax Maximum legal sample value for the bit depth ((1<<bps)-1).
 * @param bps Bits per sample (8/10/12/16 typical for ISP preview).
 * @param bigEndian Endianness of >8-bit samples.
 * @param pattern Selected Bayer layout.
 * @param cellSize Selected mosaic cell size (Single/Quad/NineInOne).
 */
inline void YUVPlaneToRGBBayerFalseColor(const int width,
                                         const int height,
                                         const MathParameters math,
                                         const unsigned char *src,
                                         unsigned char *dst,
                                         const int inMax,
                                         const int bps,
                                         const bool bigEndian,
                                         const BayerPattern pattern,
                                         const ISPCellSize cellSize)
{
  const bool           applyMath   = math.mathRequired();
  const int            shiftTo8Bit = bps - 8;
  const int            cellEdge    = ispCellEdgeLength(cellSize);
  const unsigned char *gammaLUT    = getISPGammaLUT(kISPPreviewGamma);

  for (int y = 0; y < height; ++y)
  {
    const int rowOffset = y * width;
    for (int x = 0; x < width; ++x)
    {
      const int pixelIndex = rowOffset + x;
      int       newVal     = getValueFromSource(src, pixelIndex, bps, bigEndian);
      if (applyMath)
        newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

      // Raw Bayer is full-range; map to 8-bit then apply display gamma 2.2.
      if (shiftTo8Bit > 0)
        newVal = clip8Bit(newVal >> shiftTo8Bit);
      else
        newVal = clip8Bit(newVal);

      const unsigned char gammaVal = gammaLUT[newVal];
      const BayerChannel  channel  = resolveBayerChannel(x, y, pattern, cellEdge);
      writeBayerFalseColorBGRA(dst, pixelIndex, gammaVal, channel);
    }
  }
}

} // namespace video::yuv::conversion
