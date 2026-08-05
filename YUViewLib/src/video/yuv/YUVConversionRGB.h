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
 * @file YUVConversionRGB.h
 * @brief Full YUV to RGB planar conversion functions
 * 
 * Contains the main YUVPlaneToRGB_* functions for different subsampling formats.
 * These functions are performance-critical and handle the full YUV->RGB conversion
 * with chroma interpolation.
 * 
 * Supported formats:
 * - 444: No subsampling
 * - 422: Horizontal 2x subsampling
 * - 440: Vertical 2x subsampling
 * - 420: Horizontal and vertical 2x subsampling
 * - 410: Horizontal and vertical 4x subsampling
 * - 411: Horizontal 4x subsampling
 */

#include "YUVConversionCore.h"
#include "YUVConversionPlanes.h"

namespace video::yuv::conversion
{

/**
 * @brief Convert YUV 440 planar to RGB (vertical 2x subsampling)
 */
inline void YUVPlaneToRGB_440(const int w,
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

    for (int x = 0; x < w; x++)
    {
        int curUSample = getValueFromSource(srcU, x * inValSkip, bps, bigEndian);
        int curVSample = getValueFromSource(srcV, x * inValSkip, bps, bigEndian);
        if (applyMathChroma)
        {
            curUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curUSample, inMax);
            curVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, curVSample, inMax);
        }

        for (int y = 0; y < (h / 2) - 1; y++)
        {
            const int srcIdxUV = y * w + x;
            int nextUSample = getValueFromSource(srcU, srcIdxUV * inValSkip, bps, bigEndian);
            int nextVSample = getValueFromSource(srcV, srcIdxUV * inValSkip, bps, bigEndian);
            if (applyMathChroma)
            {
                nextUSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextUSample, inMax);
                nextVSample = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextVSample, inMax);
            }

            int interpolatedU = interpolateUVSample(interpolation, curUSample, nextUSample);
            int interpolatedV = interpolateUVSample(interpolation, curVSample, nextVSample);

            int valY1 = getValueFromSource(srcY, y * 2 * w + x, bps, bigEndian);
            int valY2 = getValueFromSource(srcY, (y * 2 + 1) * w + x, bps, bigEndian);
            if (applyMathLuma)
            {
                valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
                valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
            }

            int valR1, valR2, valG1, valG2, valB1, valB2;
            convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
            convertYUVToRGB8Bit(valY2, interpolatedU, interpolatedV, valR2, valG2, valB2, RGBConv, fullRange, bps);
            
            const int pos1 = (y * 2 * w + x) * 4;
            const int pos2 = pos1 + 4 * w;
            dst[pos1] = valB1; dst[pos1 + 1] = valG1; dst[pos1 + 2] = valR1; dst[pos1 + 3] = 255;
            dst[pos2] = valB2; dst[pos2 + 1] = valG2; dst[pos2 + 2] = valR2; dst[pos2 + 3] = 255;

            curUSample = nextUSample;
            curVSample = nextVSample;
        }

        // Last row
        int valY1 = getValueFromSource(srcY, (h - 2) * w + x, bps, bigEndian);
        int valY2 = getValueFromSource(srcY, (h - 1) * w + x, bps, bigEndian);
        if (applyMathLuma)
        {
            valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
            valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
        }

        int valR1, valR2, valG1, valG2, valB1, valB2;
        convertYUVToRGB8Bit(valY1, curUSample, curVSample, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY2, curUSample, curVSample, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos1 = ((h - 2) * w + x) * 4;
        const int pos2 = pos1 + w * 4;
        dst[pos1] = valB1; dst[pos1 + 1] = valG1; dst[pos1 + 2] = valR1; dst[pos1 + 3] = 255;
        dst[pos2] = valB2; dst[pos2 + 1] = valG2; dst[pos2 + 2] = valR2; dst[pos2 + 3] = 255;
    }
}

/**
 * @brief Convert YUV 420 planar to RGB (2x2 subsampling)
 */
