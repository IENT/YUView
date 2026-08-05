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
 * @file YUVConversionCore.h
 * @brief Core YUV conversion functions extracted from videoHandlerYUV.cpp
 * 
 * This file contains all the low-level YUV->RGB conversion functions,
 * separated from videoHandlerYUV for Single Responsibility Principle compliance.
 * 
 * These functions are kept inline for performance reasons (hot path in video decoding).
 */

#include "PixelFormatYUV.h"
#include <video/LimitedRangeToFullRange.h>
#include <QByteArray>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace video::yuv::conversion
{

// ============================================================================
// Clipping Table and Basic Operations
// ============================================================================

/// Clipping buffer for fast 8-bit clamping: [-384, 640) -> [0, 255]
inline unsigned char* getClipBuffer()
{
    static unsigned char clp_buf[384 + 256 + 384];
    static bool initialized = false;
    
    if (!initialized)
    {
        memset(clp_buf, 0, 384);
        for (int i = 0; i < 256; i++)
            clp_buf[384 + i] = i;
        memset(clp_buf + 384 + 256, 255, 384);
        initialized = true;
    }
    
    return clp_buf + 384;  // Return pointer to middle for direct indexing
}

/**
 * @brief Clip value to 8-bit range [0, 255]
 */
inline int clip8Bit(int val)
{
    if (val < 0)
        return 0;
    if (val > 255)
        return 255;
    return val;
}

/**
 * @brief Apply YUV math transformation (scale, offset, invert)
 * 
 * @param invert If true, invert around offset
 * @param scale Scale factor (100 = no scaling)
 * @param offset Offset value for inversion center
 * @param value Input sample value
 * @param clipMax Maximum value for clipping
 * @return Transformed value
 */
inline int transformYUV(const bool invert,
                        const int scale,
                        const int offset,
                        const unsigned int value,
                        const int clipMax)
{
    int newValue = value;
    if (invert)
        newValue = -(newValue - offset) * scale + offset;
    else
        newValue = (newValue - offset) * scale + offset;

    if (newValue < 0)
        newValue = 0;
    if (newValue > clipMax)
        newValue = clipMax;

    return newValue;
}

/**
 * @brief Read a sample value from source buffer
 * 
 * @param src Source buffer
 * @param idx Sample index
 * @param bps Bits per sample
 * @param bigEndian Byte order for >8bit formats
 * @return Sample value
 */
inline int getValueFromSource(const unsigned char* src,
                              const int idx,
                              const int bps,
                              const bool bigEndian)
{
    if (bps > 8)
        return bigEndian ? (src[idx * 2] << 8 | src[idx * 2 + 1])
                         : (src[idx * 2] | src[idx * 2 + 1] << 8);
    return src[idx];
}

/**
 * @brief Write a sample value to destination buffer
 * 
 * @param dst Destination buffer
 * @param val Value to write
 * @param idx Sample index
 * @param bps Bits per sample
 * @param bigEndian Byte order for >8bit formats
 */
inline void setValueInBuffer(unsigned char* dst,
                             const int val,
                             const int idx,
                             const int bps,
                             const bool bigEndian)
{
    if (bps > 8)
    {
        if (bigEndian)
        {
            dst[idx * 2] = val >> 8;
            dst[idx * 2 + 1] = val & 0xff;
        }
        else
        {
            dst[idx * 2] = val & 0xff;
            dst[idx * 2 + 1] = val >> 8;
        }
    }
    else
        dst[idx] = val;
}

// ============================================================================
// Color Conversion Functions
// ============================================================================

/**
 * @brief Check if color conversion uses full range
 */
inline bool isFullRange(const ColorConversion colorConversion)
{
    return colorConversion == ColorConversion::BT709_FullRange ||
           colorConversion == ColorConversion::BT601_FullRange ||
           colorConversion == ColorConversion::BT2020_FullRange;
}

/**
 * @brief Convert YUV sample to 8-bit RGB
 * 
 * @param valY Y (luma) value
 * @param valU U (Cb) value
 * @param valV V (Cr) value
 * @param valR Output R value
 * @param valG Output G value
 * @param valB Output B value
 * @param RGBConv Conversion coefficients [5]
 * @param fullRange True if full range (0-255), false for limited (16-235)
 * @param bps Bits per sample of input
 */
inline void convertYUVToRGB8Bit(const unsigned int valY,
                                const unsigned int valU,
                                const unsigned int valV,
                                int& valR,
                                int& valG,
                                int& valB,
                                const int RGBConv[5],
                                const bool fullRange,
                                const int bps)
{
    if (bps > 14)
    {
        // High bit depth: right-shift to avoid overflow
        const int yOffset = fullRange ? 0 : 16 << (bps - 10);
        const int cZero = 128 << (bps - 10);

        const int Y_tmp = ((valY >> 2) - yOffset) * RGBConv[0];
        const int U_tmp = (valU >> 2) - cZero;
        const int V_tmp = (valV >> 2) - cZero;

        const int R_tmp = (Y_tmp + V_tmp * RGBConv[1]) >> (16 + bps - 10);
        const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 10);
        const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]) >> (16 + bps - 10);

        valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
        valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
        valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
    }
    else
    {
        const int yOffset = fullRange ? 0 : 16 << (bps - 8);
        const int cZero = 128 << (bps - 8);

        const int Y_tmp = (valY - yOffset) * RGBConv[0];
        const int U_tmp = valU - cZero;
        const int V_tmp = valV - cZero;

        const int R_tmp = (Y_tmp + V_tmp * RGBConv[1]) >> (16 + bps - 8);
        const int G_tmp = (Y_tmp + U_tmp * RGBConv[2] + V_tmp * RGBConv[3]) >> (16 + bps - 8);
        const int B_tmp = (Y_tmp + U_tmp * RGBConv[4]) >> (16 + bps - 8);

        valR = (R_tmp < 0) ? 0 : (R_tmp > 255) ? 255 : R_tmp;
        valG = (G_tmp < 0) ? 0 : (G_tmp > 255) ? 255 : G_tmp;
        valB = (B_tmp < 0) ? 0 : (B_tmp > 255) ? 255 : B_tmp;
    }
}

