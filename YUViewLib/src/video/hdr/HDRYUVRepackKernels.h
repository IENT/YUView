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

#include <cstdint>

namespace video::hdr::kernels
{

/**
 * @brief Scalar plane convert (portable fallback on all platforms).
 * @param src Source 16-bit samples.
 * @param dst Destination buffer.
 * @param sampleCount Sample count.
 * @param bigEndian Whether memory words are interpreted as big-endian.
 * @param normalizeP010 Whether to apply P010 >>6.
 */
void convertPlane16Scalar(const uint16_t *src,
                          uint16_t *dst,
                          int sampleCount,
                          bool bigEndian,
                          bool normalizeP010);

/**
 * @brief Scalar UV deinterleave (portable fallback on all platforms).
 * @param srcInterleaved Interleaved UV source.
 * @param dstU U-plane destination.
 * @param dstV V-plane destination.
 * @param uvSampleCount Number of chroma sample pairs.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 normalize flag.
 * @param uFirst true for UV order, false for VU order.
 */
void deinterleaveUVScalar(const uint16_t *srcInterleaved,
                          uint16_t *dstU,
                          uint16_t *dstV,
                          int uvSampleCount,
                          bool bigEndian,
                          bool normalizeP010,
                          bool uFirst);

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)

/**
 * @brief SSE2 plane convert (8 uint16 per iteration).
 * @param src Source samples.
 * @param dst Destination buffer.
 * @param sampleCount Sample count.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 normalize flag.
 */
void convertPlane16Sse2(const uint16_t *src,
                        uint16_t *dst,
                        int sampleCount,
                        bool bigEndian,
                        bool normalizeP010);

/**
 * @brief SSE2 UV deinterleave (8 pairs per iteration).
 * @param srcInterleaved Interleaved UV.
 * @param dstU U plane.
 * @param dstV V plane.
 * @param uvSampleCount Chroma pair count.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 normalize flag.
 * @param uFirst UV/VU order.
 */
void deinterleaveUVSse2(const uint16_t *srcInterleaved,
                        uint16_t *dstU,
                        uint16_t *dstV,
                        int uvSampleCount,
                        bool bigEndian,
                        bool normalizeP010,
                        bool uFirst);

#if defined(YUVIEW_HAS_AVX2_KERNELS) && YUVIEW_HAS_AVX2_KERNELS

/**
 * @brief AVX2 plane convert (16 uint16 per iteration; compile with /arch:AVX2 or -mavx2).
 * @param src Source samples.
 * @param dst Destination buffer.
 * @param sampleCount Sample count.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 normalize flag.
 */
void convertPlane16Avx2(const uint16_t *src,
                        uint16_t *dst,
                        int sampleCount,
                        bool bigEndian,
                        bool normalizeP010);

/**
 * @brief AVX2 UV deinterleave (16 pairs per iteration; compile under AVX2 ISA flags).
 * @param srcInterleaved Interleaved UV.
 * @param dstU U plane.
 * @param dstV V plane.
 * @param uvSampleCount Chroma pair count.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 normalize flag.
 * @param uFirst UV/VU order.
 */
void deinterleaveUVAvx2(const uint16_t *srcInterleaved,
                        uint16_t *dstU,
                        uint16_t *dstV,
                        int uvSampleCount,
                        bool bigEndian,
                        bool normalizeP010,
                        bool uFirst);

#endif // YUVIEW_HAS_AVX2_KERNELS

#endif // x86

} // namespace video::hdr::kernels
