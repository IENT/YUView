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

namespace common
{

/**
 * @brief Read-only snapshot of host CPU ISA capabilities (immutable after startup).
 *
 * Detected once via CPUID (and XGETBV where required for AVX), then used for
 * hot-path dynamic dispatch: one portable baseline binary runs AVX2 kernels on
 * capable CPUs and SSE2/scalar kernels on older hardware.
 */
class CpuFeatures
{
public:
  /**
   * @brief Process-wide singleton detection result (C++11 static locals: thread-safe, once).
   * @return Read-only CPU capability snapshot.
   */
  static const CpuFeatures &instance();

  /** @brief Whether SSE2 is available (always true under the x86_64 ABI). */
  bool hasSse2() const { return this->sse2; }

  /**
   * @brief Whether AVX2 can be executed safely (OSXSAVE + XCR0.YMM OS state save).
   *
   * Checking CPUID.AVX2 alone is insufficient; without OS YMM state save support,
   * AVX instructions raise #UD.
   */
  bool hasAvx2() const { return this->avx2; }

  /**
   * @brief Whether AVX-512F can be executed safely (XCR0 ZMM/opmask save bits).
   *
   * No AVX-512 kernels are wired into the hot path yet; reserved for optional future use.
   */
  bool hasAvx512f() const { return this->avx512f; }

private:
  CpuFeatures() = default;

  /**
   * @brief Run CPUID/XGETBV and fill the feature flags on this object.
   * @return Completed capability snapshot.
   */
  static CpuFeatures detect();

  bool sse2    = false;
  bool avx2    = false;
  bool avx512f = false;
};

} // namespace common