// ============================================================================
// Chroma Interpolation Functions
// ============================================================================

/**
 * @brief Interpolate between two UV samples
 */
inline int interpolateUVSample(const ChromaInterpolation mode, 
                               const int sample1, 
                               const int sample2)
{
    if (mode == ChromaInterpolation::Bilinear)
        return ((sample1 + sample2) + 1) >> 1;
    return sample1;  // Sample and hold
}

/**
 * @brief Interpolate at quarter positions between two UV samples
 */
inline int interpolateUVSampleQ(const ChromaInterpolation mode,
                                const int sample1,
                                const int sample2,
                                const int quarterPos)
{
    if (mode == ChromaInterpolation::Bilinear)
    {
        if (quarterPos == 0)
            return sample1;
        if (quarterPos == 1)
            return ((sample1 * 3 + sample2) + 1) >> 2;
        if (quarterPos == 2)
            return ((sample1 + sample2) + 1) >> 1;
        if (quarterPos == 3)
            return ((sample1 + sample2 * 3) + 1) >> 2;
    }
    return sample1;
}

/**
 * @brief 2D interpolation between four UV samples
 */
inline int interpolateUVSample2D(const ChromaInterpolation mode,
                                 const int sample1,
                                 const int sample2,
                                 const int sample3,
                                 const int sample4)
{
    if (mode == ChromaInterpolation::Bilinear)
        return ((sample1 + sample2 + sample3 + sample4) + 2) >> 2;
    return sample1;
}

/**
 * @brief Interpolate at 1/8 positions between prev and cur
 */
inline int interpolateUV8Pos(int prev, int cur, const int offsetX8)
{
    if (offsetX8 == 4)
        return (prev + cur + 1) / 2;
    if (offsetX8 == 2)
        return (prev + cur * 3 + 2) / 4;
    if (offsetX8 == 6)
        return (prev * 3 + cur + 2) / 4;
    if (offsetX8 == 1)
        return (prev + cur * 7 + 4) / 8;
    if (offsetX8 == 3)
        return (prev * 3 + cur * 5 + 4) / 8;
    if (offsetX8 == 5)
        return (prev * 5 + cur * 3 + 4) / 8;
    if (offsetX8 == 7)
        return (prev * 7 + cur + 4) / 8;
    return 0;
}

// ============================================================================
// MSE/PSNR Calculation
// ============================================================================

/**
 * @brief Compute MSE between two buffers
 */
template <typename T>
double computeMSE(T ptr, T ptr2, int numPixels)
{
    if (numPixels <= 0)
        return 0.0;

    uint64_t sad = 0;
    for (int i = 0; i < numPixels; i++)
    {
        int diff = (int)ptr[i] - (int)ptr2[i];
        sad += diff * diff;
    }

    return (double)sad / numPixels;
}

/**
 * @brief Format MSE and PSNR as string
 */
inline std::string formatMSEandPSNR(const double mse, const int bps_out)
{
    const auto maxSquared = ((1 << bps_out) - 1) * ((1 << bps_out) - 1);
    const auto psnr = 10 * std::log10(maxSquared / mse);

    std::ostringstream stream;
    stream << std::setw(1) << mse << " (" << std::setw(2) << psnr << ")";

    return stream.str();
}

} // namespace video::yuv::conversion
