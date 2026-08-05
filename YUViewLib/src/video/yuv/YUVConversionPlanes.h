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
 * @file YUVConversionPlanes.h
 * @brief Planar YUV to RGB conversion functions
 * 
 * Contains specialized conversion functions for different YUV subsampling formats:
 * - 444: No subsampling
 * - 422: Horizontal subsampling by 2
 * - 420: Horizontal and vertical subsampling by 2
 * - 440: Vertical subsampling by 2
 * - 410: Horizontal and vertical subsampling by 4
 * - 411: Horizontal subsampling by 4
 */

#include "YUVConversionCore.h"
#include <video/LimitedRangeToFullRange.h>

namespace video::yuv::conversion
{

// ============================================================================
// Monochrome (Single Component) Conversion Functions
// ============================================================================

/**
 * @brief Convert single YUV plane to RGB monochrome (444 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_444(const int componentSize,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int i = 0; i < componentSize; ++i)
    {
        int newVal = getValueFromSource(src, i * inValSkip, bps, bigEndian);
        if (applyMath)
            newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

        if (shiftTo8Bit > 0)
            newVal = clip8Bit(newVal >> shiftTo8Bit);
        if (!fullRange)
            newVal = LimitedRangeToFullRange.at(newVal);

        // Set BGRA
        dst[i * 4] = (unsigned char)newVal;
        dst[i * 4 + 1] = (unsigned char)newVal;
        dst[i * 4 + 2] = (unsigned char)newVal;
        dst[i * 4 + 3] = 255;
    }
}

/**
 * @brief Convert single YUV plane to RGB monochrome (422 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_422(const int componentSize,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int i = 0; i < componentSize; ++i)
    {
        int newVal = getValueFromSource(src, i * inValSkip, bps, bigEndian);
        if (applyMath)
            newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

        if (shiftTo8Bit > 0)
            newVal = clip8Bit(newVal >> shiftTo8Bit);
        if (!fullRange)
            newVal = LimitedRangeToFullRange.at(newVal);

        // Set 2 pixels BGRA
        dst[i * 8] = (unsigned char)newVal;
        dst[i * 8 + 1] = (unsigned char)newVal;
        dst[i * 8 + 2] = (unsigned char)newVal;
        dst[i * 8 + 3] = 255;
        dst[i * 8 + 4] = (unsigned char)newVal;
        dst[i * 8 + 5] = (unsigned char)newVal;
        dst[i * 8 + 6] = (unsigned char)newVal;
        dst[i * 8 + 7] = 255;
    }
}

/**
 * @brief Convert single YUV plane to RGB monochrome (420 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_420(const int w,
                                        const int h,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int y = 0; y < h / 2; y++)
    {
        for (int x = 0; x < w / 2; x++)
        {
            const int srcIdx = y * (w / 2) + x;
            int newVal = getValueFromSource(src, srcIdx * inValSkip, bps, bigEndian);
            if (applyMath)
                newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

            if (shiftTo8Bit > 0)
                newVal = clip8Bit(newVal >> shiftTo8Bit);
            if (!fullRange)
                newVal = LimitedRangeToFullRange.at(newVal);

            // Set 4 pixels (2x2 block)
            int o = (y * 2 * w + x * 2) * 4;
            dst[o] = (unsigned char)newVal;
            dst[o + 1] = (unsigned char)newVal;
            dst[o + 2] = (unsigned char)newVal;
            dst[o + 3] = 255;
            dst[o + 4] = (unsigned char)newVal;
            dst[o + 5] = (unsigned char)newVal;
            dst[o + 6] = (unsigned char)newVal;
            dst[o + 7] = 255;
            o += w * 4;
            dst[o] = (unsigned char)newVal;
            dst[o + 1] = (unsigned char)newVal;
            dst[o + 2] = (unsigned char)newVal;
            dst[o + 3] = 255;
            dst[o + 4] = (unsigned char)newVal;
            dst[o + 5] = (unsigned char)newVal;
            dst[o + 6] = (unsigned char)newVal;
            dst[o + 7] = 255;
        }
    }
}

/**
 * @brief Convert single YUV plane to RGB monochrome (440 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_440(const int w,
                                        const int h,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int y = 0; y < h / 2; y++)
    {
        for (int x = 0; x < w; x++)
        {
            const int srcIdx = y * w + x;
            int newVal = getValueFromSource(src, srcIdx * inValSkip, bps, bigEndian);
            if (applyMath)
                newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

            if (shiftTo8Bit > 0)
                newVal = clip8Bit(newVal >> shiftTo8Bit);
            if (!fullRange)
                newVal = LimitedRangeToFullRange.at(newVal);

            const int pos1 = (y * 2 * w + x) * 4;
            const int pos2 = pos1 + w * 4;
            dst[pos1] = (unsigned char)newVal;
            dst[pos1 + 1] = (unsigned char)newVal;
            dst[pos1 + 2] = (unsigned char)newVal;
            dst[pos1 + 3] = 255;
            dst[pos2] = (unsigned char)newVal;
            dst[pos2 + 1] = (unsigned char)newVal;
            dst[pos2 + 2] = (unsigned char)newVal;
            dst[pos2 + 3] = 255;
        }
    }
}

/**
 * @brief Convert single YUV plane to RGB monochrome (410 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_410(const int w,
                                        const int h,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int y = 0; y < h / 4; y++)
    {
        for (int x = 0; x < w / 4; x++)
        {
            const int srcIdx = y * (w / 4) + x;
            int newVal = getValueFromSource(src, srcIdx * inValSkip, bps, bigEndian);

            if (applyMath)
                newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

            if (shiftTo8Bit > 0)
                newVal = clip8Bit(newVal >> shiftTo8Bit);
            if (!fullRange)
                newVal = LimitedRangeToFullRange.at(newVal);

            // Set 16 pixels (4x4 block)
            for (int yo = 0; yo < 4; yo++)
            {
                for (int xo = 0; xo < 4; xo++)
                {
                    const int pos = ((y * 4 + yo) * w + (x * 4 + xo)) * 4;
                    dst[pos] = (unsigned char)newVal;
                    dst[pos + 1] = (unsigned char)newVal;
                    dst[pos + 2] = (unsigned char)newVal;
                    dst[pos + 3] = 255;
                }
            }
        }
    }
}

/**
 * @brief Convert single YUV plane to RGB monochrome (411 subsampling)
 */
inline void YUVPlaneToRGBMonochrome_411(const int componentSize,
                                        const MathParameters math,
                                        const unsigned char* src,
                                        unsigned char* dst,
                                        const int inMax,
                                        const int bps,
                                        const bool bigEndian,
                                        const int inValSkip,
                                        const bool fullRange)
{
    const bool applyMath = math.mathRequired();
    const int shiftTo8Bit = bps - 8;
    
    for (int i = 0; i < componentSize; ++i)
    {
        int newVal = getValueFromSource(src, i * inValSkip, bps, bigEndian);
        if (applyMath)
            newVal = transformYUV(math.invert, math.scale, math.offset, newVal, inMax);

        if (shiftTo8Bit > 0)
            newVal = clip8Bit(newVal >> shiftTo8Bit);
        if (!fullRange)
            newVal = LimitedRangeToFullRange.at(newVal);

        // Set 4 pixels
        for (int j = 0; j < 4; j++)
        {
            dst[i * 16 + j * 4] = (unsigned char)newVal;
            dst[i * 16 + j * 4 + 1] = (unsigned char)newVal;
            dst[i * 16 + j * 4 + 2] = (unsigned char)newVal;
            dst[i * 16 + j * 4 + 3] = 255;
        }
    }
}

// ============================================================================
// Full YUV to RGB Conversion Functions
// ============================================================================

/**
 * @brief Convert YUV 444 planar to RGB
 */
inline void YUVPlaneToRGB_444(const int componentSize,
                              const MathParameters mathY,
                              const MathParameters mathC,
                              const unsigned char* srcY,
                              const unsigned char* srcU,
                              const unsigned char* srcV,
                              unsigned char* dst,
                              const int RGBConv[5],
                              const bool fullRange,
                              const int inMax,
                              const int bps,
                              const bool bigEndian,
                              const int inValSkip)
{
    const bool applyMathLuma = mathY.mathRequired();
    const bool applyMathChroma = mathC.mathRequired();

    for (int i = 0; i < componentSize; ++i)
    {
        unsigned int valY = getValueFromSource(srcY, i, bps, bigEndian);
        unsigned int valU = getValueFromSource(srcU, i * inValSkip, bps, bigEndian);
        unsigned int valV = getValueFromSource(srcV, i * inValSkip, bps, bigEndian);

        if (applyMathLuma)
            valY = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY, inMax);
        if (applyMathChroma)
        {
            valU = transformYUV(mathC.invert, mathC.scale, mathC.offset, valU, inMax);
            valV = transformYUV(mathC.invert, mathC.scale, mathC.offset, valV, inMax);
        }

        int valR, valG, valB;
        convertYUVToRGB8Bit(valY, valU, valV, valR, valG, valB, RGBConv, fullRange, bps);

        dst[i * 4] = valB;
        dst[i * 4 + 1] = valG;
        dst[i * 4 + 2] = valR;
        dst[i * 4 + 3] = 255;
    }
}

