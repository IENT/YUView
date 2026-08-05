/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

/**
 * @file HDR_Module.h
 * @brief HDR rendering module umbrella header
 *
 * Active components:
 * - HDRTypes.h: render-mode / tone-map constants and color coefficients
 * - HDRColorConversion: YUV->RGB matrix builders (BT.601/709/2020)
 * - HDRYUVRepack: endian / P010 plane staging (CPUID dispatch AVX2/SSE2/scalar)
 * - HDRDetection(+Worker): Windows DXGI HDR capability detection
 * - HDRRenderingManager: singleton lifecycle / window attach
 * - HDR_RhiVideoWindow(+Overlays): QRhi HDR presentation path
 *
 * The live GPU upload path lives inside HDR_RhiVideoWindow::uploadYUVTextureData.
 * There is no separate HDRRhiPipeline / HDRTextureUploader layer.
 */

#ifndef HDR_MODULE_H
#define HDR_MODULE_H

#include "HDRTypes.h"
#include "HDRColorConversion.h"
#include "HDRYUVRepack.h"

#endif // HDR_MODULE_H
