/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "CpuFeatures.h"

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define YUVIEW_CPUFEATURES_X86 1
#else
#define YUVIEW_CPUFEATURES_X86 0
#endif

#if YUVIEW_CPUFEATURES_X86
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <cstdint>
#endif
#endif

namespace common
{
namespace
{

#if YUVIEW_CPUFEATURES_X86

/**
 * @brief Read a four-register CPUID leaf (MSVC / GCC / Clang).
 * @param leaf CPUID main leaf (EAX input).
 * @param subleaf CPUID sub-leaf (ECX input).
 * @param eax Output EAX.
 * @param ebx Output EBX.
 * @param ecx Output ECX.
 * @param edx Output EDX.
 */
void cpuidLeaf(unsigned leaf,
               unsigned subleaf,
               unsigned &eax,
               unsigned &ebx,
               unsigned &ecx,
               unsigned &edx)
{
#if defined(_MSC_VER)
  int regs[4] = {};
  __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
  eax = static_cast<unsigned>(regs[0]);
  ebx = static_cast<unsigned>(regs[1]);
  ecx = static_cast<unsigned>(regs[2]);
  edx = static_cast<unsigned>(regs[3]);
#else
  eax = ebx = ecx = edx = 0;
  __cpuid_count(leaf, subleaf, eax, ebx, ecx, edx);
#endif
}

/**
 * @brief Read XCR0 (OS-enabled extended state mask).
 * @return Low 32 bits of XCR0; must not be called without XGETBV support.
 */
unsigned readXcr0()
{
#if defined(_MSC_VER)
  return static_cast<unsigned>(_xgetbv(0));
#else
  unsigned eax = 0;
  unsigned edx = 0;
  __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
  return eax;
#endif
}

#endif // YUVIEW_CPUFEATURES_X86

} // namespace

const CpuFeatures &CpuFeatures::instance()
{
  static const CpuFeatures features = CpuFeatures::detect();
  return features;
}

CpuFeatures CpuFeatures::detect()
{
  CpuFeatures features;

#if !YUVIEW_CPUFEATURES_X86
  return features;
#else

#if defined(_M_X64) || defined(__x86_64__)
  // x86_64 System V / Win64 ABI baselines on SSE2; no CPUID probe needed.
  features.sse2 = true;
#else
  unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
  cpuidLeaf(1, 0, eax, ebx, ecx, edx);
  features.sse2 = (edx & (1u << 26)) != 0;
#endif

  unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
  cpuidLeaf(1, 0, eax, ebx, ecx, edx);
  const bool osxsave = (ecx & (1u << 27)) != 0;
  const bool avx     = (ecx & (1u << 28)) != 0;

  if (!osxsave || !avx)
  {
    return features;
  }

  // XCR0 bit1=XMM, bit2=YMM; both must be set for safe AVX/AVX2 use.
  const unsigned xcr0         = readXcr0();
  const bool     ymmSavedByOs = (xcr0 & 0x6u) == 0x6u;
  if (!ymmSavedByOs)
  {
    return features;
  }

  unsigned maxLeaf = 0;
  cpuidLeaf(0, 0, maxLeaf, ebx, ecx, edx);
  if (maxLeaf < 7)
  {
    return features;
  }

  cpuidLeaf(7, 0, eax, ebx, ecx, edx);
  features.avx2 = (ebx & (1u << 5)) != 0;

  // AVX-512F also needs XCR0 opmask(5) + ZMM_hi256(6) + Hi16_ZMM(7).
  const bool zmmSavedByOs = (xcr0 & 0xe0u) == 0xe0u;
  features.avx512f        = zmmSavedByOs && ((ebx & (1u << 16)) != 0);
  return features;

#endif // YUVIEW_CPUFEATURES_X86
}

} // namespace common
