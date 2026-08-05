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
#include <map>

namespace video::yuv
{

/**
 * @brief Component display mode for YUV visualization
 * 
 * Controls which YUV components are displayed in the output image.
 * Used for debugging and analysis purposes.
 */
enum class ComponentDisplayMode
{
  DisplayAll,  ///< Display all components (Y'CbCr -> RGB)
  DisplayY,    ///< Display luma (Y) only as grayscale
  DisplayCb,   ///< Display Cb chroma only
  DisplayCr    ///< Display Cr chroma only
};

/**
 * @brief String mapping for ComponentDisplayMode enum
 */
constexpr EnumMapper<ComponentDisplayMode, 4> ComponentDisplayModeMapper = {
    std::make_pair(ComponentDisplayMode::DisplayAll, "Y'CbCr"),
    std::make_pair(ComponentDisplayMode::DisplayY, "Luma (Y) Only"),
    std::make_pair(ComponentDisplayMode::DisplayCb, "Cb only"),
    std::make_pair(ComponentDisplayMode::DisplayCr, "Cr only")};

/**
 * @brief Camera Bayer mosaic layout used by ISP false-color preview.
 *
 * Each enum value names the 2x2 color order starting at the top-left sample.
 * With Quad / Nine-in-one cell sizes, each mosaic entry spans a NxN block.
 */
enum class BayerPattern
{
  RGGB, ///< R G / G B
  BGGR, ///< B G / G R
  GRBG, ///< G R / B G
  GBRG  ///< G B / R G
};

/**
 * @brief String mapping for BayerPattern enum (UI / playlist persistence).
 */
constexpr EnumMapper<BayerPattern, 4> BayerPatternMapper = {
    std::make_pair(BayerPattern::RGGB, "RGGB"),
    std::make_pair(BayerPattern::BGGR, "BGGR"),
    std::make_pair(BayerPattern::GRBG, "GRBG"),
    std::make_pair(BayerPattern::GBRG, "GBRG")};

/**
 * @brief Same-color cell size for Bayer / Quad Bayer / Nonacell sensors.
 *
 * Single uses a classic 1x1 mosaic cell. Quad paints each Bayer color as a
 * 2x2 block. Nine-in-one paints each Bayer color as a 3x3 block.
 */
enum class ISPCellSize
{
  Single,    ///< Classic Bayer: one sample per mosaic color.
  Quad,      ///< Quad Bayer / Tetracell: 2x2 samples share one color.
  NineInOne  ///< Nonacell: 3x3 samples share one color.
};

/**
 * @brief String mapping for ISPCellSize enum (UI / playlist persistence).
 */
constexpr EnumMapper<ISPCellSize, 3> ISPCellSizeMapper = {
    std::make_pair(ISPCellSize::Single, "Single"),
    std::make_pair(ISPCellSize::Quad, "Quad"),
    std::make_pair(ISPCellSize::NineInOne, "Nine-in-one")};

/**
 * @brief Return the edge length in samples for an ISP mosaic cell size.
 * @param cellSize Selected ISP cell size mode.
 * @return 1 for Single, 2 for Quad, 3 for Nine-in-one.
 */
constexpr int ispCellEdgeLength(const ISPCellSize cellSize)
{
  switch (cellSize)
  {
  case ISPCellSize::Quad:
    return 2;
  case ISPCellSize::NineInOne:
    return 3;
  case ISPCellSize::Single:
  default:
    return 1;
  }
}

/**
 * @brief Settings for YUV to RGB color conversion
 * 
 * This structure encapsulates all parameters needed for converting YUV data to RGB.
 * It includes chroma interpolation method, component display mode, color space,
 * and per-component math transformations.
 */
struct ConversionSettings
{
  /**
   * @brief Chroma upsampling interpolation method
   * 
   * Used when converting subsampled formats (4:2:0, 4:2:2) to full resolution.
   */
  ChromaInterpolation chromaInterpolation{ChromaInterpolation::NearestNeighbor};
  
  /**
   * @brief Which YUV components to display
   * 
   * Allows viewing individual Y, Cb, or Cr components for analysis.
   */
  ComponentDisplayMode componentDisplayMode{ComponentDisplayMode::DisplayAll};
  
  /**
   * @brief Color space conversion matrix
   * 
   * Specifies which YUV->RGB matrix coefficients to use (BT.601, BT.709, BT.2020).
   * SDR defaults to BT.709 Limited Range; videoHandlerYUV switches to BT.2020 Limited when HDR is on.
   */
  ColorConversion colorConversion{ColorConversion::BT709_LimitedRange};
  
  /**
   * @brief Math transformation parameters for luma and chroma
   * 
   * Allows scaling, offset, and inversion of Y and UV values before conversion.
   * Key: Component::Luma or Component::Chroma
   */
  std::map<Component, MathParameters> mathParameters;

  /**
   * @brief Enable ISP Bayer false-color preview for YUV400 sources.
   *
   * When true and the source is YUV400, each luma sample is painted as a
   * primary color according to bayerPattern and ispCellSize instead of gray.
   */
  bool ispModeEnabled{false};

  /**
   * @brief Bayer mosaic layout used while ISP mode is enabled.
   */
  BayerPattern bayerPattern{BayerPattern::RGGB};

  /**
   * @brief Mosaic cell size (single / quad / nine-in-one) for ISP preview.
   */
  ISPCellSize ispCellSize{ISPCellSize::Single};
};

/**
 * @brief YUV pixel value triplet
 * 
 * Holds the Y, U (Cb), and V (Cr) values for a single pixel.
 * Values are stored as unsigned integers in the native bit depth.
 */
struct yuv_t
{
  unsigned int Y{0};  ///< Luma component
  unsigned int U{0};  ///< Chroma blue-difference (Cb)
  unsigned int V{0};  ///< Chroma red-difference (Cr)
};

} // namespace video::yuv
