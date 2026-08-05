/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This translation unit must be compiled alone under /arch:AVX2 (MSVC) or
 *   -mavx2 (GCC/Clang). Do not mix the same global ISA flags into portable
 *   baseline object files.
 */

#include "video/hdr/HDRYUVRepackKernels.h"

#include <cstring>
#include <immintrin.h>

namespace video::hdr::kernels
{
namespace
{

/**
 * @brief Scalar sample transform (AVX2 main-loop tail fallback).
 * @param sample Raw 16-bit word.
 * @param bigEndian Big-endian flag.
 * @param normalizeP010 P010 >>6 flag.
 * @return Transformed sample.
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

/**
 * @brief Byte-swap each 16-bit lane in an AVX2 register.
 * @param v Packed uint16 lanes.
 * @return Byte-swapped vector.
 */
inline __m256i byteswapEpi16(__m256i v)
{
  return _mm256_or_si256(_mm256_slli_epi16(v, 8), _mm256_srli_epi16(v, 8));
}

/**
 * @brief Apply optional BE swap and P010 >>6 to sixteen packed samples.
 * @param loaded Sixteen uint16 values.
 * @param bigEndian If true, byte-swap each lane.
 * @param normalizeP010 If true, >>6.
 * @return Transformed lanes.
 */
inline __m256i fixupSixteenSamples(__m256i loaded, bool bigEndian, bool normalizeP010)
{
  if (bigEndian)
  {
    loaded = byteswapEpi16(loaded);
  }
  if (normalizeP010)
  {
    loaded = _mm256_srli_epi16(loaded, 6);
  }
  return loaded;
}

/**
 * @brief Helper that deinterleaves 4 pairs in one 128-bit half (same shuffle as SSE2).
 * @param a Four UV pairs: [U0 V0 U1 V1 U2 V2 U3 V3].
 * @param b Another four UV pairs.
 * @param outFirst Eight samples of the first component.
 * @param outSecond Eight samples of the second component.
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

/**
 * @brief Deinterleave 16 UV pairs from two 256-bit interleaved loads.
 *
 * Each __m256i holds 8 UV pairs (16 uint16). Split into 128-bit halves, reuse
 * the 8-pair shuffle, then reassemble 256-bit stores to keep correctness while
 * saturating 256-bit load/store bandwidth.
 *
 * @param uv0 Pairs 0-7.
 * @param uv1 Pairs 8-15.
 * @param outFirst Sixteen first-component samples.
 * @param outSecond Sixteen second-component samples.
 */
inline void deinterleaveSixteenPairs(__m256i uv0,
                                     __m256i uv1,
                                     __m256i *outFirst,
                                     __m256i *outSecond)
{
  __m128i firstLo;
  __m128i secondLo;
  deinterleaveEightPairs(_mm256_castsi256_si128(uv0),
                         _mm256_extracti128_si256(uv0, 1),
                         &firstLo,
                         &secondLo);

  __m128i firstHi;
  __m128i secondHi;
  deinterleaveEightPairs(_mm256_castsi256_si128(uv1),
                         _mm256_extracti128_si256(uv1, 1),
                         &firstHi,
                         &secondHi);

  *outFirst  = _mm256_set_m128i(firstHi, firstLo);
  *outSecond = _mm256_set_m128i(secondHi, secondLo);
}

} // namespace

void convertPlane16Avx2(const uint16_t *src,
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
  for (; index + 16 <= sampleCount; index += 16)
  {
    const __m256i loaded = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(src + index));
    const __m256i fixed  = fixupSixteenSamples(loaded, bigEndian, normalizeP010);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(dst + index), fixed);
  }
  for (; index < sampleCount; ++index)
  {
    dst[index] = convertSampleScalar(src[index], bigEndian, normalizeP010);
  }
}

void deinterleaveUVAvx2(const uint16_t *srcInterleaved,
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
  for (; index + 16 <= uvSampleCount; index += 16)
  {
    const __m256i uv0 =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(srcInterleaved + index * 2));
    const __m256i uv1 =
        _mm256_loadu_si256(reinterpret_cast<const __m256i *>(srcInterleaved + index * 2 + 16));

    __m256i first;
    __m256i second;
    deinterleaveSixteenPairs(uv0, uv1, &first, &second);
    first  = fixupSixteenSamples(first, bigEndian, normalizeP010);
    second = fixupSixteenSamples(second, bigEndian, normalizeP010);

    _mm256_storeu_si256(reinterpret_cast<__m256i *>(firstPlane + index), first);
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(secondPlane + index), second);
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

} // namespace video::hdr::kernels
