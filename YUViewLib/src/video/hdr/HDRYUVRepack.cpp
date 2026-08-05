/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "HDRYUVRepack.h"

#include "HDRYUVRepackKernels.h"
#include "common/CpuFeatures.h"

#include <cstring>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define HDR_YUV_REPACK_X86 1
#include <emmintrin.h>
#else
#define HDR_YUV_REPACK_X86 0
#endif

#ifndef YUVIEW_HAS_AVX2_KERNELS
#define YUVIEW_HAS_AVX2_KERNELS 0
#endif

namespace video::hdr
{
namespace
{

/**
 * @brief Optional big-endian swap and P010 right-shift for one 16-bit sample.
 * @param sample Raw word as read from memory on a little-endian host.
 * @param bigEndian Whether disk/buffer layout is big-endian.
 * @param normalizeP010 Whether to >>6 after endian normalization.
 * @return Host-endian low-10-bit sample for the HDR shader path.
 */
inline uint16_t convertSampleScalar(uint16_t sample, bool bigEndian, bool normalizeP010)
{
  if (bigEndian)
  {
    sample = static_cast<uint16_t>((sample << 8) | (sample >> 8));
  }
  if (normalizeP010)
  {
    sample = static_cast<uint16_t>(sample >> 6);
  }
  return sample;
}

#if HDR_YUV_REPACK_X86

/**
 * @brief Byte-swap each 16-bit lane in an SSE2 register.
 * @param v Packed uint16 lanes.
 * @return Vector with high/low bytes swapped per lane.
 */
inline __m128i byteswapEpi16(__m128i v)
{
  return _mm_or_si128(_mm_slli_epi16(v, 8), _mm_srli_epi16(v, 8));
}

/**
 * @brief Apply optional BE swap and P010 >>6 to eight packed samples.
 * @param loaded Eight uint16 values.
 * @param bigEndian If true, byte-swap each lane.
 * @param normalizeP010 If true, >>6.
 * @return Transformed lanes.
 */
inline __m128i fixupEightSamples(__m128i loaded, bool bigEndian, bool normalizeP010)
{
  if (bigEndian)
  {
    loaded = byteswapEpi16(loaded);
  }
  if (normalizeP010)
  {
    loaded = _mm_srli_epi16(loaded, 6);
  }
  return loaded;
}

/**
 * @brief Deinterleave eight UV pairs from two 128-bit loads.
 *
 * Per-register input layout: [U0 V0 U1 V1 U2 V2 U3 V3].
 * Uses SSE2 shuffles only (no SSE4.1 packus) so full 16-bit P010 words
 * are preserved before an optional >>6.
 *
 * @param a First four UV pairs.
 * @param b Next four UV pairs.
 * @param outFirst First component plane of each pair (U when uFirst).
 * @param outSecond Second component plane of each pair (V when uFirst).
 */
inline void deinterleaveEightPairs(__m128i a, __m128i b, __m128i *outFirst, __m128i *outSecond)
{
  constexpr int kGatherUV = _MM_SHUFFLE(3, 1, 2, 0);
  a = _mm_shufflehi_epi16(_mm_shufflelo_epi16(a, kGatherUV), kGatherUV);
  b = _mm_shufflehi_epi16(_mm_shufflelo_epi16(b, kGatherUV), kGatherUV);
  a = _mm_shuffle_epi32(a, _MM_SHUFFLE(3, 1, 2, 0));
  b = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 1, 2, 0));
  *outFirst  = _mm_unpacklo_epi64(a, b);
  *outSecond = _mm_unpackhi_epi64(a, b);
}

#endif // HDR_YUV_REPACK_X86

using ConvertPlaneFn = void (*)(const uint16_t *, uint16_t *, int, bool, bool);
using DeinterleaveFn =
    void (*)(const uint16_t *, uint16_t *, uint16_t *, int, bool, bool, bool);

/**
 * @brief Select plane-convert implementation via CpuFeatures (resolved once per process).
 * @return Function pointer to scalar / SSE2 / AVX2 kernel.
 */
ConvertPlaneFn resolveConvertPlane16()
{
#if HDR_YUV_REPACK_X86 && YUVIEW_HAS_AVX2_KERNELS
  if (common::CpuFeatures::instance().hasAvx2())
  {
    return &kernels::convertPlane16Avx2;
  }
#endif
#if HDR_YUV_REPACK_X86
  if (common::CpuFeatures::instance().hasSse2())
  {
    return &kernels::convertPlane16Sse2;
  }
#endif
  return &kernels::convertPlane16Scalar;
}

/**
 * @brief Select UV deinterleave implementation via CpuFeatures (resolved once per process).
 * @return Function pointer to scalar / SSE2 / AVX2 kernel.
 */
DeinterleaveFn resolveDeinterleaveUV()
{
#if HDR_YUV_REPACK_X86 && YUVIEW_HAS_AVX2_KERNELS
  if (common::CpuFeatures::instance().hasAvx2())
  {
    return &kernels::deinterleaveUVAvx2;
  }
#endif
#if HDR_YUV_REPACK_X86
  if (common::CpuFeatures::instance().hasSse2())
  {
    return &kernels::deinterleaveUVSse2;
  }
#endif
  return &kernels::deinterleaveUVScalar;
}

} // namespace