/**
 * @brief Convert YUV 422 planar to RGB with interpolation
 */
inline void YUVPlaneToRGB_422(const int w,
                              const int h,
                              const MathParameters mathY,
                              const MathParameters mathC,
                              const unsigned char* srcY,
                              const unsigned char* srcU,
                              const unsigned char* srcV,
                              unsigned char* dst,
                              const int RGBConv[5],
                              const bool fullRange,
                              const int inMax,
                              const ChromaInterpolation interpolation,
                              const int bps,
                              const bool bigEndian,
                              const int inValSkip)
{
    const bool applyMathLuma = mathY.mathRequired();
    const bool applyMathChroma = mathC.mathRequired();
    
    for (int y = 0; y < h; y++)
    {
        const int srcIdxUV = y * w / 2;
        int curUSample = getValueFromSource(srcU, srcIdxUV * inValSkip, bps, bigEndian);
        int curVSample = getValueFromSource(srcV, srcIdxUV * inValSkip, bps, bigEndian);
        
        if (applyMathChroma)
        {
            curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
            curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
        }

        for (int x = 0; x < (w / 2) - 1; x++)
        {
            const int srcPosLineUV = srcIdxUV + x + 1;
            int nextUSample = getValueFromSource(srcU, srcPosLineUV * inValSkip, bps, bigEndian);
            int nextVSample = getValueFromSource(srcV, srcPosLineUV * inValSkip, bps, bigEndian);
            
            if (applyMathChroma)
            {
                nextUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextUSample, inMax);
                nextVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextVSample, inMax);
            }

            int interpolatedU = interpolateUVSample(interpolation, curUSample, nextUSample);
            int interpolatedV = interpolateUVSample(interpolation, curVSample, nextVSample);

            int valY1 = getValueFromSource(srcY, y * w + x * 2, bps, bigEndian);
            int valY2 = getValueFromSource(srcY, y * w + x * 2 + 1, bps, bigEndian);
            
            if (applyMathLuma)
            {
                valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
                valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
            }

            int valR1, valR2, valG1, valG2, valB1, valB2;
            convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
            convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, fullRange, bps);
            
            const int pos = (y * w + x * 2) * 4;
            dst[pos] = valB1;
            dst[pos + 1] = valG1;
            dst[pos + 2] = valR1;
            dst[pos + 3] = 255;
            dst[pos + 4] = valB2;
            dst[pos + 5] = valG2;
            dst[pos + 6] = valR2;
            dst[pos + 7] = 255;

            curUSample = nextUSample;
            curVSample = nextVSample;
        }

        // Last pair in row
        int valY1 = getValueFromSource(srcY, (y + 1) * w - 2, bps, bigEndian);
        int valY2 = getValueFromSource(srcY, (y + 1) * w - 1, bps, bigEndian);
        if (applyMathLuma)
        {
            valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
            valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
        }

        int valR1, valR2, valG1, valG2, valB1, valB2;
        convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos = ((y + 1) * w) * 4;
        dst[pos - 8] = valB1;
        dst[pos - 7] = valG1;
        dst[pos - 6] = valR1;
        dst[pos - 5] = 255;
        dst[pos - 4] = valB2;
        dst[pos - 3] = valG2;
        dst[pos - 2] = valR2;
        dst[pos - 1] = 255;
    }
}

} // namespace video::yuv::conversion
