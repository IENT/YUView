/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#ifndef HDR_YUV_REPACK_H
#define HDR_YUV_REPACK_H

#include <cstdint>

namespace video::hdr
{

/**
 * @brief Plane-repack helpers for HDR GPU texture upload (CPU dynamic dispatch).
 *
 * Converts host YUV storage layouts into the R16 planar layout expected by
 * HDR_RhiVideoWindow (host-endian 16-bit words, 10-bit values in the low bits).
 *
 * Covered transforms:
 * - Big-endian planar: per-sample byte swap
 * - P010 / semi-planar 10-bit: MSB-aligned samples >>6, plus UV deinterleave
 *
 * On x86, AVX2 / SSE2 / scalar kernels are selected once via CpuFeatures;
 * non-x86 builds use scalar. Global project flags stay at a portable baseline
 * (no -mavx2); AVX2 is compiled only in a dedicated translation unit.
 */
class HDRYUVRepack
{
public:
  /**
   * @brief Convert a contiguous 16-bit plane (Y, or planar U/V).
   * @param src Source samples (may be big-endian in memory).
   * @param dst Destination host-endian samples (low 10-bit when normalizeP010).
   * @param sampleCount Number of 16-bit samples.
   * @param bigEndian Whether each sample is stored big-endian.
   * @param normalizeP010 If true, shift right by 6 (P010 MSB-aligned).
   */
  static void convertPlane16(const uint16_t* src,
                             uint16_t* dst,
                             int sampleCount,
                             bool bigEndian,
                             bool normalizeP010);

  /**
   * @brief Deinterleave UVUV... into separate U/V planes with optional BE/P010 fixups.
   * @param srcInterleaved Source UV plane (2 * uvSampleCount samples).
   * @param dstU Destination U plane (uvSampleCount samples).
   * @param dstV Destination V plane (uvSampleCount samples).
   * @param uvSampleCount Number of chroma sample pairs (U,V).
   * @param bigEndian Whether each 16-bit word is big-endian.
   * @param normalizeP010 Whether to >>6 after endian fixup.
   * @param uFirst true for UV order; false for VU order.
   */
  static void deinterleaveUV(const uint16_t* srcInterleaved,
                             uint16_t* dstU,
                             uint16_t* dstV,
                             int uvSampleCount,
                             bool bigEndian,
                             bool normalizeP010,
                             bool uFirst);
};

} // namespace video::hdr

#endif // HDR_YUV_REPACK_H