namespace kernels
{

void convertPlane16Scalar(const uint16_t *src,
                          uint16_t *dst,
                          int sampleCount,
                          bool bigEndian,
                          bool normalizeP010)
{
  if (!src || !dst || sampleCount <= 0)
  {
    return;
  }

  if (!bigEndian && !normalizeP010)
  {
    if (src != dst)
    {
      std::memcpy(dst, src, static_cast<size_t>(sampleCount) * sizeof(uint16_t));
    }
    return;
  }

  for (int index = 0; index < sampleCount; ++index)
  {
    dst[index] = convertSampleScalar(src[index], bigEndian, normalizeP010);
  }
}

void deinterleaveUVScalar(const uint16_t *srcInterleaved,
                          uint16_t *dstU,
                          uint16_t *dstV,
                          int uvSampleCount,
                          bool bigEndian,
                          bool normalizeP010,
                          bool uFirst)
{
  if (!srcInterleaved || !dstU || !dstV || uvSampleCount <= 0)
  {
    return;
  }

  uint16_t *firstPlane  = uFirst ? dstU : dstV;
  uint16_t *secondPlane = uFirst ? dstV : dstU;

  for (int index = 0; index < uvSampleCount; ++index)
  {
    const uint16_t s0 =
        convertSampleScalar(srcInterleaved[index * 2], bigEndian, normalizeP010);
    const uint16_t s1 =
        convertSampleScalar(srcInterleaved[index * 2 + 1], bigEndian, normalizeP010);
    firstPlane[index]  = s0;
    secondPlane[index] = s1;
  }
}

#if HDR_YUV_REPACK_X86

void convertPlane16Sse2(const uint16_t *src,
                        uint16_t *dst,
                        int sampleCount,
                        bool bigEndian,
                        bool normalizeP010)
{
  if (!src || !dst || sampleCount <= 0)
  {
    return;
  }

  if (!bigEndian && !normalizeP010)
  {
    if (src != dst)
    {
      std::memcpy(dst, src, static_cast<size_t>(sampleCount) * sizeof(uint16_t));
    }
    return;
  }

  int index = 0;
  for (; index + 8 <= sampleCount; index += 8)
  {
    const __m128i loaded = _mm_loadu_si128(reinterpret_cast<const __m128i *>(src + index));
    const __m128i fixed  = fixupEightSamples(loaded, bigEndian, normalizeP010);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + index), fixed);
  }
  for (; index < sampleCount; ++index)
  {
    dst[index] = convertSampleScalar(src[index], bigEndian, normalizeP010);
  }
}

void deinterleaveUVSse2(const uint16_t *srcInterleaved,
                        uint16_t *dstU,
                        uint16_t *dstV,
                        int uvSampleCount,
                        bool bigEndian,
                        bool normalizeP010,
                        bool uFirst)
{
  if (!srcInterleaved || !dstU || !dstV || uvSampleCount <= 0)
  {
    return;
  }

  uint16_t *firstPlane  = uFirst ? dstU : dstV;
  uint16_t *secondPlane = uFirst ? dstV : dstU;

  int index = 0;
  for (; index + 8 <= uvSampleCount; index += 8)
  {
    const __m128i a =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcInterleaved + index * 2));
    const __m128i b =
        _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcInterleaved + index * 2 + 8));

    __m128i first;
    __m128i second;
    deinterleaveEightPairs(a, b, &first, &second);
    first  = fixupEightSamples(first, bigEndian, normalizeP010);
    second = fixupEightSamples(second, bigEndian, normalizeP010);

    _mm_storeu_si128(reinterpret_cast<__m128i *>(firstPlane + index), first);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(secondPlane + index), second);
  }

  for (; index < uvSampleCount; ++index)
  {
    const uint16_t s0 =
        convertSampleScalar(srcInterleaved[index * 2], bigEndian, normalizeP010);
    const uint16_t s1 =
        convertSampleScalar(srcInterleaved[index * 2 + 1], bigEndian, normalizeP010);
    firstPlane[index]  = s0;
    secondPlane[index] = s1;
  }
}

#endif // HDR_YUV_REPACK_X86

} // namespace kernels

void HDRYUVRepack::convertPlane16(const uint16_t *src,
                                  uint16_t *dst,
                                  int sampleCount,
                                  bool bigEndian,
                                  bool normalizeP010)
{
  static const ConvertPlaneFn fn = resolveConvertPlane16();
  fn(src, dst, sampleCount, bigEndian, normalizeP010);
}

void HDRYUVRepack::deinterleaveUV(const uint16_t *srcInterleaved,
                                  uint16_t *dstU,
                                  uint16_t *dstV,
                                  int uvSampleCount,
                                  bool bigEndian,
                                  bool normalizeP010,
                                  bool uFirst)
{
  static const DeinterleaveFn fn = resolveDeinterleaveUV();
  fn(srcInterleaved, dstU, dstV, uvSampleCount, bigEndian, normalizeP010, uFirst);
}

} // namespace video::hdr