inline void YUVPlaneToRGB_420(const int w,
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
    const int hh = h / 2;
    const int wh = w / 2;

    for (int y = 0; y < hh - 1; y++)
    {
        const int srcIdxUV0 = y * wh;
        const int srcIdxUV1 = (y + 1) * wh;
        int curU = getValueFromSource(srcU, srcIdxUV0 * inValSkip, bps, bigEndian);
        int curV = getValueFromSource(srcV, srcIdxUV0 * inValSkip, bps, bigEndian);
        int curU_NL = getValueFromSource(srcU, srcIdxUV1 * inValSkip, bps, bigEndian);
        int curV_NL = getValueFromSource(srcV, srcIdxUV1 * inValSkip, bps, bigEndian);
        
        if (applyMathChroma)
        {
            curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
            curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
            curU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU_NL, inMax);
            curV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV_NL, inMax);
        }

        for (int x = 0; x < wh - 1; x++)
        {
            const int srcIdxUVLine0 = srcIdxUV0 + x + 1;
            const int srcIdxUVLine1 = srcIdxUV1 + x + 1;
            int nextU = getValueFromSource(srcU, srcIdxUVLine0 * inValSkip, bps, bigEndian);
            int nextV = getValueFromSource(srcV, srcIdxUVLine0 * inValSkip, bps, bigEndian);
            int nextU_NL = getValueFromSource(srcU, srcIdxUVLine1 * inValSkip, bps, bigEndian);
            int nextV_NL = getValueFromSource(srcV, srcIdxUVLine1 * inValSkip, bps, bigEndian);
            
            if (applyMathChroma)
            {
                nextU = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
                nextV = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
                nextU_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU_NL, inMax);
                nextV_NL = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV_NL, inMax);
            }

            int interpolatedU_Hor = interpolateUVSample(interpolation, curU, nextU);
            int interpolatedV_Hor = interpolateUVSample(interpolation, curV, nextV);
            int interpolatedU_Ver = interpolateUVSample(interpolation, curU, curU_NL);
            int interpolatedV_Ver = interpolateUVSample(interpolation, curV, curV_NL);
            int interpolatedU_Bi = interpolateUVSample2D(interpolation, curU, nextU, curU_NL, nextU_NL);
            int interpolatedV_Bi = interpolateUVSample2D(interpolation, curV, nextV, curV_NL, nextV_NL);

            int valY1 = getValueFromSource(srcY, (y * w + x) * 2, bps, bigEndian);
            int valY2 = getValueFromSource(srcY, (y * w + x) * 2 + 1, bps, bigEndian);
            int valY3 = getValueFromSource(srcY, (y * 2 + 1) * w + x * 2, bps, bigEndian);
            int valY4 = getValueFromSource(srcY, (y * 2 + 1) * w + x * 2 + 1, bps, bigEndian);
            
            if (applyMathLuma)
            {
                valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
                valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
                valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
                valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
            }

            int valR1, valR2, valG1, valG2, valB1, valB2;
            convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
            convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
            
            const int pos1 = (y * 2 * w + x * 2) * 4;
            dst[pos1] = valB1; dst[pos1 + 1] = valG1; dst[pos1 + 2] = valR1; dst[pos1 + 3] = 255;
            dst[pos1 + 4] = valB2; dst[pos1 + 5] = valG2; dst[pos1 + 6] = valR2; dst[pos1 + 7] = 255;
            
            convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, fullRange, bps);
            convertYUVToRGB8Bit(valY4, interpolatedU_Bi, interpolatedV_Bi, valR2, valG2, valB2, RGBConv, fullRange, bps);
            
            const int pos2 = pos1 + w * 4;
            dst[pos2] = valB1; dst[pos2 + 1] = valG1; dst[pos2 + 2] = valR1; dst[pos2 + 3] = 255;
            dst[pos2 + 4] = valB2; dst[pos2 + 5] = valG2; dst[pos2 + 6] = valR2; dst[pos2 + 7] = 255;

            curU = nextU; curV = nextV;
            curU_NL = nextU_NL; curV_NL = nextV_NL;
        }

        // Handle right border (sample and hold horizontally)
        int interpolatedU_Ver = interpolateUVSample(interpolation, curU, curU_NL);
        int interpolatedV_Ver = interpolateUVSample(interpolation, curV, curV_NL);

        int valY1 = getValueFromSource(srcY, (y * 2 + 1) * w - 2, bps, bigEndian);
        int valY2 = getValueFromSource(srcY, (y * 2 + 1) * w - 1, bps, bigEndian);
        int valY3 = getValueFromSource(srcY, (y * 2 + 2) * w - 2, bps, bigEndian);
        int valY4 = getValueFromSource(srcY, (y * 2 + 2) * w - 1, bps, bigEndian);
        
        if (applyMathLuma)
        {
            valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
            valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
            valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
            valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
        }

        int valR1, valR2, valG1, valG2, valB1, valB2;
        convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos1 = ((y * 2 + 1) * w) * 4;
        dst[pos1 - 8] = valB1; dst[pos1 - 7] = valG1; dst[pos1 - 6] = valR1; dst[pos1 - 5] = 255;
        dst[pos1 - 4] = valB2; dst[pos1 - 3] = valG2; dst[pos1 - 2] = valR2; dst[pos1 - 1] = 255;
        
        convertYUVToRGB8Bit(valY3, interpolatedU_Ver, interpolatedV_Ver, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY4, interpolatedU_Ver, interpolatedV_Ver, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos2 = pos1 + w * 4;
        dst[pos2 - 8] = valB1; dst[pos2 - 7] = valG1; dst[pos2 - 6] = valR1; dst[pos2 - 5] = 255;
        dst[pos2 - 4] = valB2; dst[pos2 - 3] = valG2; dst[pos2 - 2] = valR2; dst[pos2 - 1] = 255;
    }

    // Handle bottom border (last two rows)
    const int y = hh - 1;
    const int y2 = (hh - 1) * 2;
    const int srcIdxUV = y * wh;
    int curU = getValueFromSource(srcU, srcIdxUV * inValSkip, bps, bigEndian);
    int curV = getValueFromSource(srcV, srcIdxUV * inValSkip, bps, bigEndian);
    
    if (applyMathChroma)
    {
        curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
        curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
    }

    for (int x = 0; x < wh - 1; x++)
    {
        const int srcIdxLineUV = srcIdxUV + x + 1;
        int nextU = getValueFromSource(srcU, srcIdxLineUV * inValSkip, bps, bigEndian);
        int nextV = getValueFromSource(srcV, srcIdxLineUV * inValSkip, bps, bigEndian);
        
        if (applyMathChroma)
        {
            nextU = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
            nextV = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
        }

        int interpolatedU_Hor = interpolateUVSample(interpolation, curU, nextU);
        int interpolatedV_Hor = interpolateUVSample(interpolation, curV, nextV);

        int valY1 = getValueFromSource(srcY, (y * w + x) * 2, bps, bigEndian);
        int valY2 = getValueFromSource(srcY, (y * w + x) * 2 + 1, bps, bigEndian);
        int valY3 = getValueFromSource(srcY, (y2 + 1) * w + x * 2, bps, bigEndian);
        int valY4 = getValueFromSource(srcY, (y2 + 1) * w + x * 2 + 1, bps, bigEndian);
        
        if (applyMathLuma)
        {
            valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
            valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
            valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
            valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
        }

        int valR1, valR2, valG1, valG2, valB1, valB2;
        convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY2, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos1 = (y2 * w + x * 2) * 4;
        dst[pos1] = valB1; dst[pos1 + 1] = valG1; dst[pos1 + 2] = valR1; dst[pos1 + 3] = 255;
        dst[pos1 + 4] = valB2; dst[pos1 + 5] = valG2; dst[pos1 + 6] = valR2; dst[pos1 + 7] = 255;
        
        convertYUVToRGB8Bit(valY3, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
        convertYUVToRGB8Bit(valY4, interpolatedU_Hor, interpolatedV_Hor, valR2, valG2, valB2, RGBConv, fullRange, bps);
        
        const int pos2 = pos1 + w * 4;
        dst[pos2] = valB1; dst[pos2 + 1] = valG1; dst[pos2 + 2] = valR1; dst[pos2 + 3] = 255;
        dst[pos2 + 4] = valB2; dst[pos2 + 5] = valG2; dst[pos2 + 6] = valR2; dst[pos2 + 7] = 255;

        curU = nextU;
        curV = nextV;
    }

    // Bottom-right corner
    int valY1 = getValueFromSource(srcY, (y2 + 1) * w - 2, bps, bigEndian);
    int valY2 = getValueFromSource(srcY, (y2 + 1) * w - 1, bps, bigEndian);
    int valY3 = getValueFromSource(srcY, (y2 + 2) * w - 2, bps, bigEndian);
    int valY4 = getValueFromSource(srcY, (y2 + 2) * w - 1, bps, bigEndian);
    
    if (applyMathLuma)
    {
        valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
        valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
        valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
        valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
    }

    int valR1, valR2, valG1, valG2, valB1, valB2;
    convertYUVToRGB8Bit(valY1, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY2, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
    
    const int pos1 = (y2 + 1) * w * 4;
    dst[pos1 - 8] = valB1; dst[pos1 - 7] = valG1; dst[pos1 - 6] = valR1; dst[pos1 - 5] = 255;
    dst[pos1 - 4] = valB2; dst[pos1 - 3] = valG2; dst[pos1 - 2] = valR2; dst[pos1 - 1] = 255;
    
    convertYUVToRGB8Bit(valY3, curU, curV, valR1, valG1, valB1, RGBConv, fullRange, bps);
    convertYUVToRGB8Bit(valY4, curU, curV, valR2, valG2, valB2, RGBConv, fullRange, bps);
    
    const int pos2 = pos1 + w * 4;
    dst[pos2 - 8] = valB1; dst[pos2 - 7] = valG1; dst[pos2 - 6] = valR1; dst[pos2 - 5] = 255;
    dst[pos2 - 4] = valB2; dst[pos2 - 3] = valG2; dst[pos2 - 2] = valR2; dst[pos2 - 1] = 255;
}

/**
 * @brief Convert YUV 410 planar to RGB (4x4 subsampling)
 */
inline void YUVPlaneToRGB_410(const int w,
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
    
    // 4:1:0 - chroma subsampled 4x horizontally and 4x vertically
    const int wh = w / 4;
    const int hh = h / 4;
    
    for (int yc = 0; yc < hh; yc++)
    {
        for (int xc = 0; xc < wh; xc++)
        {
            const int srcIdxUV = yc * wh + xc;
            int curU = getValueFromSource(srcU, srcIdxUV * inValSkip, bps, bigEndian);
            int curV = getValueFromSource(srcV, srcIdxUV * inValSkip, bps, bigEndian);
            
            if (applyMathChroma)
            {
                curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
                curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
            }
            
            // Process 4x4 block of Y samples
            for (int yo = 0; yo < 4; yo++)
            {
                for (int xo = 0; xo < 4; xo++)
                {
                    const int yp = yc * 4 + yo;
                    const int xp = xc * 4 + xo;
                    
                    int valY = getValueFromSource(srcY, yp * w + xp, bps, bigEndian);
                    if (applyMathLuma)
                        valY = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY, inMax);
                    
                    int valR, valG, valB;
                    convertYUVToRGB8Bit(valY, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
                    
                    const int pos = (yp * w + xp) * 4;
                    dst[pos] = valB;
                    dst[pos + 1] = valG;
                    dst[pos + 2] = valR;
                    dst[pos + 3] = 255;
                }
            }
        }
    }
}

/**
 * @brief Convert YUV 411 planar to RGB (4x horizontal subsampling)
 */
inline void YUVPlaneToRGB_411(const int w,
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
    const int wh = w / 4;
    
    for (int y = 0; y < h; y++)
    {
        const int srcIdxUV = y * wh;
        int curU = getValueFromSource(srcU, srcIdxUV * inValSkip, bps, bigEndian);
        int curV = getValueFromSource(srcV, srcIdxUV * inValSkip, bps, bigEndian);
        
        if (applyMathChroma)
        {
            curU = transformYUV(mathC.invert, mathC.scale, mathC.offset, curU, inMax);
            curV = transformYUV(mathC.invert, mathC.scale, mathC.offset, curV, inMax);
        }
        
        for (int x = 0; x < wh - 1; x++)
        {
            const int srcIdxUVLine = srcIdxUV + x + 1;
            int nextU = getValueFromSource(srcU, srcIdxUVLine * inValSkip, bps, bigEndian);
            int nextV = getValueFromSource(srcV, srcIdxUVLine * inValSkip, bps, bigEndian);
            
            if (applyMathChroma)
            {
                nextU = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextU, inMax);
                nextV = transformYUV(mathC.invert, mathC.scale, mathC.offset, nextV, inMax);
            }
            
            // Interpolate at quarter positions
            int interpU1 = interpolateUVSampleQ(interpolation, curU, nextU, 1);
            int interpV1 = interpolateUVSampleQ(interpolation, curV, nextV, 1);
            int interpU2 = interpolateUVSampleQ(interpolation, curU, nextU, 2);
            int interpV2 = interpolateUVSampleQ(interpolation, curV, nextV, 2);
            int interpU3 = interpolateUVSampleQ(interpolation, curU, nextU, 3);
            int interpV3 = interpolateUVSampleQ(interpolation, curV, nextV, 3);
            
            // Get 4 Y samples
            int valY1 = getValueFromSource(srcY, y * w + x * 4, bps, bigEndian);
            int valY2 = getValueFromSource(srcY, y * w + x * 4 + 1, bps, bigEndian);
            int valY3 = getValueFromSource(srcY, y * w + x * 4 + 2, bps, bigEndian);
            int valY4 = getValueFromSource(srcY, y * w + x * 4 + 3, bps, bigEndian);
            
            if (applyMathLuma)
            {
                valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
                valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
                valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
                valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
            }
            
            int valR, valG, valB;
            const int pos = (y * w + x * 4) * 4;
            
            convertYUVToRGB8Bit(valY1, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
            dst[pos] = valB; dst[pos + 1] = valG; dst[pos + 2] = valR; dst[pos + 3] = 255;
            
            convertYUVToRGB8Bit(valY2, interpU1, interpV1, valR, valG, valB, RGBConv, fullRange, bps);
            dst[pos + 4] = valB; dst[pos + 5] = valG; dst[pos + 6] = valR; dst[pos + 7] = 255;
            
            convertYUVToRGB8Bit(valY3, interpU2, interpV2, valR, valG, valB, RGBConv, fullRange, bps);
            dst[pos + 8] = valB; dst[pos + 9] = valG; dst[pos + 10] = valR; dst[pos + 11] = 255;
            
            convertYUVToRGB8Bit(valY4, interpU3, interpV3, valR, valG, valB, RGBConv, fullRange, bps);
            dst[pos + 12] = valB; dst[pos + 13] = valG; dst[pos + 14] = valR; dst[pos + 15] = 255;
            
            curU = nextU;
            curV = nextV;
        }
        
        // Handle last 4 pixels in row (sample and hold)
        int valY1 = getValueFromSource(srcY, (y + 1) * w - 4, bps, bigEndian);
        int valY2 = getValueFromSource(srcY, (y + 1) * w - 3, bps, bigEndian);
        int valY3 = getValueFromSource(srcY, (y + 1) * w - 2, bps, bigEndian);
        int valY4 = getValueFromSource(srcY, (y + 1) * w - 1, bps, bigEndian);
        
        if (applyMathLuma)
        {
            valY1 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY1, inMax);
            valY2 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY2, inMax);
            valY3 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY3, inMax);
            valY4 = transformYUV(mathY.invert, mathY.scale, mathY.offset, valY4, inMax);
        }
        
        int valR, valG, valB;
        const int pos = ((y + 1) * w) * 4;
        
        convertYUVToRGB8Bit(valY1, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
        dst[pos - 16] = valB; dst[pos - 15] = valG; dst[pos - 14] = valR; dst[pos - 13] = 255;
        
        convertYUVToRGB8Bit(valY2, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
        dst[pos - 12] = valB; dst[pos - 11] = valG; dst[pos - 10] = valR; dst[pos - 9] = 255;
        
        convertYUVToRGB8Bit(valY3, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
        dst[pos - 8] = valB; dst[pos - 7] = valG; dst[pos - 6] = valR; dst[pos - 5] = 255;
        
        convertYUVToRGB8Bit(valY4, curU, curV, valR, valG, valB, RGBConv, fullRange, bps);
        dst[pos - 4] = valB; dst[pos - 3] = valG; dst[pos - 2] = valR; dst[pos - 1] = 255;
    }
}

} // namespace video::yuv::conversion
