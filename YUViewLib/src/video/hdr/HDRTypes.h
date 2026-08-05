/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDR_TYPES_H
#define HDR_TYPES_H

#include <cstdint>

namespace video::hdr
{

// ============================================================================
// HDR Render Mode Enumeration
// ============================================================================

/**
 * @brief HDR render mode selection
 * 
 * Mode_BT2020_PQ_10bit: HDR10 with PQ transfer function (SMPTE ST.2084)
 * Mode_BT2020_HLG_10bit: HDR with HLG transfer function, processed in shader
 * Mode_BT2020_Linear_16bit: Linear BT.2020 pass-through to scRGB (1.0 = 80 nits)
 */
enum class RenderMode
{
  PQ     = 1,  // HDR10 BT.2020 PQ 10-bit rendering (SMPTE ST.2084)
  HLG    = 2,  // HLG rendering with realtime shader math
  Linear = 3   // Linear pass-through to scRGB (1.0 = 80 nits, gamut only)
};

/**
 * @brief Tone mapping target brightness range for PQ content
 * 
 * The user selects a target max luminance via a slider (500-10000 nits).
 * The ITU-R BT.2390 EETF maps PQ's full 10000 nit range down to the target.
 * A value of 0 disables tone mapping (pass-through).
 */
namespace ToneMapping
{
  constexpr float MIN_TARGET_NITS  = 500.0f;
  constexpr float MAX_TARGET_NITS  = 10000.0f;
  constexpr float DEFAULT_TARGET_NITS = 1000.0f;
}

// ============================================================================
// PQ/HLG Constants (SMPTE ST.2084 / ARIB STD-B67)
// ============================================================================

namespace constants
{

// PQ EOTF/OETF constants (SMPTE ST.2084)
constexpr float PQ_M1 = 0.1593017578125f;      // 2610/16384
constexpr float PQ_M2 = 78.84375f;             // 2523/32 * 128
constexpr float PQ_C1 = 0.8359375f;            // 3424/4096 = c3 - c2 + 1
constexpr float PQ_C2 = 18.8515625f;           // 2413/128 * 32
constexpr float PQ_C3 = 18.6875f;              // 2392/128 * 32
constexpr float PQ_PEAK_LUMINANCE = 10000.0f;  // Reference peak luminance in nits

// HLG OETF constants (ARIB STD-B67)
constexpr float HLG_A = 0.17883277f;
constexpr float HLG_B = 0.28466892f;  // 1 - 4*HLG_A
constexpr float HLG_C = 0.55991073f;  // 0.5 - HLG_A * ln(4*HLG_A)

// Display parameters
constexpr float DISPLAY_MAX_NITS = 350.0f;   // Target display peak brightness
constexpr float DISPLAY_MIN_NITS = 0.0005f;  // Minimum black level

// R16 texture normalization
// C++ uploads 10-bit int (0-1023) to 16-bit texture (0-65535).
// Scale factor: 65535.0 / 1023.0 = 64.06158358
constexpr float R16_SCALE = 64.061584f;

// 10-bit YUV constants
constexpr float Y_OFFSET_LIMITED_RANGE = 64.0f / 65535.0f;   // 64 = 16 << 2 for 10-bit
constexpr float CHROMA_CENTER_OFFSET   = 512.0f / 65535.0f;  // 512 = 1 << 9 for 10-bit
constexpr float COMBINED_SCALE = 65535.0f / (1023.0f * 65536.0f);

} // namespace constants

// ============================================================================
// Color Conversion Coefficients
// ============================================================================

/**
 * @brief Pre-computed YUV->RGB conversion coefficients
 * 
 * Layout: [Y, cRV, cGU, cGV, cBU] scaled by 65536
 * These are integer coefficients for 16-bit precision computation.
 */
struct ColorCoefficients
{
  int coeffY;   // Y multiplier
  int coeffRV;  // V -> R multiplier
  int coeffGU;  // U -> G multiplier (negative)
  int coeffGV;  // V -> G multiplier (negative)
  int coeffBU;  // U -> B multiplier
  bool fullRange;
};

// Pre-computed coefficient tables for different color spaces
namespace color_coeffs
{

// BT.709 Limited Range: Kr=0.2126, Kb=0.0722
constexpr ColorCoefficients BT709_LIMITED = {76309, 117489, -13975, -34925, 138438, false};

// BT.709 Full Range
constexpr ColorCoefficients BT709_FULL = {65536, 103206, -12276, -30679, 121608, true};

// BT.601 Limited Range: Kr=0.299, Kb=0.114
constexpr ColorCoefficients BT601_LIMITED = {76309, 104597, -25675, -53279, 132201, false};

// BT.601 Full Range
constexpr ColorCoefficients BT601_FULL = {65536, 91881, -22553, -46802, 116129, true};

// BT.2020 Limited Range: Kr=0.2627, Kb=0.0593
constexpr ColorCoefficients BT2020_LIMITED = {76309, 110013, -12276, -42626, 140363, false};

// BT.2020 Full Range
constexpr ColorCoefficients BT2020_FULL = {65536, 96638, -10783, -37444, 123299, true};

} // namespace color_coeffs

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Clamp value to range [lo, hi]
 * @tparam T Value type
 * @param v Value to clamp
 * @param lo Lower bound
 * @param hi Upper bound
 * @return Clamped value
 */
template <typename T>
constexpr T clamp(T v, T lo, T hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace video::hdr

#endif // HDR_TYPES_H
