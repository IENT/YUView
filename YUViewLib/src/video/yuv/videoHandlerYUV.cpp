/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   In addition, as a special exception, the copyright holders give
 *   permission to link the code of portions of this program with the
 *   OpenSSL library under certain conditions as described in each
 *   individual source file, and distribute linked combinations including
 *   the two.
 *
 *   You must obey the GNU General Public License in all respects for all
 *   of the code used other than OpenSSL. If you modify file(s) with this
 *   exception, you may extend this exception to your version of the
 *   file(s), but you are not obligated to do so. If you do not wish to do
 *   so, delete this exception statement from your version. If you delete
 *   this exception statement from all source files in the program, then
 *   also delete it here.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "videoHandlerYUV.h"

// SRP Refactoring: Include extracted conversion functions
#include "YUVConversionCore.h"
#include "YUVConversionPlanes.h"
#include "YUVConversionRGB.h"
#include "YUVISPBayer.h"
#include "YUVColorConverter.h"
#include "YUVPixelRenderer.h"
#include "YUVDataManager.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPushButton>
#include <QLineEdit>
#include <QStatusBar>
#include <QThread>
#include <QDebug>
#include <QSettings>
#include <QProcess>
#include <ui/widgets/PlaylistTreeWidget.h>
#include <ui/Mainwindow.h>

#include <video/hdr/HDRDetection.h>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTimer>

#include <common/Formatting.h>
#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <common/InfoItemAndData.h>
#include <video/LimitedRangeToFullRange.h>
#include <video/yuv/PixelFormatYUVGuess.h>
#include <video/yuv/videoHandlerYUVCustomFormatDialog.h>

// Import conversion functions into anonymous namespace for compatibility
using namespace video::yuv::conversion;

using namespace std::string_view_literals;

namespace video::yuv
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERYUV_DEBUG_LOADING 0
#if VIDEOHANDLERYUV_DEBUG_LOADING && !NDEBUG
#include <QDebug>
#else
#define DEBUG_YUV(message) ((void)0)
#endif

// Restrict is basically a promise to the compiler that for the scope of the pointer, the target of
// the pointer will only be accessed through that pointer (and pointers copied from it).
#if __STDC__ != 1
#define restrict __restrict /* use implementation __ format */
#else
#ifndef __STDC_VERSION__
#define restrict __restrict /* use implementation __ format */
#else
#if __STDC_VERSION__ < 199901L
#define restrict __restrict /* use implementation __ format */
#else
#/* all ok */
#endif
#endif
#endif

namespace
{

constexpr int kExposureSliderSteps = 1000;
constexpr char kExposureSettingsKey[] = "HDRExposureNits";
constexpr char kLegacyToneMapSettingsKey[] = "ToneMapTargetNits";

struct ExposureRange
{
  int minNits{};
  int maxNits{};
  bool enabled{};
};

ExposureRange getExposureRangeForHDRMode(int hdrModeIndex)
{
  switch (hdrModeIndex)
  {
  case 0: return {350, 10000, true}; // PQ
  case 1: return {350, 1000, true};  // HLG
  default: return {350, 350, false}; // Linear / unsupported
  }
}

int clampExposureNitsForHDRMode(int nits, int hdrModeIndex)
{
  const auto range = getExposureRangeForHDRMode(hdrModeIndex);
  return qBound(range.minNits, nits, range.maxNits);
}

int getSavedExposureNits(QSettings& settings, int hdrModeIndex)
{
  const int fallback = settings.value(kLegacyToneMapSettingsKey, 1000).toInt();
  const int saved = settings.value(kExposureSettingsKey, fallback).toInt();
  return clampExposureNitsForHDRMode(saved, hdrModeIndex);
}

int exposureSliderToNits(int sliderValue, int hdrModeIndex)
{
  const auto range = getExposureRangeForHDRMode(hdrModeIndex);
  if (!range.enabled)
    return range.minNits;

  const double t = static_cast<double>(qBound(0, sliderValue, kExposureSliderSteps)) /
                   static_cast<double>(kExposureSliderSteps);
  const double ratio = static_cast<double>(range.maxNits) / static_cast<double>(range.minNits);
  const double nits =
      static_cast<double>(range.minNits) * std::pow(ratio, t);
  return clampExposureNitsForHDRMode(qRound(nits), hdrModeIndex);
}

int exposureNitsToSlider(int nits, int hdrModeIndex)
{
  const auto range = getExposureRangeForHDRMode(hdrModeIndex);
  if (!range.enabled || range.maxNits <= range.minNits)
    return 0;

  const int clampedNits = clampExposureNitsForHDRMode(nits, hdrModeIndex);
  const double ratio = static_cast<double>(range.maxNits) / static_cast<double>(range.minNits);
  const double normalized = std::log(static_cast<double>(clampedNits) / static_cast<double>(range.minNits)) /
                            std::log(ratio);
  return qBound(0, qRound(normalized * static_cast<double>(kExposureSliderSteps)),
                kExposureSliderSteps);
}

// NOTE: Basic conversion functions (clp_buf, initClippingTable, computeMSE, formatMSEandPSNR, 
// isFullRange) have been moved to YUVConversionCore.h for SRP compliance.
// They are imported via "using namespace video::yuv::conversion;" above.

std::pair<bool, PixelFormatYUV> convertYUVPackedToPlanar(const QByteArray     &sourceBuffer,
                                                         QByteArray           &targetBuffer,
                                                         const Size            curFrameSize,
                                                         const PixelFormatYUV &format)
{
    const auto packing = format.getPackingOrder();

    // Make sure that the target buffer is big enough. It should be as big as the input buffer.
    if (targetBuffer.size() != sourceBuffer.size())
        targetBuffer.resize(sourceBuffer.size());

    const auto w = curFrameSize.width;
    const auto h = curFrameSize.height;

    // Bytes per sample
    const auto bps = (format.getBitsPerSample() > 8) ? 2u : 1u;

    if (format.getSubsampling() == Subsampling::YUV_422)
    {
        // The data is arranged in blocks of 4 samples. How many of these are there?
        const auto nr4Samples = w * h / 2;

        // What are the offsets withing the 4 samples for the components?
        const int oY = (packing == PackingOrder::YUYV || packing == PackingOrder::YVYU) ? 0 : 1;
        const int oU = (packing == PackingOrder::UYVY)   ? 0
                                                         : (packing == PackingOrder::YUYV) ? 1
                                                                                           : (packing == PackingOrder::VYUY) ? 2
                                                                                                                             : 3;
        const int oV = (packing == PackingOrder::VYUY)   ? 0
                                                         : (packing == PackingOrder::YVYU) ? 1
                                                                                           : (packing == PackingOrder::UYVY) ? 2
                                                                                                                             : 3;

        if (format.getBitsPerSample() == 10 && format.isBytePacking())
        {
            // Byte packing in 422 with 10 bit. So for each 2 pixels we have 4 10 bit values which
            // are exactly 5 bytes (40 bits).
            auto fmt        = PixelFormatYUV(Subsampling::YUV_422, 10, PlaneOrder::YUV);
            auto outputSize = fmt.bytesPerFrame(curFrameSize);
            if (targetBuffer.size() < outputSize)
                targetBuffer.resize(outputSize);

            const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();
            unsigned short *restrict dstY     = (unsigned short *)targetBuffer.data();
            unsigned short *restrict dstU     = dstY + w * h;
            unsigned short *restrict dstV     = dstU + w / 2 * h;

            for (unsigned i = 0; i < nr4Samples; i++)
            {
                unsigned short values[4];
                values[0] = (src[0] << 2) + (src[1] >> 6);
                values[1] = ((src[1] & 0x3f) << 4) + (src[2] >> 4);
                values[2] = ((src[2] & 0x0f) << 6) + (src[3] >> 2);
                values[3] = ((src[3] & 0x03) << 8) + src[4];

                *dstY++ = values[oY];
                *dstY++ = values[oY + 2];
                *dstU++ = values[oU];
                *dstV++ = values[oV];

                src += 5;
            }

            return {true, fmt};
        }
        else
        {
            if (bps == 1)
            {
                // One byte per sample.
                const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();
                unsigned char *restrict dstY      = (unsigned char *)targetBuffer.data();
                unsigned char *restrict dstU      = dstY + w * h;
                unsigned char *restrict dstV      = dstU + w / 2 * h;

                for (unsigned i = 0; i < nr4Samples; i++)
                {
                    *dstY++ = src[oY];
                    *dstY++ = src[oY + 2];
                    *dstU++ = src[oU];
                    *dstV++ = src[oV];
                    src += 4; // Goto the next 4 samples
                }
            }
            else
            {
                // Two bytes per sample.
                const unsigned short *restrict src = (unsigned short *)sourceBuffer.data();
                unsigned short *restrict dstY      = (unsigned short *)targetBuffer.data();
                unsigned short *restrict dstU      = dstY + w * h;
                unsigned short *restrict dstV      = dstU + w / 2 * h;

                for (unsigned i = 0; i < nr4Samples; i++)
                {
                    *dstY++ = src[oY];
                    *dstY++ = src[oY + 2];
                    *dstU++ = src[oU];
                    *dstV++ = src[oV];
                    src += 4; // Goto the next 4 samples
                }
            }
        }
    }
    else if (format.getSubsampling() == Subsampling::YUV_444)
    {
        // What are the offsets withing the 3 or 4 bytes per sample?
        const int oY = (packing == PackingOrder::AYUV) ? 1 : (packing == PackingOrder::VUYA) ? 2 : 0;
        const int oU = (packing == PackingOrder::YUV || packing == PackingOrder::YUVA ||
                        packing == PackingOrder::VUYA)
                ? 1
                : 2;
        const int oV = (packing == PackingOrder::YVU)    ? 1
                                                         : (packing == PackingOrder::AYUV) ? 3
                                                                                           : (packing == PackingOrder::VUYA) ? 0
                                                                                                                             : 2;

        // How many samples to the next sample?
        const int offsetNext = (packing == PackingOrder::YUV || packing == PackingOrder::YVU ? 3 : 4);

        if (bps == 1)
        {
            // One byte per sample.
            const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();
            unsigned char *restrict dstY      = (unsigned char *)targetBuffer.data();
            unsigned char *restrict dstU      = dstY + w * h;
            unsigned char *restrict dstV      = dstU + w * h;

            for (unsigned i = 0; i < w * h; i++)
            {
                *dstY++ = src[oY];
                *dstU++ = src[oU];
                *dstV++ = src[oV];
                src += offsetNext; // Goto the next sample
            }
        }
        else
        {
            // Two bytes per sample.
            const unsigned short *restrict src = (unsigned short *)sourceBuffer.data();
            unsigned short *restrict dstY      = (unsigned short *)targetBuffer.data();
            unsigned short *restrict dstU      = dstY + w * h;
            unsigned short *restrict dstV      = dstU + w * h;

            for (unsigned i = 0; i < w * h; i++)
            {
                *dstY++ = src[oY];
                *dstU++ = src[oU];
                *dstV++ = src[oV];
                src += offsetNext; // Goto the next sample
            }
        }
    }
    else
        return {};

    // The output buffer is planar with the same subsampling as before
    auto newFormat = PixelFormatYUV(format.getSubsampling(),
                                    format.getBitsPerSample(),
                                    PlaneOrder::YUV,
                                    format.isBigEndian(),
                                    format.getChromaOffset(),
                                    format.isUVInterleaved());

    return {true, newFormat};
}

std::pair<bool, PixelFormatYUV> convertV210PackedToPlanar(const QByteArray &sourceBuffer,
                                                          QByteArray       &targetBuffer,
                                                          const Size        curFrameSize)
{
    // There are 6 pixels values per 16 bytes in the input.
    // 6 Values (6 Y, 3 U/V) are packed like this (highest to lowest bit, each value is 10 bit):
    // Byte 0-3:   (2 zero bytes), Cr0, Y0, Cb0
    // Byte 4-7:   (2 zero bytes), Y2, Cb1, Y1
    // Byte 8-11:  (2 zero bytes), Cb2, Y3, Cr1
    // Byte 12-15: (2 zero bytes), Y5, Cr2, Y4

    // The output format is 422 10 bit planar
    auto       newFormat        = PixelFormatYUV(Subsampling::YUV_422, 10, PlaneOrder::YUV);
    const auto bytesPerOutFrame = newFormat.bytesPerFrame(curFrameSize);
    if (targetBuffer.size() < bytesPerOutFrame)
        targetBuffer.resize(bytesPerOutFrame);

    const auto w = curFrameSize.width;
    const auto h = curFrameSize.height;

    auto widthRoundUp = (((w + 48 - 1) / 48) * 48);
    auto strideIn     = widthRoundUp / 6 * 16;

    const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();
    unsigned short *restrict dstY     = (unsigned short *)targetBuffer.data();
    unsigned short *restrict dstU     = dstY + w * h;
    unsigned short *restrict dstV     = dstU + w / 2 * h;

    for (unsigned y = 0; y < h; y++)
    {
        for (auto [xIn, xOutY, xOutUV] = std::tuple{0u, 0u, 0u}; xOutY < w;
             xOutY += 6, xOutUV += 3, xIn += 16)
        {
            auto           xw0 = xIn;
            unsigned short Cb0 = src[xw0] + ((src[xw0 + 1] & 0x03) << 8);
            unsigned short Y0  = ((src[xw0 + 1] >> 2) & 0x3f) + ((src[xw0 + 2] & 0x0f) << 6);
            unsigned short Cr0 = (src[xw0 + 2] >> 4) + ((src[xw0 + 3] & 0x3f) << 4);

            auto           xw1 = xIn + 4;
            unsigned short Y1  = src[xw1] + ((src[xw1 + 1] & 0x03) << 8);
            unsigned short Cb1 = ((src[xw1 + 1] >> 2) & 0x3f) + ((src[xw1 + 2] & 0x0f) << 6);
            unsigned short Y2  = (src[xw1 + 2] >> 4) + ((src[xw1 + 3] & 0x3f) << 4);

            auto           xw2 = xIn + 8;
            unsigned short Cr1 = src[xw2] + ((src[xw2 + 1] & 0x03) << 8);
            unsigned short Y3  = ((src[xw2 + 1] >> 2) & 0x3f) + ((src[xw2 + 2] & 0x0f) << 6);
            unsigned short Cb2 = (src[xw2 + 2] >> 4) + ((src[xw2 + 3] & 0x3f) << 4);

            auto           xw3 = xIn + 12;
            unsigned short Y4  = src[xw3] + ((src[xw3 + 1] & 0x03) << 8);
            unsigned short Cr2 = ((src[xw3 + 1] >> 2) & 0x3f) + ((src[xw3 + 2] & 0x0f) << 6);
            unsigned short Y5  = (src[xw3 + 2] >> 4) + ((src[xw3 + 3] & 0x3f) << 4);

            dstY[xOutY]     = Y0;
            dstY[xOutY + 1] = Y1;
            dstU[xOutUV]    = Cb0;
            dstV[xOutUV]    = Cr0;

            if (xOutY + 2 < w)
            {
                dstY[xOutY + 2]  = Y2;
                dstY[xOutY + 3]  = Y3;
                dstU[xOutUV + 1] = Cb1;
                dstV[xOutUV + 1] = Cr1;

                if (xOutY + 4 < w)
                {
                    dstY[xOutY + 4]  = Y4;
                    dstY[xOutY + 5]  = Y5;
                    dstU[xOutUV + 2] = Cb2;
                    dstV[xOutUV + 2] = Cr2;
                }
            }
        }
        src += strideIn;
        dstY += w;
        dstU += w / 2;
        dstV += w / 2;
    }

    return {true, newFormat};
}

yuv_t getPixelValueV210(const QByteArray &sourceBuffer,
                        const Size       &curFrameSize,
                        const QPoint     &pixelPos)
{
    auto widthRoundUp = (((curFrameSize.width + 48 - 1) / 48) * 48);
    auto strideIn     = widthRoundUp / 6 * 16;

    auto startInBuffer = (unsigned(pixelPos.y()) * strideIn) + unsigned(pixelPos.x()) / 6 * 16;

    const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();

    yuv_t ret;
    auto  xSub = unsigned(pixelPos.x()) % 6;
    if (xSub == 0)
        ret.Y = ((src[startInBuffer + 1] >> 2) & 0x3f) + ((src[startInBuffer + 2] & 0x0f) << 6);
    else if (xSub == 1)
        ret.Y = src[startInBuffer + 4] + ((src[startInBuffer + 4 + 1] & 0x03) << 8);
    else if (xSub == 2)
        ret.Y = (src[startInBuffer + 4 + 2] >> 4) + ((src[startInBuffer + 4 + 3] & 0x3f) << 4);
    else if (xSub == 3)
        ret.Y = ((src[startInBuffer + 8 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 8 + 2] & 0x0f) << 6);
    else if (xSub == 4)
        ret.Y = src[startInBuffer + 12] + ((src[startInBuffer + 12 + 1] & 0x03) << 8);
    else
        ret.Y = (src[startInBuffer + 12 + 2] >> 4) + ((src[startInBuffer + 12 + 3] & 0x3f) << 4);

    if (xSub == 0 || xSub == 1)
    {
        ret.U = src[startInBuffer] + ((src[startInBuffer + 1] & 0x03) << 8);
        ret.V = (src[startInBuffer + 2] >> 4) + ((src[startInBuffer + 3] & 0x3f) << 4);
    }
    else if (xSub == 2 || xSub == 3)
    {
        ret.U = ((src[startInBuffer + 4 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 4 + 2] & 0x0f) << 6);
        ret.V = src[startInBuffer + 8] + ((src[startInBuffer + 8 + 1] & 0x03) << 8);
    }
    else
    {
        ret.U = (src[startInBuffer + 8 + 2] >> 4) + ((src[startInBuffer + 8 + 3] & 0x3f) << 4);
        ret.V =
                ((src[startInBuffer + 12 + 1] >> 2) & 0x3f) + ((src[startInBuffer + 12 + 2] & 0x0f) << 6);
    }

    return ret;
}

// This is a specialized function that can convert 8 - bit YUV 4 : 2 : 0 to RGB888 using
// NearestNeighborInterpolation. The chroma must be 0 in x direction and 1 in y direction. No
// yuvMath is supported.
// TODO: Correct the chroma subsampling offset.
template <int bitDepth>
bool convertYUV420ToRGB(const QByteArray         &sourceBuffer,
                        unsigned char            *targetBuffer,
                        const Size               &size,
                        const PixelFormatYUV     &format,
                        const ConversionSettings &conversionSettings)
{
    typedef typename std::conditional<bitDepth == 8, uint8_t *, uint16_t *>::type InValueType;
    static_assert(bitDepth == 8 || bitDepth == 10);
    constexpr auto rightShift = (bitDepth == 8) ? 0 : 2;

    const auto frameWidth  = size.width;
    const auto frameHeight = size.height;

    // For 4:2:0, w and h must be dividible by 2
    assert(frameWidth % 2 == 0 && frameHeight % 2 == 0);

    int componentLenghtY  = frameWidth * frameHeight;
    int componentLengthUV = componentLenghtY >> 2;
    Q_ASSERT(sourceBuffer.size() >= componentLenghtY + componentLengthUV +
             componentLengthUV); // YUV 420 must be (at least) 1.5*Y-area

    // Use the clip buffer from YUVConversionCore.h (already initialized)
    unsigned char *clip_buf_local = getClipBuffer();

    unsigned char *restrict dst = targetBuffer;

    // Get/set the parameters used for YUV -> RGB conversion
    const bool fullRange = isFullRange(conversionSettings.colorConversion);
    const int  yOffset   = (fullRange ? 0 : 16);
    const int  cZero     = 128;
    int        RGBConv[5];
    getColorConversionCoefficients(conversionSettings.colorConversion, RGBConv);

    // Get pointers to the source and the output array
    const bool uPplaneFirst =
            (format.getPlaneOrder() == PlaneOrder::YUV ||
             format.getPlaneOrder() == PlaneOrder::YUVA); // Is the U plane the first or the second?
    const auto *restrict srcY = InValueType(sourceBuffer.data());
    const auto *restrict srcU =
            uPplaneFirst ? srcY + componentLenghtY : srcY + componentLenghtY + componentLengthUV;
    const auto *restrict srcV =
            uPplaneFirst ? srcY + componentLenghtY + componentLengthUV : srcY + componentLenghtY;

    for (unsigned yh = 0; yh < frameHeight / 2; yh++)
    {
        // Process two lines at once, always 4 RGB values at a time (they have the same U/V components)

        int dstAddr1  = yh * 2 * frameWidth * 4;       // The RGB output address of line yh*2
        int dstAddr2  = (yh * 2 + 1) * frameWidth * 4; // The RGB output address of line yh*2+1
        int srcAddrY1 = yh * 2 * frameWidth;           // The Y source address of line yh*2
        int srcAddrY2 = (yh * 2 + 1) * frameWidth;     // The Y source address of line yh*2+1
        int srcAddrUV = yh * frameWidth / 2; // The UV source address of both lines (UV are identical)

        for (unsigned xh = 0, x = 0; xh < frameWidth / 2; xh++, x += 2)
        {
            // Process four pixels (the ones for which U/V are valid

            // Load UV and pre-multiply
            const int U_tmp_G = (((int)srcU[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[2];
            const int U_tmp_B = (((int)srcU[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[4];
            const int V_tmp_R = (((int)srcV[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[1];
            const int V_tmp_G = (((int)srcV[srcAddrUV + xh] >> rightShift) - cZero) * RGBConv[3];

            // Pixel top left
            {
                const int Y_tmp = (((int)srcY[srcAddrY1 + x] >> rightShift) - yOffset) * RGBConv[0];

                const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
                const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
                const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

                dst[dstAddr1]     = clip_buf_local[B_tmp];
                dst[dstAddr1 + 1] = clip_buf_local[G_tmp];
                dst[dstAddr1 + 2] = clip_buf_local[R_tmp];
                dst[dstAddr1 + 3] = 255;
                dstAddr1 += 4;
            }
            // Pixel top right
            {
                const int Y_tmp = (((int)srcY[srcAddrY1 + x + 1] >> rightShift) - yOffset) * RGBConv[0];

                const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
                const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
                const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

                dst[dstAddr1]     = clip_buf_local[B_tmp];
                dst[dstAddr1 + 1] = clip_buf_local[G_tmp];
                dst[dstAddr1 + 2] = clip_buf_local[R_tmp];
                dst[dstAddr1 + 3] = 255;
                dstAddr1 += 4;
            }
            // Pixel bottom left
            {
                const int Y_tmp = (((int)srcY[srcAddrY2 + x] >> rightShift) - yOffset) * RGBConv[0];

                const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
                const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
                const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

                dst[dstAddr2]     = clip_buf_local[B_tmp];
                dst[dstAddr2 + 1] = clip_buf_local[G_tmp];
                dst[dstAddr2 + 2] = clip_buf_local[R_tmp];
                dst[dstAddr2 + 3] = 255;
                dstAddr2 += 4;
            }
            // Pixel bottom right
            {
                const int Y_tmp = (((int)srcY[srcAddrY2 + x + 1] >> rightShift) - yOffset) * RGBConv[0];

                const int R_tmp = (Y_tmp + V_tmp_R) >> 16;
                const int G_tmp = (Y_tmp + U_tmp_G + V_tmp_G) >> 16;
                const int B_tmp = (Y_tmp + U_tmp_B) >> 16;

                dst[dstAddr2]     = clip_buf_local[B_tmp];
                dst[dstAddr2 + 1] = clip_buf_local[G_tmp];
                dst[dstAddr2 + 2] = clip_buf_local[R_tmp];
                dst[dstAddr2 + 3] = 255;
                dstAddr2 += 4;
            }
        }
    }

    return true;
}


// NOTE: clip8Bit, transformYUV, convertYUVToRGB8Bit, getValueFromSource, setValueInBuffer
// have been moved to YUVConversionCore.h for SRP compliance.
// They are imported via "using namespace video::yuv::conversion;" above.

// NOTE: YUVPlaneToRGBMonochrome_444/422/420/440/410/411, interpolateUVSample/Q/2D, interpolateUV8Pos
// have been moved to YUVConversionPlanes.h for SRP compliance.

// Re-sample the chroma component so that the chroma samples and the luma samples are aligned after
// this operation.
inline void UVPlaneResamplingChromaOffset(const PixelFormatYUV format,
                                          const int            w,
                                          const int            h,
                                          const unsigned char *restrict srcU,
                                          const unsigned char *restrict srcV,
                                          const int inValSkip,
                                          unsigned char *restrict dstU,
                                          unsigned char *restrict dstV)
{
    // We can perform linear interpolation for 7 positions (6 in between) two pixels.
    // Which of these position is needed depends on the chromaOffset and the subsampling.
    const int possibleValsX = getMaxPossibleChromaOffsetValues(true, format.getSubsampling());
    const int possibleValsY = getMaxPossibleChromaOffsetValues(false, format.getSubsampling());
    const int offsetX8      = (possibleValsX == 1)   ? format.getChromaOffset().x * 4
                                                     : (possibleValsX == 3) ? format.getChromaOffset().x * 2
                                                                            : format.getChromaOffset().x;
    const int offsetY8      = (possibleValsY == 1)   ? format.getChromaOffset().y * 4
                                                     : (possibleValsY == 3) ? format.getChromaOffset().y * 2
                                                                            : format.getChromaOffset().y;

    // The format to use for input/output
    const bool bigEndian = format.isBigEndian();
    const int  bps       = format.getBitsPerSample();

    const int stride = bps > 8 ? w * 2 : w;
    if (offsetX8 != 0)
    {
        // Perform horizontal re-sampling
        for (int y = 0; y < h; y++)
        {
            // On the left side, there is no previous sample, so the first value is never changed.
            const int srcIdx = y * stride * inValSkip;
            int       prevU  = getValueFromSource(srcU, srcIdx, bps, bigEndian);
            int       prevV  = getValueFromSource(srcV, srcIdx, bps, bigEndian);
            setValueInBuffer(dstU, prevU, y * stride, bps, bigEndian);
            setValueInBuffer(dstV, prevV, y * stride, bps, bigEndian);

            for (int x = 0; x < w - 1; x++)
            {
                // Calculate the new current value using the previous and the current value
                const int srcIdxInLine = srcIdx + (x + 1) * inValSkip;
                int       curU         = getValueFromSource(srcU, srcIdxInLine, bps, bigEndian);
                int       curV         = getValueFromSource(srcV, srcIdxInLine, bps, bigEndian);

                // Perform interpolation and save the value for the current UV value. Goto next value.
                int newU = interpolateUV8Pos(prevU, curU, offsetX8);
                int newV = interpolateUV8Pos(prevV, curV, offsetX8);
                setValueInBuffer(dstU, newU, y * stride + x, bps, bigEndian);
                setValueInBuffer(dstV, newV, y * stride + x, bps, bigEndian);

                prevU = curU;
                prevV = curV;
            }
        }
    }

    // For the second step, use the filtered values (or the source if no filtering was applied)
    const unsigned char *srcUStep2    = (offsetX8 == 0) ? srcU : dstU;
    const unsigned char *srcVStep2    = (offsetX8 == 0) ? srcV : dstV;
    const int            valSkipStep2 = (offsetX8 == 0) ? inValSkip : 1;

    if (offsetY8 != 0)
    {
        // Perform vertical re-sampling. It works exactly like horizontal up-sampling but x and y are
        // switched.
        for (int x = 0; x < w; x++)
        {
            // On the top, there is no previous sample, so the first value is never changed.
            int prevU = getValueFromSource(srcUStep2, x * valSkipStep2, bps, bigEndian);
            int prevV = getValueFromSource(srcVStep2, x * valSkipStep2, bps, bigEndian);
            setValueInBuffer(dstU, prevU, x, bps, bigEndian);
            setValueInBuffer(dstV, prevV, x, bps, bigEndian);

            for (int y = 0; y < h - 1; y++)
            {
                // Calculate the new current value using the previous and the current value
                const int srcIdx = (y + 1) * w + x;
                int       curU   = getValueFromSource(srcUStep2, srcIdx * valSkipStep2, bps, bigEndian);
                int       curV   = getValueFromSource(srcVStep2, srcIdx * valSkipStep2, bps, bigEndian);

                // Perform interpolation and save the value for the current UV value. Goto next value.
                int newU = interpolateUV8Pos(prevU, curU, offsetY8);
                int newV = interpolateUV8Pos(prevV, curV, offsetY8);
                setValueInBuffer(dstU, newU, srcIdx, bps, bigEndian);
                setValueInBuffer(dstV, newV, srcIdx, bps, bigEndian);

                prevU = curU;
                prevV = curV;
            }
        }
    }
}

// NOTE: YUVPlaneToRGB_444/422/440/420/410/411 functions have been moved to 
// YUVConversionPlanes.h for SRP compliance. They are imported via 
// "using namespace video::yuv::conversion;" at the top of this file.

bool convertYUVPlanarToRGB(const QByteArray         &sourceBuffer,
                           uchar                    *targetBuffer,
                           const Size                curFrameSize,
                           const PixelFormatYUV     &sourceBufferFormat,
                           const ConversionSettings &conversionSettings)
{
    // These are constant for the runtime of this function. This way, the compiler can optimize the
    // hell out of this function.
    const auto format        = sourceBufferFormat;
    const auto interpolation = conversionSettings.chromaInterpolation;
    const auto component     = conversionSettings.componentDisplayMode;
    const auto conversion    = conversionSettings.colorConversion;
    const auto w             = curFrameSize.width;
    const auto h             = curFrameSize.height;

    // Do we have to apply YUV math?
    const auto mathY = conversionSettings.mathParameters.at(Component::Luma);
    const auto mathC = conversionSettings.mathParameters.at(Component::Chroma);
    // const auto applyMathLuma   = mathY.mathRequired();
    // const auto applyMathChroma = mathC.mathRequired();

    const auto bps       = format.getBitsPerSample();
    const bool fullRange = isFullRange(conversionSettings.colorConversion);
    // const auto yOffset = 16<<(bps-8);
    // const auto cZero = 128<<(bps-8);
    const auto inputMax = (1 << bps) - 1;

    // The luma component has full resolution. The size of each chroma components depends on the
    // subsampling.
    const auto componentSizeLuma = (w * h);
    const auto componentSizeChroma =
            (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

    // How many bytes are in each component?
    const auto nrBytesLumaPlane   = (bps > 8) ? componentSizeLuma * 2 : componentSizeLuma;
    const auto nrBytesChromaPlane = (bps > 8) ? componentSizeChroma * 2 : componentSizeChroma;

    // If the U and V (and A if present) components are interlevaed, we have to skip every nth value
    // in the input when reading U and V
    const auto inputValSkip = format.isUVInterleaved()
            ? ((format.getPlaneOrder() == PlaneOrder::YUV ||
                format.getPlaneOrder() == PlaneOrder::YVU)
               ? 2
               : 3)
            : 1;

    // A pointer to the output
    unsigned char *restrict dst = targetBuffer;

    if (component != ComponentDisplayMode::DisplayAll ||
            format.getSubsampling() == Subsampling::YUV_400)
    {
        // We only display (or there is only) one of the color components (possibly with YUV math)
        if (component == ComponentDisplayMode::DisplayY ||
                format.getSubsampling() == Subsampling::YUV_400)
        {
            // Luma only. The chroma subsampling does not matter.
            const unsigned char *restrict srcY = (unsigned char *)sourceBuffer.data();
            if (format.getSubsampling() == Subsampling::YUV_400 &&
                conversionSettings.ispModeEnabled)
            {
                // ISP Bayer false-color preview for YUV400 raw / mono sensor data.
                YUVPlaneToRGBBayerFalseColor(static_cast<int>(w),
                                             static_cast<int>(h),
                                             mathY,
                                             srcY,
                                             dst,
                                             inputMax,
                                             bps,
                                             format.isBigEndian(),
                                             conversionSettings.bayerPattern,
                                             conversionSettings.ispCellSize);
            }
            else
            {
                YUVPlaneToRGBMonochrome_444(componentSizeLuma,
                                            mathY,
                                            srcY,
                                            dst,
                                            inputMax,
                                            bps,
                                            format.isBigEndian(),
                                            1,
                                            fullRange);
            }
        }
        else
        {
            // Display only the U or V component
            bool firstComponent = (((format.getPlaneOrder() == PlaneOrder::YUV ||
                                     format.getPlaneOrder() == PlaneOrder::YUVA) &&
                                    component == ComponentDisplayMode::DisplayCb) ||
                                   ((format.getPlaneOrder() == PlaneOrder::YVU ||
                                     format.getPlaneOrder() == PlaneOrder::YVUA) &&
                                    component == ComponentDisplayMode::DisplayCr));

            int srcOffset = nrBytesLumaPlane;
            if (!firstComponent)
            {
                if (format.isUVInterleaved())
                    srcOffset += (bps > 8) ? 2 : 1;
                else
                    srcOffset += nrBytesChromaPlane;
            }

            const unsigned char *restrict srcC = (unsigned char *)sourceBuffer.data() + srcOffset;
            if (format.getSubsampling() == Subsampling::YUV_444)
                YUVPlaneToRGBMonochrome_444(componentSizeChroma,
                                            mathC,
                                            srcC,
                                            dst,
                                            inputMax,
                                            bps,
                                            format.isBigEndian(),
                                            inputValSkip,
                                            fullRange);
            else if (format.getSubsampling() == Subsampling::YUV_422)
                YUVPlaneToRGBMonochrome_422(componentSizeChroma,
                                            mathC,
                                            srcC,
                                            dst,
                                            inputMax,
                                            bps,
                                            format.isBigEndian(),
                                            inputValSkip,
                                            fullRange);
            else if (format.getSubsampling() == Subsampling::YUV_420)
                YUVPlaneToRGBMonochrome_420(
                            w, h, mathC, srcC, dst, inputMax, bps, format.isBigEndian(), inputValSkip, fullRange);
            else if (format.getSubsampling() == Subsampling::YUV_440)
                YUVPlaneToRGBMonochrome_440(
                            w, h, mathC, srcC, dst, inputMax, bps, format.isBigEndian(), inputValSkip, fullRange);
            else if (format.getSubsampling() == Subsampling::YUV_410)
                YUVPlaneToRGBMonochrome_410(
                            w, h, mathC, srcC, dst, inputMax, bps, format.isBigEndian(), inputValSkip, fullRange);
            else if (format.getSubsampling() == Subsampling::YUV_411)
                YUVPlaneToRGBMonochrome_411(componentSizeChroma,
                                            mathC,
                                            srcC,
                                            dst,
                                            inputMax,
                                            bps,
                                            format.isBigEndian(),
                                            inputValSkip,
                                            fullRange);
            else
                return false;
        }
    }
    else
    {
        // Is the U plane the first or the second?
        const bool uPlaneFirst =
                (format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YUVA);

        // In case the U and V (and A if present) components are interleaved, the skip to the next plane
        // is just 1 (or 2) bytes
        int nrBytesToNextChromaPlane = nrBytesChromaPlane;
        if (format.isUVInterleaved())
            nrBytesToNextChromaPlane = (bps > 8) ? 2 : 1;

        // Get/set the parameters used for YUV -> RGB conversion
        int RGBConv[5];
        getColorConversionCoefficients(conversion, RGBConv);

        // We are displaying all components, so we have to perform conversion to RGB (possibly including
        // interpolation and YUV math)
        if (format.getSubsampling() != Subsampling::YUV_400 &&
                (format.getChromaOffset().x != 0 || format.getChromaOffset().y != 0) &&
                interpolation != ChromaInterpolation::NearestNeighbor)
        {
            // If there is a chroma offset, we must resample the chroma components before we convert them
            // to RGB. If so, the resampled chroma values are saved in these arrays. We only ignore the
            // chroma offset for other interpolations then nearest neighbor.
            QByteArray uvPlaneChromaResampled[2];
            uvPlaneChromaResampled[0].resize(nrBytesChromaPlane);
            uvPlaneChromaResampled[1].resize(nrBytesChromaPlane);

            // We have to perform pre-filtering for the U and V positions, because there is an offset
            // between the pixel positions of Y and U/V
            unsigned char *restrict dstU = (unsigned char *)uvPlaneChromaResampled[0].data();
            unsigned char *restrict dstV = (unsigned char *)uvPlaneChromaResampled[1].data();

            unsigned char *restrict srcY = (unsigned char *)sourceBuffer.data();
            unsigned char *restrict srcU = uPlaneFirst
                    ? srcY + nrBytesLumaPlane
                    : srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane;
            unsigned char *restrict srcV = uPlaneFirst
                    ? srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane
                    : srcY + nrBytesLumaPlane;

            UVPlaneResamplingChromaOffset(format,
                                          w / format.getSubsamplingHor(),
                                          h / format.getSubsamplingVer(),
                                          srcU,
                                          srcV,
                                          inputValSkip,
                                          dstU,
                                          dstV);

            if (format.getSubsampling() == Subsampling::YUV_444)
                YUVPlaneToRGB_444(componentSizeLuma,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else if (format.getSubsampling() == Subsampling::YUV_422)
                YUVPlaneToRGB_422(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else if (format.getSubsampling() == Subsampling::YUV_420)
                YUVPlaneToRGB_420(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else if (format.getSubsampling() == Subsampling::YUV_440)
                YUVPlaneToRGB_440(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else if (format.getSubsampling() == Subsampling::YUV_410)
                YUVPlaneToRGB_410(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else if (format.getSubsampling() == Subsampling::YUV_411)
                YUVPlaneToRGB_411(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  dstU,
                                  dstV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  1);
            else
                return false;
        }
        else
        {
            // Get the pointers to the source planes (8 bit per sample)
            const unsigned char *restrict srcY = (unsigned char *)sourceBuffer.data();
            const unsigned char *restrict srcU = uPlaneFirst
                    ? srcY + nrBytesLumaPlane
                    : srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane;
            const unsigned char *restrict srcV = uPlaneFirst
                    ? srcY + nrBytesLumaPlane + nrBytesToNextChromaPlane
                    : srcY + nrBytesLumaPlane;

            if (format.getSubsampling() == Subsampling::YUV_444)
                YUVPlaneToRGB_444(componentSizeLuma,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_422)
                YUVPlaneToRGB_422(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_420)
                YUVPlaneToRGB_420(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_440)
                YUVPlaneToRGB_440(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_410)
                YUVPlaneToRGB_410(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_411)
                YUVPlaneToRGB_411(w,
                                  h,
                                  mathY,
                                  mathC,
                                  srcY,
                                  srcU,
                                  srcV,
                                  dst,
                                  RGBConv,
                                  fullRange,
                                  inputMax,
                                  interpolation,
                                  bps,
                                  format.isBigEndian(),
                                  inputValSkip);
            else if (format.getSubsampling() == Subsampling::YUV_400)
                YUVPlaneToRGBMonochrome_444(
                            componentSizeLuma, mathY, srcY, dst, fullRange, inputMax, bps, format.isBigEndian(), 1);
            else
                return false;
        }
    }

    return true;
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to image (RGB-888), using
// the buffer tmpRGBBuffer for intermediate RGB values.
void convertYUVToImage(const QByteArray         &sourceBuffer,
                       QImage                   &outputImage,
                       const PixelFormatYUV     &yuvFormat,
                       const Size               &curFrameSize,
                       const ConversionSettings &conversionSettings,
                       bool                      enable10BitDisplay = false)
{
    if (!yuvFormat.canConvertToRGB(curFrameSize) || sourceBuffer.isEmpty())
    {
        outputImage = QImage();
        return;
    }



    // Create the output image in the right format.
    // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA
    // (each 8 bit). Internally, this is how QImage allocates the number of bytes per line (with depth
    // = 32): const int bytes_per_line = ((width * depth + 31) >> 5) << 2; // bytes per scanline (must
    // be multiple of 4)
    auto qFrameSize = QSize(int(curFrameSize.width), int(curFrameSize.height));


    // Standard 8-bit display path
    auto platformImageFormat = functionsGui::platformImageFormat(yuvFormat.hasAlpha());
    if (is_Q_OS_WIN || is_Q_OS_MAC)
        outputImage = QImage(qFrameSize, platformImageFormat);
    else if (is_Q_OS_LINUX)
    {
        if (platformImageFormat == QImage::Format_ARGB32_Premultiplied ||
                platformImageFormat == QImage::Format_ARGB32)
            outputImage = QImage(qFrameSize, platformImageFormat);
        else
            outputImage = QImage(qFrameSize, QImage::Format_RGB32);
    }


    // Check the image buffer size before we write to it
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    assert(functions::clipToUnsigned(outputImage.byteCount()) >=
           curFrameSize.width * curFrameSize.height * 4);
#else
    assert(functions::clipToUnsigned(outputImage.sizeInBytes()) >=
           curFrameSize.width * curFrameSize.height * 4);
#endif

    auto convOK = false;
    if (yuvFormat.isPlanar())
    {
        if ((yuvFormat.getBitsPerSample() == 8 || yuvFormat.getBitsPerSample() == 10) &&
                yuvFormat.getSubsampling() == Subsampling::YUV_420 &&
                conversionSettings.chromaInterpolation == ChromaInterpolation::NearestNeighbor &&
                yuvFormat.getChromaOffset().x == 0 && yuvFormat.getChromaOffset().y == 1 &&
                conversionSettings.componentDisplayMode == ComponentDisplayMode::DisplayAll &&
                !yuvFormat.isUVInterleaved() &&
                !conversionSettings.mathParameters.at(Component::Luma).mathRequired() &&
                !conversionSettings.mathParameters.at(Component::Chroma).mathRequired())
            // 8/10 bit 4:2:0, nearest neighbor, chroma offset (0,1) (the default for 4:2:0), all components
            // displayed and no yuv math. We can use a specialized function for this.
        {
            if (yuvFormat.getBitsPerSample() == 8)
                convOK = convertYUV420ToRGB<8>(
                            sourceBuffer, outputImage.bits(), curFrameSize, yuvFormat, conversionSettings);
            else if (yuvFormat.getBitsPerSample() == 10)
                convOK = convertYUV420ToRGB<10>(
                            sourceBuffer, outputImage.bits(), curFrameSize, yuvFormat, conversionSettings);
        }
        else
            convOK = convertYUVPlanarToRGB(
                        sourceBuffer, outputImage.bits(), curFrameSize, yuvFormat, conversionSettings);
    }
    else
    {
        // Convert to a planar format first
        QByteArray tmpPlanarYUVSource;
        // This is the current format of the buffer. The conversion function will change this.
        PixelFormatYUV newPixelFormat;

        if (auto predefinedFormat = yuvFormat.getPredefinedFormat())
        {
            if (*predefinedFormat == PredefinedPixelFormat::V210)
                std::tie(convOK, newPixelFormat) =
                        convertV210PackedToPlanar(sourceBuffer, tmpPlanarYUVSource, curFrameSize);
            else
                convOK = false;
        }
        else
            std::tie(convOK, newPixelFormat) =
                    convertYUVPackedToPlanar(sourceBuffer, tmpPlanarYUVSource, curFrameSize, yuvFormat);

        if (convOK)
            convOK &= convertYUVPlanarToRGB(
                        tmpPlanarYUVSource, outputImage.bits(), curFrameSize, newPixelFormat, conversionSettings);
    }

    assert(convOK);

    if (is_Q_OS_LINUX)
    {
        // On linux, we may have to convert the image to the platform image format if it is not one of
        // the RGBA formats.
        auto format = functionsGui::platformImageFormat(yuvFormat.hasAlpha());
        if (format != QImage::Format_ARGB32_Premultiplied && format != QImage::Format_ARGB32 &&
                format != QImage::Format_RGB32)
            outputImage = outputImage.convertToFormat(format);
    }

    DEBUG_YUV("videoHandlerYUV::convertYUVToImage Done");
}

} // namespace

std::vector<PixelFormatYUV> videoHandlerYUV::formatPresetList = {
    PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV),
    PixelFormatYUV(Subsampling::YUV_420, 10, PlaneOrder::YUV),
    PixelFormatYUV(Subsampling::YUV_422, 8, PlaneOrder::YUV),
    PixelFormatYUV(Subsampling::YUV_444, 8, PlaneOrder::YUV),
    PixelFormatYUV(PredefinedPixelFormat::V210)};

videoHandlerYUV::videoHandlerYUV() : videoHandler()
{
    // Set the default YUV transformation parameters.
    this->conversionSettings.mathParameters[Component::Luma]   = MathParameters(1, 125, false);
    this->conversionSettings.mathParameters[Component::Chroma] = MathParameters(1, 128, false);

    // Color matrix defaults: HDR -> BT.2020 Limited; SDR -> BT.709 Limited (SDR may use usage prefs)
    QSettings settings;
    if (settings.value("Enable10BitDisplay", false).toBool()) {
        this->conversionSettings.colorConversion = ColorConversion::BT2020_LimitedRange;
    } else {
        settings.beginGroup("YUV/ColorConversionUsage");

        int maxCount = 0;
        ColorConversion mostUsedConversion = ColorConversion::BT709_LimitedRange;

        for (size_t i = 0; i < ColorConversionMapper.size(); ++i) {
            auto conversion = ColorConversionMapper[i].first;
            auto name = ColorConversionMapper[i].second;
            int count = settings.value(QString::fromUtf8(name.data(), name.size()), 0).toInt();

            if (count > maxCount) {
                maxCount = count;
                mostUsedConversion = conversion;
            }
        }

        settings.endGroup();

        // Apply usage preference only for SDR so an HDR BT.2020 preference is not reused
        if (maxCount >= 2) {
            this->conversionSettings.colorConversion = mostUsedConversion;
            auto name = ColorConversionMapper.getName(mostUsedConversion);
            Q_UNUSED(name);
        }
    }

    // If we know nothing about the YUV format, assume YUV 4:2:0 8 bit planar by default.
    const auto defaultPixelFormat = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV);
    this->srcPixelFormat          = defaultPixelFormat;

    // Initialize component managers (use shared HDR manager instance)
    m_hdrRenderingManager = HDRRenderingManager::instance();

    // Connect HDR signals
    connect(m_hdrRenderingManager, &HDRRenderingManager::hdrRenderingStateChangedWindow,
            this, &videoHandlerYUV::onHDRRenderingStateChangedWindow);

    connect(m_hdrRenderingManager, &HDRRenderingManager::hdrDetectionFailed,
            this, &videoHandlerYUV::onHDRDetectionFailed);

    // HDR detection is triggered when user enables HDR via checkbox
}

videoHandlerYUV::~videoHandlerYUV()
{
    DEBUG_YUV("videoHandlerYUV destruction");

    // Clean up component managers (HDR manager is a singleton and owned by qApp)
    m_hdrRenderingManager = nullptr;
}

unsigned videoHandlerYUV::getCachingFrameSize() const
{
    /**
     * @brief Delegate cache-size accounting to YUVDataManager.
     *
     * Raw YUV mode stores planar bytes (~25% smaller than RGBA for 4:2:0).
     */
    return static_cast<unsigned>(
        YUVDataManager::getCachingFrameSize(srcPixelFormat, frameSize, shouldUseRawYUVCache()));
}

/**
 * @brief Determine whether raw YUV caching should be used for this handler.
 *
 * Delegates format/mode policy to YUVDataManager and reads the HDR toggle from
 * the cached QSettings snapshot to avoid per-frame disk I/O.
 */
bool videoHandlerYUV::shouldUseRawYUVCache() const
{
    if (!m_settingsCacheValid.load(std::memory_order_relaxed))
    {
        QSettings settings;
        m_enable10BitDisplayCached.store(
                    settings.value("Enable10BitDisplay", false).toBool(),
                    std::memory_order_relaxed);
        m_settingsCacheValid.store(true, std::memory_order_relaxed);
    }

    return YUVDataManager::shouldUseRawYUVCache(
        srcPixelFormat,
        m_enable10BitDisplayCached.load(std::memory_order_relaxed));
}

/**
 * @brief Return true when the active source format is eligible for GPU YUV upload.
 */
bool videoHandlerYUV::canUseGPUYUVHDRPath() const
{
    if (srcPixelFormat.getBitsPerSample() != 10 || !srcPixelFormat.isPlanar())
        return false;

    const auto subsampling = srcPixelFormat.getSubsampling();
    return subsampling == Subsampling::YUV_420 ||
           subsampling == Subsampling::YUV_422 ||
           subsampling == Subsampling::YUV_444;
}

/**
 * @brief Resolve raw YUV for HDR using cache-take → live snapshot → load priority.
 */
bool videoHandlerYUV::resolveRawYUVForHDR(int frameIndex, ResolvedHDRYUVFrame &out)
{
    out.yuvData.clear();
    out.ownership = HDRYUVOwnership::Shared;

    if (frameIndex < 0)
        return false;

    if (takeRawYUVFromCache(frameIndex, out.yuvData))
    {
        out.ownership = HDRYUVOwnership::Exclusive;
        return !out.yuvData.isEmpty();
    }

    if (getCurrentRawFrameSnapshot(frameIndex, out.yuvData))
        return !out.yuvData.isEmpty();

    if (loadRawYUVData(frameIndex) && getCurrentRawFrameSnapshot(frameIndex, out.yuvData))
        return !out.yuvData.isEmpty();

    return false;
}

/**
 * @brief Unified HDR GPU push: move on exclusive ownership, copy on shared buffers.
 */
bool videoHandlerYUV::pushFrameToHDR(int frameIndex)
{
    if (!m_hdrRenderingManager || !m_hdrRenderingManager->isHDRRenderingActive() || frameIndex < 0)
        return false;

    if (!canUseGPUYUVHDRPath())
        return false;

    ResolvedHDRYUVFrame resolved;
    if (!resolveRawYUVForHDR(frameIndex, resolved))
        return false;

    const int width = static_cast<int>(frameSize.width);
    const int height = static_cast<int>(frameSize.height);
    const auto colorConversion = conversionSettings.colorConversion;

    if (resolved.ownership == HDRYUVOwnership::Exclusive)
    {
        m_hdrRenderingManager->updateHDRFrameYUVMove(
            std::move(resolved.yuvData), width, height, srcPixelFormat, colorConversion);
    }
    else
    {
        m_hdrRenderingManager->updateHDRFrameYUV(
            resolved.yuvData, width, height, srcPixelFormat, colorConversion);
    }

    return true;
}

// HDR 10-bit optimization - load raw YUV data for caching
// This method bypasses the expensive CPU-side YUV->RGB conversion
// The raw YUV data is cached directly and GPU converts it at render time
void videoHandlerYUV::loadRawFrameForCaching(int frameIndex, QByteArray &rawDataToCache)
{
    DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching " << frameIndex);

    // Get the YUV format and size (thread-safe copies)
    const auto yuvFormat    = this->srcPixelFormat;
    const auto curFrameSize = this->frameSize;
    const auto expectedBytes = yuvFormat.bytesPerFrame(curFrameSize);

    // PERFORMANCE OPTIMIZATION: Use direct read callback if available
    // This bypasses the shared rawData buffer and requestDataMutex,
    // enabling true parallel I/O when multiple caching threads are active.
    if (hasDirectReadCallback())
    {
        DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching using direct read callback");
        
        const auto bytesRead = m_directReadCallback(frameIndex, rawDataToCache);
        
        if (bytesRead < expectedBytes)
        {
            DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching direct read failed: got " 
                      << bytesRead << " expected " << expectedBytes);
            rawDataToCache.clear();
            return;
        }
        
        DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching " << frameIndex
                  << " Done (direct), size=" << rawDataToCache.size());
        return;
    }

    // Fallback: Original signal-based path (serialized by requestDataMutex)
    // This path is used when direct callback is not available
    requestDataMutex.lock();
    emit signalRequestRawData(frameIndex, true);
    rawDataToCache = rawData;  // Direct copy of raw YUV data
    requestDataMutex.unlock();

    if (frameIndex != rawData_frameIndex)
    {
        DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching Loading failed");
        rawDataToCache.clear();
        return;
    }

    DEBUG_YUV("videoHandlerYUV::loadRawFrameForCaching " << frameIndex
              << " Done (signal), size=" << rawDataToCache.size());
}

void videoHandlerYUV::loadValues(Size newFramesize, const QString &)
{
    this->setFrameSize(newFramesize);
}

void videoHandlerYUV::drawFrame(QPainter *painter,
                                int       frameIdx,
                                double    zoomFactor,
                                bool      drawRawData)
{
    // Lazy initialization of settings cache.
    // Using acquire/release semantics ensures thread safety if multiple threads access this,
    // though relaxed is usually sufficient for single-writer scenarios.
    if (!m_settingsCacheValid.load(std::memory_order_acquire))
    {
        QSettings settings;
        m_enable10BitDisplayCached.store(
                    settings.value("Enable10BitDisplay", false).toBool(),
                    std::memory_order_relaxed);
        m_settingsCacheValid.store(true, std::memory_order_release);
    }

    // Fast atomic load for per-frame check
    const bool enable10BitDisplay = m_enable10BitDisplayCached.load(std::memory_order_relaxed);

    // ---------------------------------------------------------------------------
    // HDR Rendering Path (10-bit)
    // ---------------------------------------------------------------------------
    if (m_hdrRenderingManager &&
            m_hdrRenderingManager->isHDRRenderingActive() &&
            enable10BitDisplay &&
            srcPixelFormat.getBitsPerSample() == 10)
    {
        QWindow* hdrWindow = m_hdrRenderingManager->getHDRWindow();

        // Optimization - Minimize casting overhead.
        // Ideally, cache the casted pointer as a member variable (m_cachedHDRWindow)
        // and only update it when the window changes, but this local cast is safer.
        auto hdrVideoWindow = qobject_cast<HDR_WindowType*>(hdrWindow);

        bool hdrReady = false;
        if (hdrVideoWindow) {
            // Short-circuit evaluation: Check internal flags first before calling virtual functions
            hdrReady = hdrWindow->isExposed() && hdrVideoWindow->isInitialized();
        }

        // Paint black on the SDR surface to prevent artifacts underneath the HDR surface
        if (painter) {
            painter->fillRect(painter->viewport(), Qt::black);
        }

        if (hdrReady)
        {
            if (pushFrameToHDR(frameIdx))
                return;

            qWarning() << "videoHandlerYUV: Failed to retrieve data for frame" << frameIdx;
            return;
        }
        else
        {
            if (hdrVideoWindow)
            {
                hdrVideoWindow->clearFrame();
            }
        }
        return; // End of HDR Path
    }

    // ---------------------------------------------------------------------------
    // Standard SDR Rendering Path
    // ---------------------------------------------------------------------------

    std::string msg;
    // Performance check - canConvertToRGB might be expensive.
    // Ensure this function caches results internally if possible.
    if (!srcPixelFormat.canConvertToRGB(frameSize, &msg))
    {
        // Only construct font and metrics objects when absolutely necessary (Error case)
        QFont displayFont = painter->font();
        displayFont.setPointSizeF(displayFont.pointSizeF() * zoomFactor); // Use variable directly
        painter->setFont(displayFont);

        // Using QStringLiteral or FromUtf8 is slightly faster if message is const,
        // but std::string conversion is necessary here.
        QString qMsg = QString::fromStdString("With the given settings, the YUV data can not be converted to RGB:\n" + msg);

        QRect textRect(QPoint(0,0), painter->fontMetrics().size(0, qMsg));
        textRect.moveCenter(QPoint(0, 0));

        painter->drawText(textRect, qMsg);
    }
    else
    {
        videoHandler::drawFrame(painter, frameIdx, zoomFactor, drawRawData);
    }
}

QLayout *videoHandlerYUV::createVideoHandlerControls(bool isSizeAndFormatFixed)
{
    // Absolutely always only call this function once!
    assert(!ui.created());

    QVBoxLayout *newVBoxLayout = nullptr;
    if (!isSizeAndFormatFixed)
    {
        // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the
        // parent controls and our controls into that layout, separated by a line. Return that layout
        newVBoxLayout = new QVBoxLayout;
        newVBoxLayout->addLayout(FrameHandler::createFrameHandlerControls(false));

        QFrame *line = new QFrame;
        line->setObjectName(QStringLiteral("line"));
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        newVBoxLayout->addWidget(line);
    }

    // Create the UI and setup all the controls
    ui.setupUi();

    for (const auto &format : videoHandlerYUV::formatPresetList)
        ui.yuvFormatComboBox->addItem(QString::fromStdString(format.getName()));

    const auto currentFormatInPresetList =
            vectorContains(videoHandlerYUV::formatPresetList, this->srcPixelFormat);
    if (!currentFormatInPresetList && this->srcPixelFormat.isValid())
    {
        videoHandlerYUV::formatPresetList.push_back(this->srcPixelFormat);
        ui.yuvFormatComboBox->addItem(QString::fromStdString(this->srcPixelFormat.getName()));
    }
    ui.yuvFormatComboBox->addItem("Custom...");
    ui.yuvFormatComboBox->setEnabled(!isSizeAndFormatFixed);

    if (const auto presetIndex =
            vectorIndexOf(videoHandlerYUV::formatPresetList, this->srcPixelFormat))
        ui.yuvFormatComboBox->setCurrentIndex(static_cast<int>(*presetIndex));

    // Set all the values of the properties widget to the values of this class
    const auto hasChroma = (srcPixelFormat.getSubsampling() != Subsampling::YUV_400);
    ui.colorComponentsComboBox->addItems(
                functions::toQStringList(ComponentDisplayModeMapper.getNames()));
    ui.colorComponentsComboBox->setCurrentIndex(
                int(ComponentDisplayModeMapper.indexOf(this->conversionSettings.componentDisplayMode)));
    ui.colorComponentsComboBox->setEnabled(hasChroma);
    ui.chromaInterpolationComboBox->addItems(
                functions::toQStringList(ChromaInterpolationMapper.getNames()));
    ui.chromaInterpolationComboBox->setCurrentIndex(
                int(ChromaInterpolationMapper.indexOf(this->conversionSettings.chromaInterpolation)));
    ui.chromaInterpolationComboBox->setEnabled(hasChroma && srcPixelFormat.isChromaSubsampled());
    ui.colorConversionComboBox->addItems(functions::toQStringList(ColorConversionMapper.getNames()));
    ui.colorConversionComboBox->setCurrentIndex(
                int(ColorConversionMapper.indexOf(this->conversionSettings.colorConversion)));
    ui.colorConversionComboBox->setEnabled(hasChroma);
    ui.lumaScaleSpinBox->setValue(this->conversionSettings.mathParameters[Component::Luma].scale);
    ui.lumaOffsetSpinBox->setMaximum(1000);
    ui.lumaOffsetSpinBox->setValue(this->conversionSettings.mathParameters[Component::Luma].offset);
    ui.lumaInvertCheckBox->setChecked(
                this->conversionSettings.mathParameters[Component::Luma].invert);
    ui.chromaScaleSpinBox->setValue(this->conversionSettings.mathParameters[Component::Chroma].scale);
    ui.chromaOffsetSpinBox->setMaximum(1000);
    ui.chromaOffsetSpinBox->setValue(
                this->conversionSettings.mathParameters[Component::Chroma].offset);
    ui.chromaInvertCheckBox->setChecked(
                this->conversionSettings.mathParameters[Component::Chroma].invert);

    // ISP mode controls (YUV400 Bayer false-color preview)
    ui.comboBoxBayerPattern->addItems(functions::toQStringList(BayerPatternMapper.getNames()));
    ui.comboBoxBayerPattern->setCurrentIndex(
                int(BayerPatternMapper.indexOf(this->conversionSettings.bayerPattern)));
    ui.comboBoxISPCellSize->addItems(functions::toQStringList(ISPCellSizeMapper.getNames()));
    ui.comboBoxISPCellSize->setCurrentIndex(
                int(ISPCellSizeMapper.indexOf(this->conversionSettings.ispCellSize)));
    {
        QSignalBlocker blocker(ui.checkBoxEnableISPMode);
        ui.checkBoxEnableISPMode->setChecked(this->conversionSettings.ispModeEnabled);
    }
    this->updateISPModeAvailability();

    // Restore 10-bit display setting from QSettings (PRD Requirements 5.2-5.5)
    QSettings settings;
    bool enable10BitFromSettings = settings.value("Enable10BitDisplay", false).toBool();
    int hdrModeFromSettings = settings.value("HDRMode", 0).toInt(); // 0=PQ, 1=HLG

    // Set checkbox state based on saved configuration
    // This ensures UI consistency with startup HDR decision made in main()
    {
        QSignalBlocker blocker(ui.checkBoxEnable10BitDisplay); // Prevent triggering slot during initialization
        ui.checkBoxEnable10BitDisplay->setChecked(enable10BitFromSettings);
    }

    // Initialize HDR mode combo box with PQ, HLG, and Linear options
    if (ui.comboBoxHDRMode)
    {
        QSignalBlocker blocker(ui.comboBoxHDRMode);
        ui.comboBoxHDRMode->clear();
        ui.comboBoxHDRMode->addItem("PQ (HDR10)",
                                    static_cast<int>(HDR_WindowType::Mode_BT2020_PQ_10bit));
        ui.comboBoxHDRMode->addItem("HLG",
                                    static_cast<int>(HDR_WindowType::Mode_BT2020_HLG_10bit));
        ui.comboBoxHDRMode->addItem("Linear (scRGB)",
                                    static_cast<int>(HDR_WindowType::Mode_BT2020_Linear_16bit));
        ui.comboBoxHDRMode->setCurrentIndex(hdrModeFromSettings);
        ui.comboBoxHDRMode->setEnabled(enable10BitFromSettings); // Enable only when HDR is enabled
    }

    const int exposureNitsFromSettings = getSavedExposureNits(settings, hdrModeFromSettings);
    updateExposureControls(hdrModeFromSettings, enable10BitFromSettings, exposureNitsFromSettings);

    // Force BT.2020 Limited when HDR starts; SDR keeps ctor default (BT.709 or usage prefs)
    if (enable10BitFromSettings)
        applyDefaultColorConversionForDisplayMode(true);

    // Clear any restart notice from previous session (PRD Requirements 5.1 & 5.3)
    this->clearHDRRestartNotice();

    // Connect all the change signals from the controls to "connectWidgetSignals()"
    connect(ui.yuvFormatComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotYUVFormatControlChanged);
    connect(ui.colorComponentsComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.chromaInterpolationComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.colorConversionComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.lumaScaleSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.lumaOffsetSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.lumaInvertCheckBox,
            &QCheckBox::stateChanged,
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.chromaScaleSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.chromaOffsetSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.chromaInvertCheckBox,
            &QCheckBox::stateChanged,
            this,
            &videoHandlerYUV::slotYUVControlChanged);
    connect(ui.checkBoxEnableISPMode,
            &QCheckBox::stateChanged,
            this,
            &videoHandlerYUV::slotISPModeChanged);
    connect(ui.comboBoxBayerPattern,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotISPModeChanged);
    connect(ui.comboBoxISPCellSize,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &videoHandlerYUV::slotISPModeChanged);
    connect(ui.checkBoxEnable10BitDisplay,
            &QCheckBox::stateChanged,
            this,
            &videoHandlerYUV::slot10BitDisplayChanged);

    // Connect HDR mode combo box
    if (ui.comboBoxHDRMode)
    {
        connect(ui.comboBoxHDRMode,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                &videoHandlerYUV::slotHDRModeChanged);
    }

    // Connect Exposure slider and value editor
    if (ui.sliderToneMapNits)
    {
        ui.sliderToneMapNits->setRange(0, kExposureSliderSteps);
        ui.sliderToneMapNits->setSingleStep(1);
        ui.sliderToneMapNits->setPageStep(50);
        ui.sliderToneMapNits->setTickInterval(100);
        connect(ui.sliderToneMapNits,
                &QSlider::valueChanged,
                this,
                &videoHandlerYUV::slotToneMapNitsChanged);
    }

    if (ui.spinBoxToneMapNits)
    {
        connect(ui.spinBoxToneMapNits,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                &videoHandlerYUV::slotToneMapNitsInputChanged);
    }

    // Initialize and connect "Restart now" button (hidden by default)
    if (ui.buttonRestartNow)
    {
        ui.buttonRestartNow->setVisible(false);
        connect(ui.buttonRestartNow, &QPushButton::clicked, this, &videoHandlerYUV::slotRestartNow);
    }

    if (!isSizeAndFormatFixed && newVBoxLayout)
        newVBoxLayout->addLayout(ui.topVBoxLayout);

    // Initial check of HDR availability based on current format
    updateHDRAvailability();

    return (isSizeAndFormatFixed) ? ui.topVBoxLayout : newVBoxLayout;
}

void videoHandlerYUV::slotYUVFormatControlChanged(int selectionIndex)
{
    auto newFormat = this->srcPixelFormat;

    const auto customFormatSelected =
            (selectionIndex == static_cast<int>(videoHandlerYUV::formatPresetList.size()));
    if (customFormatSelected)
    {
        videoHandlerYUVCustomFormatDialog dialog(srcPixelFormat);
        if (dialog.exec() == QDialog::Accepted && dialog.getSelectedYUVFormat().isValid())
            newFormat = dialog.getSelectedYUVFormat();

        const auto isInPresetList = vectorContains(videoHandlerYUV::formatPresetList, newFormat);
        if (!isInPresetList)
        {
            videoHandlerYUV::formatPresetList.push_back(newFormat);
            const QSignalBlocker blocker(this->ui.yuvFormatComboBox);
            const auto           insertPositionBeforeCustom = (this->ui.yuvFormatComboBox->count() - 1);
            ui.yuvFormatComboBox->insertItem(insertPositionBeforeCustom,
                                             QString::fromStdString(newFormat.getName()));
        }

        if (const auto presetIndex = vectorIndexOf(videoHandlerYUV::formatPresetList, newFormat))
        {
            const QSignalBlocker blocker(this->ui.yuvFormatComboBox);
            ui.yuvFormatComboBox->setCurrentIndex(static_cast<int>(*presetIndex));
        }
    }
    else if (selectionIndex >= 0 &&
             selectionIndex < static_cast<int>(videoHandlerYUV::formatPresetList.size()))
        newFormat = videoHandlerYUV::formatPresetList.at(selectionIndex);

    // Set the new format (if new) and emit a signal that a new format was selected.
    if (newFormat != this->srcPixelFormat && newFormat.isValid())
        this->setSrcPixelFormat(newFormat);
}

void videoHandlerYUV::setSrcPixelFormat(PixelFormatYUV format, bool emitSignal)
{
    // Store the number bytes per frame of the old pixel format
    auto oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

    // Set the new pixel format. Lock the mutex, so that no background process is running wile the
    // format changes.
    srcPixelFormat = format;

    const bool newCandidate = isHDR10Candidate();
    if (newCandidate != m_isHDRCandidate)
    {
        m_isHDRCandidate = newCandidate;
        emit signalHandlerChanged(false, RECACHE_NONE);
    }

    // Update the math parameter offset (the default offset depends on the bit depth and the range)
    int        shift     = format.getBitsPerSample() - 8;
    const bool fullRange = isFullRange(this->conversionSettings.colorConversion);
    this->conversionSettings.mathParameters[Component::Luma].offset = (fullRange ? 128 : 125)
            << shift;
    this->conversionSettings.mathParameters[Component::Chroma].offset = 128 << shift;

    if (ui.created())
    {
        // Every time the pixel format changed, see if the interpolation combo box is enabled/disabled
        const auto     hasChroma = (srcPixelFormat.getSubsampling() != Subsampling::YUV_400);
        QSignalBlocker blocker1(ui.colorComponentsComboBox);
        QSignalBlocker blocker2(ui.chromaInterpolationComboBox);
        QSignalBlocker blocker3(ui.colorConversionComboBox);
        QSignalBlocker blocker4(ui.lumaOffsetSpinBox);
        QSignalBlocker blocker5(ui.chromaOffsetSpinBox);
        ui.colorComponentsComboBox->setEnabled(hasChroma);
        ui.chromaInterpolationComboBox->setEnabled(hasChroma && format.isChromaSubsampled());
        ui.colorConversionComboBox->setEnabled(hasChroma);
        ui.lumaOffsetSpinBox->setValue(this->conversionSettings.mathParameters[Component::Luma].offset);
        ui.chromaOffsetSpinBox->setValue(
                    this->conversionSettings.mathParameters[Component::Chroma].offset);

        // ISP mode is only meaningful for YUV400; disable and clear when leaving it.
        this->updateISPModeAvailability();

        // Check and update HDR availability based on bit depth
        updateHDRAvailability();
    }

    if (emitSignal)
    {
        // Set the current buffers to be invalid and emit the signal that this item needs to be redrawn.
        this->currentImageIndex       = -1;
        this->currentImage_frameIndex = -1;

        // Set the cache to invalid until it is cleared an recached
        this->setCacheInvalid();

        if (srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
            // The number of bytes per frame changed. The raw YUV data buffer is also out of date
            this->currentFrameRawData_frameIndex = -1;

        emit signalHandlerChanged(true, RECACHE_CLEAR);
    }
}

void videoHandlerYUV::slotYUVControlChanged()
{
    // The control that caused the slot to be called
    auto sender = QObject::sender();

    if (sender == ui.colorComponentsComboBox || sender == ui.chromaInterpolationComboBox ||
            sender == ui.colorConversionComboBox || sender == ui.lumaScaleSpinBox ||
            sender == ui.lumaOffsetSpinBox || sender == ui.lumaInvertCheckBox ||
            sender == ui.chromaScaleSpinBox || sender == ui.chromaOffsetSpinBox ||
            sender == ui.chromaInvertCheckBox)
    {
        // Store previous color conversion to track changes
        ColorConversion previousColorConversion = this->conversionSettings.colorConversion;

        this->conversionSettings.chromaInterpolation =
                *ChromaInterpolationMapper.getValueAt(ui.chromaInterpolationComboBox->currentIndex());
        this->conversionSettings.componentDisplayMode =
                *ComponentDisplayModeMapper.getValueAt(ui.colorComponentsComboBox->currentIndex());
        this->conversionSettings.colorConversion =
                *ColorConversionMapper.getValueAt(ui.colorConversionComboBox->currentIndex());

        // Track color conversion usage for smart default selection
        // Only update if the color conversion actually changed (user explicitly changed it)
        if (sender == ui.colorConversionComboBox &&
                this->conversionSettings.colorConversion != previousColorConversion) {

            QSettings settings;
            settings.beginGroup("YUV/ColorConversionUsage");

            // Increment usage count for the newly selected color conversion
            auto nameView = ColorConversionMapper.getName(this->conversionSettings.colorConversion);
            QString conversionName = QString::fromUtf8(nameView.data(), nameView.size());
            int currentCount = settings.value(conversionName, 0).toInt();
            settings.setValue(conversionName, currentCount + 1);

            // Also track the total number of color conversion changes
            int totalChanges = settings.value("TotalChanges", 0).toInt();
            settings.setValue("TotalChanges", totalChanges + 1);

            settings.endGroup();

        }

        this->conversionSettings.mathParameters[Component::Luma].scale  = ui.lumaScaleSpinBox->value();
        this->conversionSettings.mathParameters[Component::Luma].offset = ui.lumaOffsetSpinBox->value();
        this->conversionSettings.mathParameters[Component::Luma].invert =
                ui.lumaInvertCheckBox->isChecked();
        this->conversionSettings.mathParameters[Component::Chroma].scale =
                ui.chromaScaleSpinBox->value();
        this->conversionSettings.mathParameters[Component::Chroma].offset =
                ui.chromaOffsetSpinBox->value();
        this->conversionSettings.mathParameters[Component::Chroma].invert =
                ui.chromaInvertCheckBox->isChecked();

        // Set the current frame in the buffer to be invalid and clear the cache.
        // Emit that this item needs redraw and the cache needs updating.
        this->currentImageIndex       = -1;
        this->currentImage_frameIndex = -1;
        this->setCacheInvalid();
        emit signalHandlerChanged(true, RECACHE_CLEAR);
    }
    else if (sender == ui.yuvFormatComboBox)
    {
        auto oldFormatBytesPerFrame = this->srcPixelFormat.bytesPerFrame(frameSize);

        // Set the new YUV format
        // setSrcPixelFormat(yuvFormatList.getFromName(ui.yuvFormatComboBox->currentText()));

        // Set the current frame in the buffer to be invalid and clear the cache.
        // Emit that this item needs redraw and the cache needs updating.
        this->currentImageIndex       = -1;
        this->currentImage_frameIndex = -1;
        if (this->srcPixelFormat.bytesPerFrame(frameSize) != oldFormatBytesPerFrame)
            // The number of bytes per frame changed. The raw YUV data buffer also has to be updated.
            this->currentFrameRawData_frameIndex = -1;
        this->setCacheInvalid();
        emit signalHandlerChanged(true, RECACHE_CLEAR);
    }
}

void videoHandlerYUV::updateExposureControls(int hdrModeIndex, bool hdrEnabled, int exposureNits)
{
    const auto range = getExposureRangeForHDRMode(hdrModeIndex);
    const int clampedExposureNits = clampExposureNitsForHDRMode(exposureNits, hdrModeIndex);
    const bool exposureEnabled = hdrEnabled && range.enabled;

    if (ui.sliderToneMapNits)
    {
        const QSignalBlocker blocker(ui.sliderToneMapNits);
        ui.sliderToneMapNits->setRange(0, kExposureSliderSteps);
        ui.sliderToneMapNits->setValue(exposureNitsToSlider(clampedExposureNits, hdrModeIndex));
        ui.sliderToneMapNits->setEnabled(exposureEnabled);
        ui.sliderToneMapNits->setToolTip(range.enabled
                                         ? QString("Exposure target luminance with logarithmic spacing. Range: %1-%2 nits.")
                                               .arg(range.minNits)
                                               .arg(range.maxNits)
                                         : "Exposure is available in PQ and HLG modes only.");
    }

    if (ui.spinBoxToneMapNits)
    {
        const QSignalBlocker blocker(ui.spinBoxToneMapNits);
        ui.spinBoxToneMapNits->setRange(range.minNits, range.maxNits);
        ui.spinBoxToneMapNits->setValue(clampedExposureNits);
        ui.spinBoxToneMapNits->setEnabled(exposureEnabled);
        ui.spinBoxToneMapNits->setToolTip(range.enabled
                                          ? QString("Enter an exact exposure target. Range: %1-%2 nits.")
                                                .arg(range.minNits)
                                                .arg(range.maxNits)
                                          : "Exposure is available in PQ and HLG modes only.");
    }

    if (ui.labelToneMapping)
    {
        if (range.enabled)
            ui.labelToneMapping->setToolTip(
                        QString("Adjust displayed peak luminance with the slider or enter an exact value. Range: %1-%2 nits.")
                        .arg(range.minNits)
                        .arg(range.maxNits));
        else
            ui.labelToneMapping->setToolTip("Exposure is available in PQ and HLG modes only.");
    }
}

void videoHandlerYUV::applyExposureNits(int exposureNits)
{
    const int hdrModeIndex = ui.comboBoxHDRMode ? ui.comboBoxHDRMode->currentIndex() : 0;
    const int clampedExposureNits = clampExposureNitsForHDRMode(exposureNits, hdrModeIndex);

    updateExposureControls(hdrModeIndex, ui.checkBoxEnable10BitDisplay->isChecked(), clampedExposureNits);

    QSettings settings;
    settings.setValue(kExposureSettingsKey, clampedExposureNits);
    settings.sync();

    auto* hdrMgr = HDRRenderingManager::instance();
    if (hdrMgr && hdrMgr->isHDRRenderingActive())
    {
        auto* hdrWindow = hdrMgr->getHDRWindow();
        const int selectedRenderMode =
                ui.comboBoxHDRMode ? ui.comboBoxHDRMode->currentData().toInt()
                                   : static_cast<int>(HDR_WindowType::Mode_BT2020_PQ_10bit);
        if (hdrWindow && static_cast<int>(hdrWindow->getRenderMode()) == selectedRenderMode)
        {
            hdrWindow->setToneMapTargetNits(static_cast<float>(clampedExposureNits));
        }
    }
}

/**
 * @brief Enable or disable ISP Bayer controls based on the current pixel format.
 *
 * ISP false-color preview is only available for YUV400 (luma-only) sources.
 * Leaving YUV400 clears the ISP enable flag so conversion falls back to gray.
 */
void videoHandlerYUV::updateISPModeAvailability()
{
    if (!ui.created())
        return;

    const bool isYUV400 = (srcPixelFormat.getSubsampling() == Subsampling::YUV_400);
    if (!isYUV400 && this->conversionSettings.ispModeEnabled)
        this->conversionSettings.ispModeEnabled = false;

    {
        QSignalBlocker blocker(ui.checkBoxEnableISPMode);
        ui.checkBoxEnableISPMode->setEnabled(isYUV400);
        ui.checkBoxEnableISPMode->setChecked(isYUV400 && this->conversionSettings.ispModeEnabled);
    }

    const bool ispActive = isYUV400 && this->conversionSettings.ispModeEnabled;
    ui.comboBoxBayerPattern->setEnabled(ispActive);
    ui.comboBoxISPCellSize->setEnabled(ispActive);
    ui.labelBayerPattern->setEnabled(ispActive);
    ui.labelISPCellSize->setEnabled(ispActive);
    ui.labelISPMode->setEnabled(isYUV400);
}

/**
 * @brief Apply ISP mode checkbox / Bayer / cell-size UI changes to conversion settings.
 *
 * Re-converts the current frame so Bayer false-color updates immediately.
 */
void videoHandlerYUV::slotISPModeChanged()
{
    if (!ui.created())
        return;

    const bool isYUV400 = (srcPixelFormat.getSubsampling() == Subsampling::YUV_400);
    this->conversionSettings.ispModeEnabled =
            isYUV400 && ui.checkBoxEnableISPMode->isChecked();

    if (ui.comboBoxBayerPattern->count() > 0)
    {
        this->conversionSettings.bayerPattern =
                *BayerPatternMapper.getValueAt(ui.comboBoxBayerPattern->currentIndex());
    }
    if (ui.comboBoxISPCellSize->count() > 0)
    {
        this->conversionSettings.ispCellSize =
                *ISPCellSizeMapper.getValueAt(ui.comboBoxISPCellSize->currentIndex());
    }

    const bool ispActive = this->conversionSettings.ispModeEnabled;
    ui.comboBoxBayerPattern->setEnabled(ispActive);
    ui.comboBoxISPCellSize->setEnabled(ispActive);
    ui.labelBayerPattern->setEnabled(ispActive);
    ui.labelISPCellSize->setEnabled(ispActive);

    this->currentImageIndex       = -1;
    this->currentImage_frameIndex = -1;
    this->setCacheInvalid();
    emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerYUV::slot10BitDisplayChanged()
{
    // PRD Requirements 5.1 & 5.3: Save settings immediately and show restart notice
    // NO runtime HDR switching - startup-based initialization only

    bool enable10Bit = ui.checkBoxEnable10BitDisplay->isChecked();

    // Enable/disable HDR mode combo box based on checkbox state
    if (ui.comboBoxHDRMode)
    {
        ui.comboBoxHDRMode->setEnabled(enable10Bit);
    }

    // HDR on -> BT.2020 Limited; HDR off -> SDR default BT.709 Limited
    applyDefaultColorConversionForDisplayMode(enable10Bit);

    QSettings settings;
    const int hdrModeIndex = ui.comboBoxHDRMode ? ui.comboBoxHDRMode->currentIndex() : 0;
    updateExposureControls(hdrModeIndex, enable10Bit, getSavedExposureNits(settings, hdrModeIndex));

    // Step 1: Save user intent to configuration immediately (PRD Requirement 5.1 & 5.3)
    settings.setValue("Enable10BitDisplay", enable10Bit);
    settings.sync(); // Ensure immediate write to disk

    // Invalidate cached settings so next access reads fresh value
    // This ensures the new cache mode takes effect immediately for the caching system
    m_settingsCacheValid.store(false, std::memory_order_relaxed);

    // Step 2: Show restart notice (PRD Requirement 5.1 & 5.3)
    showHDRRestartNotice(enable10Bit);

    // Note: No runtime HDR pipeline changes - this eliminates threading and lifecycle issues
    // The new setting will be read at next startup and HDR will be initialized properly
}

void videoHandlerYUV::slotHDRModeChanged(int index)
{
    // Save HDR mode selection (0=PQ, 1=HLG, 2=Linear) to settings
    // This will take effect after application restart
    QSettings settings;
    settings.setValue("HDRMode", index);
    const int exposureNits = getSavedExposureNits(settings, index);
    settings.sync();

    updateExposureControls(index, ui.checkBoxEnable10BitDisplay->isChecked(), exposureNits);

    // Show restart notice since HDR mode change requires restart
    showHDRRestartNotice(ui.checkBoxEnable10BitDisplay->isChecked());
}

void videoHandlerYUV::slotToneMapNitsChanged(int value)
{
    const int hdrModeIndex = ui.comboBoxHDRMode ? ui.comboBoxHDRMode->currentIndex() : 0;
    applyExposureNits(exposureSliderToNits(value, hdrModeIndex));
}

void videoHandlerYUV::slotToneMapNitsInputChanged(int value)
{
    applyExposureNits(value);
}

void videoHandlerYUV::updateHDRAvailability()
{
    if (!ui.created()) {
        return; // UI not created yet
    }

    // Check if current YUV format supports 10-bit display
    bool supports10BitFormat = (srcPixelFormat.getBitsPerSample() >= 10);

    // Check if display supports HDR
    HDRDetection* detector = HDRDetection::instance();
    HDRDetection::HDRCapabilities capabilities = detector->detectHDRCapabilities(nullptr);
    bool supportsHDRDisplay = capabilities.isHDRSupported;

    QSettings settings;

    // Enable checkbox only if BOTH conditions are met
    bool shouldEnableHDR = supports10BitFormat && supportsHDRDisplay;
    ui.checkBoxEnable10BitDisplay->setEnabled(shouldEnableHDR);

    if (!shouldEnableHDR) {
        // Disable and uncheck the checkbox
        const bool wasChecked = ui.checkBoxEnable10BitDisplay->isChecked();
        QSignalBlocker blocker(ui.checkBoxEnable10BitDisplay);  // Prevent triggering slot
        ui.checkBoxEnable10BitDisplay->setChecked(false);
        if (wasChecked)
            applyDefaultColorConversionForDisplayMode(false);

        // NOTE: Do not force-disable global HDR settings here.
        // The authoritative decision (and restart) is handled in HDRRenderingManager after detection.

        // Update tooltip based on the specific reason for being disabled
        QString tooltipText = "Enable native 10-bit display support for 10-bit YUV sources. "
                              "This preserves 10-bit precision during YUV to RGB conversion.\n\n";

        if (!supports10BitFormat && !supportsHDRDisplay) {
            tooltipText += QString("DISABLED: Current YUV format is %1-bit (requires 10-bit+) AND display does not support HDR.\n\n"
                                   "Requirements:\n"
                                   "- 10-bit or higher YUV source\n"
                                   "- HDR-capable display\n"
                                   "- HDR enabled in Windows display settings")
                    .arg(srcPixelFormat.getBitsPerSample());
        } else if (!supports10BitFormat) {
            tooltipText += QString("DISABLED: Current YUV format is %1-bit. 10-bit display requires 10-bit or higher YUV sources.")
                    .arg(srcPixelFormat.getBitsPerSample());
        } else if (!supportsHDRDisplay) {
            tooltipText += QString("DISABLED: Display does not support HDR.\n\n"
                                   "Reason: %1\n\n"
                                   "Requirements:\n"
                                   "- HDR-capable display\n"
                                   "- HDR enabled in Windows display settings\n"
                                   "- Display supporting HDR10 or Dolby Vision")
                    .arg(capabilities.errorMessage.isEmpty() ? "HDR not supported" : capabilities.errorMessage);
        }

        ui.checkBoxEnable10BitDisplay->setToolTip(tooltipText);
    } else {
        bool desiredChecked = settings.value("Enable10BitDisplay", false).toBool();
        if (m_hdrRenderingManager && m_hdrRenderingManager->isHDRRenderingActive())
            desiredChecked = true;

        if (ui.checkBoxEnable10BitDisplay->isChecked() != desiredChecked) {
            QSignalBlocker blocker(ui.checkBoxEnable10BitDisplay);
            ui.checkBoxEnable10BitDisplay->setChecked(desiredChecked);
            applyDefaultColorConversionForDisplayMode(desiredChecked);
        }

        if (desiredChecked && !settings.value("Enable10BitDisplay", false).toBool()) {
            settings.setValue("Enable10BitDisplay", true);
            settings.sync();
            m_settingsCacheValid.store(false, std::memory_order_relaxed);
        }

        // Restore original tooltip for supported configurations
        ui.checkBoxEnable10BitDisplay->setToolTip(
                    "Enable native 10-bit display support for 10-bit YUV sources. "
                    "This preserves 10-bit precision during YUV to RGB conversion.");
    }

    const bool effectiveHDRChecked = shouldEnableHDR && ui.checkBoxEnable10BitDisplay->isChecked();
    if (ui.comboBoxHDRMode)
        ui.comboBoxHDRMode->setEnabled(effectiveHDRChecked);

    const int hdrModeIndex = ui.comboBoxHDRMode ? ui.comboBoxHDRMode->currentIndex() : 0;
    updateExposureControls(hdrModeIndex, effectiveHDRChecked, getSavedExposureNits(settings, hdrModeIndex));

}

void videoHandlerYUV::onHDRRenderingStateChangedWindow(bool enabled, QWindow* window)
{
    // Update SplitView overlay linkage if needed (now handled by manager and SplitViewWidget)
    // Keep logs minimal to avoid flooding the console.
    Q_UNUSED(window);

    // Same behavior: wait for ready, then push pending frame via manager
    if (enabled) {
        QSettings settings;
        bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();
        int bitsPerSample = srcPixelFormat.getBitsPerSample();

        if (enable10BitDisplay && bitsPerSample == 10) {
            const int bootstrapFrameIndex =
                    currentImageIndex >= 0 ? currentImageIndex : currentFrameRawData_frameIndex;
            if (!pushCurrentFrameToHDR(bootstrapFrameIndex))
                emit signalHandlerChanged(true, RECACHE_NONE);
        }
    }
}

bool videoHandlerYUV::pushCurrentFrameToHDR(int frameIndex)
{
    if (pushFrameToHDR(frameIndex))
        return true;

    QImage currentFrameImage = getCurrentFrameAsImage();
    if (currentFrameImage.isNull())
        return false;

    m_pendingHDRFrame = currentFrameImage;
    m_pendingHDRFrameIndex = frameIndex;
    QTimer::singleShot(50, this, [this, currentFrameImage]() {
        if (m_hdrRenderingManager && m_hdrRenderingManager->isHDRRenderingActive())
            m_hdrRenderingManager->updateHDRFrame(currentFrameImage);
    });
    return true;
}

void videoHandlerYUV::onHDRDetectionFailed(const QString& error)
{

    ui.checkBoxEnable10BitDisplay->setChecked(false);

    // Clear any stored settings to prevent inconsistency
    QSettings settings;
    settings.setValue("Enable10BitDisplay", false);

    // Show user-friendly error message
    QWidget* parentWidget = QApplication::activeWindow();
    if (parentWidget) {
        QMessageBox::warning(parentWidget,
                             "HDR Display Not Supported",
                             QString("Unable to enable native 10-bit HDR display:\n\n%1\n\n"
                                     "The checkbox has been reset to disabled state.")
                             .arg(error));
    }

}

QImage videoHandlerYUV::getCurrentFrameAsImage()
{
    const int targetFrameIndex =
            currentImageIndex >= 0 ? currentImageIndex : currentFrameRawData_frameIndex;

    if (currentImage.isNull() && currentImageIndex >= 0) {
        // Try to load the current frame if not already loaded.
        loadFrame(currentImageIndex);
    }

    QSettings settings;
    const bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();

    currentImageSetMutex.lock();
    const QImage currentImageSnapshot = currentImage;
    currentImageSetMutex.unlock();

    if (!currentImageSnapshot.isNull()) {
        if (enable10BitDisplay && srcPixelFormat.getBitsPerSample() == 10) {
            if (currentImageSnapshot.format() == QImage::Format_RGBA64 ||
                    currentImageSnapshot.format() == QImage::Format_RGBA64_Premultiplied) {
                return currentImageSnapshot;
            }
        } else if (currentImageSnapshot.format() != QImage::Format_ARGB32_Premultiplied &&
                   currentImageSnapshot.format() != QImage::Format_RGB32) {
            return currentImageSnapshot.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }

        return currentImageSnapshot;
    }

    if (targetFrameIndex < 0)
        return QImage();

    if (m_pendingHDRFrameIndex == targetFrameIndex && !m_pendingHDRFrame.isNull())
        return m_pendingHDRFrame;

    QByteArray rawFrameSnapshot;
    if (!getCurrentRawFrameSnapshot(targetFrameIndex, rawFrameSnapshot) &&
            !(loadRawYUVData(targetFrameIndex) &&
              getCurrentRawFrameSnapshot(targetFrameIndex, rawFrameSnapshot))) {
        return QImage();
    }

    QImage convertedFrame;
    convertYUVToImage(rawFrameSnapshot,
                      convertedFrame,
                      this->srcPixelFormat,
                      this->frameSize,
                      this->conversionSettings,
                      enable10BitDisplay);

    if (convertedFrame.isNull())
        return QImage();

    m_pendingHDRFrame = convertedFrame;
    m_pendingHDRFrameIndex = targetFrameIndex;
    return m_pendingHDRFrame;
}

QImage videoHandlerYUV::getCurrentFramePatchAsImage(const QRect &regionInPixels)
{
        // Small-region YUV->RGB used by the HDR overlay ZoomBox. This intentionally
    // avoids convertYUVToImage() (which converts the entire frame) and only
    // samples the 25-ish luma + up to a few chroma positions that the overlay
    // needs. For a 4K 10-bit source this is many orders of magnitude cheaper
    // than the full-frame path that was previously invoked every frame.

    if (!isFormatValid() || regionInPixels.isEmpty())
        return QImage();

    const int frameW = static_cast<int>(frameSize.width);
    const int frameH = static_cast<int>(frameSize.height);
    if (frameW <= 0 || frameH <= 0)
        return QImage();

    const QRect frameRect(0, 0, frameW, frameH);
    const QRect clipped = regionInPixels.intersected(frameRect);
    if (clipped.isEmpty())
        return QImage();

    // Pick the most recent raw YUV payload. We prefer the live buffer, fall
    // back to the already-cached "pending" raw snapshot only for the very
    // first HDR bootstrap frame.
    const int targetFrameIndex =
        currentFrameRawData_frameIndex >= 0
            ? currentFrameRawData_frameIndex
            : currentImageIndex;
    if (targetFrameIndex < 0)
        return QImage();

    QByteArray rawFrameSnapshot;
    if (!getCurrentRawFrameSnapshot(targetFrameIndex, rawFrameSnapshot))
        return QImage();

    const PixelFormatYUV fmt = this->srcPixelFormat;
    const Size yuvSize = this->frameSize;

    int rgbConv[5];
    YUVColorConverter::getColorConversionCoefficients(
        conversionSettings.colorConversion, rgbConv);
    const bool fullRange =
        YUVColorConverter::isFullRange(conversionSettings.colorConversion);
    const int bps = fmt.getBitsPerSample();

    QImage out(clipped.size(), QImage::Format_ARGB32_Premultiplied);
    if (out.isNull())
        return QImage();
    out.fill(Qt::black);

    // Per-pixel YUV sampling via the shared, correctness-proven reader. For
    // the ~25 pixels the ZoomBox needs this is negligible compared to the
    // 4K full-frame conversion it replaces (~8.3M pixels).
    for (int dy = 0; dy < clipped.height(); ++dy)
    {
        QRgb *dstLine = reinterpret_cast<QRgb *>(out.scanLine(dy));
        const int srcY = clipped.y() + dy;
        for (int dx = 0; dx < clipped.width(); ++dx)
        {
            const int srcX = clipped.x() + dx;
            const yuv_t yuv = YUVPixelRenderer::getPixelValue(
                rawFrameSnapshot, fmt, yuvSize, QPoint(srcX, srcY));

            int r = 0, g = 0, b = 0;
            YUVColorConverter::convertYUVToRGB8Bit(
                static_cast<unsigned int>(yuv.Y),
                static_cast<unsigned int>(yuv.U),
                static_cast<unsigned int>(yuv.V),
                r, g, b, rgbConv, fullRange, bps);

            dstLine[dx] = qRgb(r, g, b);
        }
    }

    return out;
}

/**
 * @brief Thread-safe single-pixel YUV sampling for HDR overlay callbacks.
 */
bool videoHandlerYUV::getYuvPixelValueAt(const QPoint &pixelPos, yuv_t &value) const
{
    if (!isFormatValid())
        return false;

    const int frameW = static_cast<int>(frameSize.width);
    const int frameH = static_cast<int>(frameSize.height);
    if (pixelPos.x() < 0 || pixelPos.y() < 0 || pixelPos.x() >= frameW || pixelPos.y() >= frameH)
        return false;

    const int targetFrameIndex =
        currentFrameRawData_frameIndex >= 0 ? currentFrameRawData_frameIndex : currentImageIndex;
    if (targetFrameIndex < 0)
        return false;

    QByteArray rawFrameSnapshot;
    if (!getCurrentRawFrameSnapshot(targetFrameIndex, rawFrameSnapshot))
        return false;

    value = YUVPixelRenderer::getPixelValue(
        rawFrameSnapshot, srcPixelFormat, frameSize, pixelPos);
    return true;
}

/* Get the pixels values so we can show them in the info part of the zoom box.
 * If a second frame handler is provided, the difference values from that item will be returned.
 */
QStringPairList videoHandlerYUV::getPixelValues(const QPoint &pixelPos,
                                                int           frameIdx,
                                                FrameHandler *item2,
                                                const int     frameIdx1)
{
    QStringPairList values;

    const int formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
    if (item2 != nullptr)
    {
        videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV *>(item2);
        if (yuvItem2 == nullptr)
            // The given item is not a YUV source. We cannot compare YUV values to non YUV values.
            // Call the base class comparison function to compare the items using the RGB values.
            return FrameHandler::getPixelValues(pixelPos, frameIdx, item2, frameIdx1);

        // Do not get the pixel values if the buffer for the raw YUV values is out of date.
        if (currentFrameRawData_frameIndex != frameIdx ||
                yuvItem2->currentFrameRawData_frameIndex != frameIdx1)
            return QStringPairList();

        int width  = std::min(frameSize.width, yuvItem2->frameSize.width);
        int height = std::min(frameSize.height, yuvItem2->frameSize.height);

        if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
            return QStringPairList();

        yuv_t thisValue  = getPixelValue(pixelPos);
        yuv_t otherValue = yuvItem2->getPixelValue(pixelPos);

        // For difference items, we support difference bit depths for the two items.
        // If the bit depth is different, we scale to value with the lower bit depth to the higher bit
        // depth and calculate the difference there. These values are only needed for difference values
        const unsigned bps_in[2] = {srcPixelFormat.getBitsPerSample(),
                                    yuvItem2->srcPixelFormat.getBitsPerSample()};
        const auto     bps_out   = std::max(bps_in[0], bps_in[1]);
        // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
        const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
        // Scale the input up by this many bits
        const auto depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);

        if (bitDepthScaling[0])
        {
            thisValue.Y = thisValue.Y << depthScale;
            thisValue.U = thisValue.U << depthScale;
            thisValue.V = thisValue.V << depthScale;
        }
        else if (bitDepthScaling[1])
        {
            otherValue.Y = otherValue.Y << depthScale;
            otherValue.U = otherValue.U << depthScale;
            otherValue.V = otherValue.V << depthScale;
        }

        const int     Y       = int(thisValue.Y) - int(otherValue.Y);
        const QString YString = ((Y < 0) ? "-" : "") + QString::number(std::abs(Y), formatBase);
        values.append(QStringPair("Y", YString));

        if (srcPixelFormat.getSubsampling() != Subsampling::YUV_400)
        {
            const int     U       = int(thisValue.U) - int(otherValue.U);
            const int     V       = int(thisValue.V) - int(otherValue.V);
            const QString UString = ((U < 0) ? "-" : "") + QString::number(std::abs(U), formatBase);
            const QString VString = ((V < 0) ? "-" : "") + QString::number(std::abs(V), formatBase);
            values.append(QStringPair("U", UString));
            values.append(QStringPair("V", VString));
        }
    }
    else
    {
        int width  = frameSize.width;
        int height = frameSize.height;

        // Do not get the pixel values if the buffer for the raw YUV values is out of date.
        if (currentFrameRawData_frameIndex != frameIdx)
            return QStringPairList();

        if (pixelPos.x() < 0 || pixelPos.x() >= width || pixelPos.y() < 0 || pixelPos.y() >= height)
            return QStringPairList();

        const yuv_t value = getPixelValue(pixelPos);

        if (showPixelValuesAsDiff)
        {
            // If 'showPixelValuesAsDiff' is set, this is the zero value
            const int differenceZeroValue = 1 << (srcPixelFormat.getBitsPerSample() - 1);

            const int     Y       = int(value.Y) - differenceZeroValue;
            const QString YString = ((Y < 0) ? "-" : "") + QString::number(std::abs(Y), formatBase);
            values.append(QStringPair("Y", YString));

            if (srcPixelFormat.getSubsampling() != Subsampling::YUV_400)
            {
                const int     U       = int(value.U) - differenceZeroValue;
                const int     V       = int(value.V) - differenceZeroValue;
                const QString UString = ((U < 0) ? "-" : "") + QString::number(std::abs(U), formatBase);
                const QString VString = ((V < 0) ? "-" : "") + QString::number(std::abs(V), formatBase);
                values.append(QStringPair("U", UString));
                values.append(QStringPair("V", VString));
            }
        }
        else
        {
            values.append(QStringPair("Y", QString::number(value.Y, formatBase)));
            if (srcPixelFormat.getSubsampling() != Subsampling::YUV_400)
            {
                values.append(QStringPair("U", QString::number(value.U, formatBase)));
                values.append(QStringPair("V", QString::number(value.V, formatBase)));
            }
        }
    }

    return values;
}

/* Draw the YUV values of the pixels over the actual pixels when zoomed in. The values are drawn at
 * the position where they are assumed. So also chroma shifts and subsampling modes are drawn
 * correctly.
 */
void videoHandlerYUV::drawPixelValues(QPainter     *painter,
                                      const int     frameIdx,
                                      const QRect  &videoRect,
                                      const double  zoomFactor,
                                      FrameHandler *item2,
                                      const bool    markDifference,
                                      const int     frameIdxItem1)
{
    // Get the other YUV item (if any)
    auto yuvItem2 = (item2 == nullptr) ? nullptr : dynamic_cast<videoHandlerYUV *>(item2);
    if (item2 != nullptr && yuvItem2 == nullptr)
    {
        // The other item is not a yuv item
        FrameHandler::drawPixelValues(
                    painter, frameIdx, videoRect, zoomFactor, item2, markDifference, frameIdxItem1);
        return;
    }

    auto       size          = frameSize;
    const bool useDiffValues = (yuvItem2 != nullptr);
    if (useDiffValues)
        // If the two items are not of equal size, use the minimum possible size.
        size = Size(std::min(frameSize.width, yuvItem2->frameSize.width),
                    std::min(frameSize.height, yuvItem2->frameSize.height));

    // Check if the raw YUV values are up to date. If not, do not draw them. Do not trigger loading of
    // data here. The needsLoadingRawValues function will return that loading is needed. The caching
    // in the background should then trigger loading of them.
    if (currentFrameRawData_frameIndex != frameIdx)
        return;
    if (yuvItem2 && yuvItem2->currentFrameRawData_frameIndex != frameIdxItem1)
        return;

    // For difference items, we support difference bit depths for the two items.
    // If the bit depth is different, we scale to value with the lower bit depth to the higher bit
    // depth and calculate the difference there. These values are only needed for difference values
    const unsigned bps_in[2] = {srcPixelFormat.getBitsPerSample(),
                                (useDiffValues) ? yuvItem2->srcPixelFormat.getBitsPerSample() : 0};
    const auto     bps_out   = std::max(bps_in[0], bps_in[1]);
    // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
    const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
    // Scale the input up by this many bits
    const auto depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
    // What are the maximum and middle value for the output bit depth
    // const int diffZero = 128 << (bps_out-8);

    // First determine which pixels from this item are actually visible, because we only have to draw
    // the pixel values of the pixels that are actually visible
    auto viewport       = painter->viewport();
    auto worldTransform = painter->worldTransform();

    int xMin_tmp = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
    int yMin_tmp = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
    int xMax_tmp = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
    int yMax_tmp = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

    // Clip the min/max visible pixel values to the size of the item (no pixels outside of the
    // item have to be labeled)
    const int xMin = functions::clip(xMin_tmp, 0, int(size.width) - 1);
    const int yMin = functions::clip(yMin_tmp, 0, int(size.height) - 1);
    const int xMax = functions::clip(xMax_tmp, 0, int(size.width) - 1);
    const int yMax = functions::clip(yMax_tmp, 0, int(size.height) - 1);

    // The center point of the pixel (0,0).
    const auto centerPointZero = (QPoint(-(int(size.width)), -(int(size.height))) * zoomFactor +
                                  QPoint(zoomFactor, zoomFactor)) /
            2;
    // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
    QRect pixelRect;
    pixelRect.setSize(QSize(zoomFactor, zoomFactor));

    // We might change the pen doing this so backup the current pen and reset it later
    auto backupPen = painter->pen();

    // If the Y is below this value, use white text, otherwise black text
    // If there is a second item, a difference will be drawn. A difference of 0 is displayed as gray.
    const int whiteLimit = (yuvItem2) ? 0 : 1 << (srcPixelFormat.getBitsPerSample() - 1);

    // Are there chroma components?
    const bool chromaPresent = (srcPixelFormat.getSubsampling() != Subsampling::YUV_400);
    // The chroma offset in full luma pixels. This can range from 0 to 3.
    const int chromaOffsetFullX = srcPixelFormat.getChromaOffset().x / 2;
    const int chromaOffsetFullY = srcPixelFormat.getChromaOffset().y / 2;
    // Is the chroma offset by another half luma pixel?
    const bool chromaOffsetHalfX = (srcPixelFormat.getChromaOffset().x % 2 != 0);
    const bool chromaOffsetHalfY = (srcPixelFormat.getChromaOffset().y % 2 != 0);
    // By what factor is X and Y subsampled?
    const int subsamplingX = srcPixelFormat.getSubsamplingHor();
    const int subsamplingY = srcPixelFormat.getSubsamplingVer();

    // If 'showPixelValuesAsDiff' is set, this is the zero value
    const int differenceZeroValue = 1 << (srcPixelFormat.getBitsPerSample() - 1);

    const auto mathParameters = this->conversionSettings.mathParameters;

    for (int x = xMin; x <= xMax; x++)
    {
        for (int y = yMin; y <= yMax; y++)
        {
            // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor))
            // and move the pixelRect to that point.
            auto pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
            pixelRect.moveCenter(pixCenter);

            // Get the YUV values to show
            int  Y, U, V;
            bool drawWhite;
            if (useDiffValues)
            {
                auto thisValue  = getPixelValue(QPoint(x, y));
                auto otherValue = yuvItem2->getPixelValue(QPoint(x, y));

                // Do we have to scale one of the values (bit depth different)
                if (bitDepthScaling[0])
                {
                    thisValue.Y = thisValue.Y << depthScale;
                    thisValue.U = thisValue.U << depthScale;
                    thisValue.V = thisValue.V << depthScale;
                }
                else if (bitDepthScaling[1])
                {
                    otherValue.Y = otherValue.Y << depthScale;
                    otherValue.U = otherValue.U << depthScale;
                    otherValue.V = otherValue.V << depthScale;
                }

                Y = int(thisValue.Y) - int(otherValue.Y);
                U = int(thisValue.U) - int(otherValue.U);
                V = int(thisValue.V) - int(otherValue.V);

                if (markDifference)
                    drawWhite = (Y == 0);
                else
                    drawWhite =
                            (mathParameters.at(Component::Luma).invert) ? (Y > whiteLimit) : (Y < whiteLimit);
            }
            else if (showPixelValuesAsDiff)
            {
                yuv_t value = getPixelValue(QPoint(x, y));
                Y           = value.Y - differenceZeroValue;
                U           = value.U - differenceZeroValue;
                V           = value.V - differenceZeroValue;

                drawWhite = (mathParameters.at(Component::Luma).invert) ? (Y > 0) : (Y < 0);
            }
            else
            {
                yuv_t value = getPixelValue(QPoint(x, y));
                Y           = int(value.Y);
                U           = int(value.U);
                V           = int(value.V);
                drawWhite =
                        (mathParameters.at(Component::Luma).invert) ? (Y > whiteLimit) : (Y < whiteLimit);
            }

            const int     formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
            const QString YString    = ((Y < 0) ? "-" : "") + QString::number(std::abs(Y), formatBase);
            const QString UString    = ((U < 0) ? "-" : "") + QString::number(std::abs(U), formatBase);
            const QString VString    = ((V < 0) ? "-" : "") + QString::number(std::abs(V), formatBase);

            painter->setPen(drawWhite ? Qt::white : Qt::black);

            if (chromaPresent && (x - chromaOffsetFullX) % subsamplingX == 0 &&
                    (y - chromaOffsetFullY) % subsamplingY == 0)
            {
                QString valText;
                if (chromaOffsetHalfX || chromaOffsetHalfY)
                    // We will only draw the Y value at the center of this pixel
                    valText = QString("Y%1").arg(YString);
                else
                    // We also draw the U and V value at this position
                    valText = QString("Y%1\nU%2\nV%3").arg(YString, UString, VString);
                painter->drawText(pixelRect, Qt::AlignCenter, valText);

                if (chromaOffsetHalfX || chromaOffsetHalfY)
                {
                    // Draw the U and V values shifted half a pixel right and/or down
                    valText = QString("U%1\nV%2").arg(UString, VString);

                    // Move the QRect by half a pixel
                    if (chromaOffsetHalfX)
                        pixelRect.translate(zoomFactor / 2, 0);
                    if (chromaOffsetHalfY)
                        pixelRect.translate(0, zoomFactor / 2);

                    painter->drawText(pixelRect, Qt::AlignCenter, valText);
                }
            }
            else
            {
                // We only draw the luma value for this pixel
                QString valText = QString("Y%1").arg(YString);
                painter->drawText(pixelRect, Qt::AlignCenter, valText);
            }
        }
    }

    // Reset pen
    painter->setPen(backupPen);
}

void videoHandlerYUV::guessAndSetPixelFormat(
        const filesource::frameFormatGuess::GuessedFrameFormat &frameFormat,
        const filesource::frameFormatGuess::FileInfoForGuess   &fileInfo)
{
    auto format = guessPixelFormatFromSizeAndName(frameFormat, fileInfo);
    if (format.isValid())
        this->setSrcPixelFormat(format, false);
    else
        this->setSrcPixelFormat(PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV), false);
}

/** Try to guess the format of the raw YUV data. A list of candidates is tried (candidateModes) and
 * it is checked if the file size matches and if the correlation of the first two frames is below a
 * threshold. radData must contain at least two frames of the video sequence. Only formats that two
 * frames of could fit into rawData are tested. E.g. the biggest format that is tested here is 1080p
 * YUV 4:2:2 8 bit which is 4147200 bytes per frame. So make sure rawData contains 8294400 bytes so
 * that all formats are tested. If a file size is given, we test if the candidates frame size is a
 * multiple of the fileSize. If fileSize is -1, this test is skipped.
 */
void videoHandlerYUV::setFormatFromCorrelation(const QByteArray &rawYUVData, int64_t fileSize)
{
    if (rawYUVData.size() < 1)
        return;

    class testFormatAndSize
    {
    public:
        testFormatAndSize(const Size &size, PixelFormatYUV format) : size(size), format(format)
        {
            interesting = false;
            mse         = 0;
        }
        Size           size;
        PixelFormatYUV format;
        bool           interesting;
        double         mse;
    };

    // The candidates for the size
    const auto testSizes = std::vector<Size>({Size(176, 144),
                                              Size(352, 240),
                                              Size(352, 288),
                                              Size(480, 480),
                                              Size(480, 576),
                                              Size(704, 480),
                                              Size(720, 480),
                                              Size(704, 576),
                                              Size(720, 576),
                                              Size(1024, 768),
                                              Size(1280, 720),
                                              Size(1280, 960),
                                              Size(1920, 1072),
                                              Size(1920, 1080)});

    // Test bit depths 8, 10 and 16
    std::vector<testFormatAndSize> formatList;
    for (int b = 0; b < 3; b++)
    {
        int bits = (b == 0) ? 8 : (b == 1) ? 10 : 16;
        // Test all subsampling modes
        for (const auto &subsampling : SubsamplingMapper.getValues())
            for (const auto &size : testSizes)
                formatList.push_back(
                            testFormatAndSize(size, PixelFormatYUV(subsampling, bits, PlaneOrder::YUV)));
    }

    if (fileSize > 0)
    {
        // if any candidate exceeds file size for two frames, discard
        // if any candidate does not represent a multiple of file size, discard
        bool fileSizeMatchFound = false;

        for (testFormatAndSize &testFormat : formatList)
        {
            auto picSize = testFormat.format.bytesPerFrame(testFormat.size);

            const bool atLeastTwoPictureInInput = fileSize >= (picSize * 2);
            if (atLeastTwoPictureInInput)
            {
                if ((fileSize % picSize) == 0) // important: file size must be multiple of the picture size
                {
                    testFormat.interesting = true;
                    fileSizeMatchFound     = true;
                }
            }
        }

        if (!fileSizeMatchFound)
            return;
    }

    // calculate max. correlation for first two frames, use max. candidate frame size
    for (testFormatAndSize &testFormat : formatList)
    {
        if (testFormat.interesting)
        {
            auto picSize     = testFormat.format.bytesPerFrame(testFormat.size);
            int  lumaSamples = testFormat.size.width * testFormat.size.height;

            // Calculate the MSE for 2 frames
            if (testFormat.format.getBitsPerSample() == 8)
            {
                auto ptr       = (unsigned char *)rawYUVData.data();
                testFormat.mse = computeMSE(ptr, ptr + picSize, lumaSamples);
            }
            else if (testFormat.format.getBitsPerSample() > 8 &&
                     testFormat.format.getBitsPerSample() <= 16)
            {
                auto ptr       = (unsigned short *)rawYUVData.data();
                testFormat.mse = computeMSE(ptr, ptr + picSize / 2, lumaSamples);
            }
            else
                continue;
        }
    }

    // step3: select best candidate
    double         leastMSE = std::numeric_limits<double>::max();
    PixelFormatYUV bestFormat;
    Size           bestSize;
    for (const testFormatAndSize &testFormat : formatList)
    {
        if (testFormat.interesting && testFormat.mse < leastMSE)
        {
            bestFormat = testFormat.format;
            bestSize   = testFormat.size;
            leastMSE   = testFormat.mse;
        }
    }

    const double mseThreshold = 400;
    if (leastMSE < mseThreshold)
    {
        setSrcPixelFormat(bestFormat, false);
        setFrameSize(bestSize);
    }
}

bool videoHandlerYUV::setFormatFromString(QString format)
{
    DEBUG_YUV("videoHandlerYUV::setFormatFromString " << format << "\n");

    auto split = format.split(";");
    if (split.length() != 4 || split[2] != "YUV")
        return false;

    if (!FrameHandler::setFormatFromString(split[0] + ";" + split[1]))
        return false;

    auto fmt = PixelFormatYUV(split[3].toStdString());
    if (!fmt.isValid())
        return false;

    setSrcPixelFormat(fmt, false);
    return true;
}

ItemLoadingState videoHandlerYUV::needsLoading(int frameIndex, bool loadRawValues)
{
    if (!shouldUseRawYUVCache())
        return videoHandler::needsLoading(frameIndex, loadRawValues);

    if (loadRawValues)
    {
        const auto rawValueState = needsLoadingRawValues(frameIndex);
        if (rawValueState != ItemLoadingState::LoadingNotNeeded)
            return rawValueState;
    }

    if (isInRawCache(frameIndex))
        return ItemLoadingState::LoadingNotNeeded;

    QByteArray currentRawFrameSnapshot;
    if (getCurrentRawFrameSnapshot(frameIndex, currentRawFrameSnapshot))
        return ItemLoadingState::LoadingNotNeeded;

    return ItemLoadingState::LoadingNeeded;
}

void videoHandlerYUV::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
    DEBUG_YUV("videoHandlerYUV::loadFrame " << frameIndex);

    if (!isFormatValid())
        // We cannot load a frame if the format is not known
        return;

    // Does the data in currentFrameRawData need to be updated?
    if (!loadRawYUVData(frameIndex))
        // Loading failed or it is still being performed in the background
        return;

    // In HDR raw-cache mode, rendering uses raw YUV data directly on GPU.
    // Avoid any CPU-side YUV->RGB conversion, even on cache miss fallback.
    if (shouldUseRawYUVCache())
    {
        if (loadToDoubleBuffer)
            doubleBufferImageFrameIndex = -1;
        return;
    }

    // Take a local snapshot to keep conversion independent from concurrent updates.
    QByteArray rawFrameSnapshot;
    if (!getCurrentRawFrameSnapshot(frameIndex, rawFrameSnapshot))
    {
        DEBUG_YUV("videoHandlerYUV::loadFrame Snapshot acquisition failed");
        return;
    }

    // The data in currentFrameRawData is now up to date. If necessary
    // convert the data to RGB.
    QSettings settings;
    bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();

    if (loadToDoubleBuffer)
    {
        QImage newImage;
        convertYUVToImage(rawFrameSnapshot,
                          newImage,
                          this->srcPixelFormat,
                          this->frameSize,
                          this->conversionSettings,
                          enable10BitDisplay);

        // Store the image for potential HDR use, but defer HDR widget creation
        // HDR widget creation must happen in main thread (during drawFrame)
        doubleBufferImage = newImage;
        doubleBufferImageFrameIndex = frameIndex;
    }
    else if (currentImageIndex != frameIndex)
    {
        QImage newImage;
        convertYUVToImage(rawFrameSnapshot,
                          newImage,
                          this->srcPixelFormat,
                          this->frameSize,
                          this->conversionSettings,
                          enable10BitDisplay);

        // Store the image for potential HDR use, but defer HDR widget creation
        // HDR widget creation must happen in main thread (during drawFrame)
        {
            QMutexLocker setLock(&currentImageSetMutex);
            currentImage = newImage;
        }
        currentImageIndex = frameIndex;
    }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
    DEBUG_YUV("videoHandlerYUV::loadFrameForCaching " << frameIndex);

    // Get the YUV format and the size here, so that the caching process does not crash if this
    // changes.
    const auto yuvFormat          = this->srcPixelFormat;
    const auto curFrameSize       = this->frameSize;
    const auto convSettings       = this->conversionSettings;
    const auto expectedBytes      = yuvFormat.bytesPerFrame(curFrameSize);

    QByteArray tmpBufferRawYUVDataCaching;

    // PERFORMANCE OPTIMIZATION: Use direct read callback if available
    // This bypasses the shared rawData buffer and requestDataMutex,
    // enabling true parallel I/O when multiple caching threads are active.
    if (hasDirectReadCallback())
    {
        DEBUG_YUV("videoHandlerYUV::loadFrameForCaching using direct read callback");
        
        const auto bytesRead = m_directReadCallback(frameIndex, tmpBufferRawYUVDataCaching);
        
        if (bytesRead < expectedBytes)
        {
            DEBUG_YUV("videoHandlerYUV::loadFrameForCaching direct read failed: got " 
                      << bytesRead << " expected " << expectedBytes);
            return;
        }
    }
    else
    {
        // Fallback: Original signal-based path (serialized by requestDataMutex)
        requestDataMutex.lock();
        emit signalRequestRawData(frameIndex, true);
        tmpBufferRawYUVDataCaching = rawData;
        requestDataMutex.unlock();

        if (frameIndex != rawData_frameIndex)
        {
            // Loading failed
            DEBUG_YUV("videoHandlerYUV::loadFrameForCaching Loading failed");
            return;
        }
    }

    // Convert YUV to image. This can then be cached.
    QSettings settings;
    bool enable10BitDisplay = settings.value("Enable10BitDisplay", false).toBool();

    convertYUVToImage(
                tmpBufferRawYUVDataCaching, frameToCache, yuvFormat, curFrameSize, convSettings, enable10BitDisplay);
}

bool videoHandlerYUV::getCurrentRawFrameSnapshot(int frameIndex, QByteArray &snapshot) const
{
    QMutexLocker rawDataLock(&m_currentFrameRawDataMutex);
    if (currentFrameRawData_frameIndex != frameIndex || currentFrameRawData.isEmpty())
        return false;

    snapshot = currentFrameRawData;
    return true;
}

// Load the raw YUV data for the given frame index into currentFrameRawData.
bool videoHandlerYUV::loadRawYUVData(int frameIndex)
{
    {
        QMutexLocker rawDataLock(&m_currentFrameRawDataMutex);
        if (currentFrameRawData_frameIndex == frameIndex && cacheValid)
            // Buffer already up to date
            return true;
    }

    DEBUG_YUV("videoHandlerYUV::loadRawYUVData " << frameIndex);

    // The function loadFrameForCaching also uses the signalRequesRawYUVData to request raw data.
    // However, only one thread can use this at a time.
    requestDataMutex.lock();
    emit signalRequestRawData(frameIndex, false);

    if (frameIndex != rawData_frameIndex || rawData.isEmpty())
    {
        // Loading failed
        DEBUG_YUV("videoHandlerYUV::loadRawYUVData Loading failed");
        requestDataMutex.unlock();
        return false;
    }

    {
        QMutexLocker rawDataLock(&m_currentFrameRawDataMutex);
        currentFrameRawData            = rawData;
        currentFrameRawData_frameIndex = frameIndex;
    }
    requestDataMutex.unlock();

    DEBUG_YUV("videoHandlerYUV::loadRawYUVData " << frameIndex << " Done");
    return true;
}

yuv_t videoHandlerYUV::getPixelValue(const QPoint &pixelPos) const
{
    const PixelFormatYUV format = srcPixelFormat;
    const int            w      = frameSize.width;
    const int            h      = frameSize.height;

    yuv_t value = {0, 0, 0};

    if (auto predefinedFormat = format.getPredefinedFormat())
    {
        if (predefinedFormat == PredefinedPixelFormat::V210)
            value = getPixelValueV210(currentFrameRawData, frameSize, pixelPos);
    }
    else if (format.isPlanar())
    {
        // The luma component has full resolution. The size of each chroma components depends on the
        // subsampling.
        const int componentSizeLuma = (w * h);
        const int componentSizeChroma =
                (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

        // How many bytes are in each component?
        const int nrBytesLumaPlane =
                (format.getBitsPerSample() > 8) ? componentSizeLuma * 2 : componentSizeLuma;
        const int nrBytesChromaPlane =
                (format.getBitsPerSample() > 8) ? componentSizeChroma * 2 : componentSizeChroma;

        // Luma first
        const unsigned char *restrict srcY   = (unsigned char *)currentFrameRawData.data();
        const unsigned int offsetCoordinateY = w * pixelPos.y() + pixelPos.x();
        value.Y                              = getValueFromSource(
                    srcY, offsetCoordinateY, format.getBitsPerSample(), format.isBigEndian());

        if (format.getSubsampling() != Subsampling::YUV_400)
        {
            // Now Chroma
            const bool uFirst =
                    (format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YUVA);
            const bool hasAlpha = (format.getPlaneOrder() == PlaneOrder::YUVA ||
                                   format.getPlaneOrder() == PlaneOrder::YVUA);
            if (format.isUVInterleaved())
            {
                // U, V (and alpha) are interleaved
                const unsigned char *restrict srcUVA = srcY + nrBytesLumaPlane;
                const unsigned int mult              = hasAlpha ? 3 : 2;
                const unsigned int offsetCoordinateUV =
                        ((w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) +
                         pixelPos.x() / format.getSubsamplingHor()) *
                        mult;

                value.U = getValueFromSource(srcUVA,
                                             offsetCoordinateUV + (uFirst ? 0 : 1),
                                             format.getBitsPerSample(),
                                             format.isBigEndian());
                value.V = getValueFromSource(srcUVA,
                                             offsetCoordinateUV + (uFirst ? 1 : 0),
                                             format.getBitsPerSample(),
                                             format.isBigEndian());
            }
            else
            {
                const unsigned char *restrict srcU =
                        uFirst ? srcY + nrBytesLumaPlane : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
                const unsigned char *restrict srcV =
                        uFirst ? srcY + nrBytesLumaPlane + nrBytesChromaPlane : srcY + nrBytesLumaPlane;

                // Get the YUV data from the currentFrameRawData
                const unsigned int offsetCoordinateUV =
                        (w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) +
                        pixelPos.x() / format.getSubsamplingHor();

                value.U = getValueFromSource(
                            srcU, offsetCoordinateUV, format.getBitsPerSample(), format.isBigEndian());
                value.V = getValueFromSource(
                            srcV, offsetCoordinateUV, format.getBitsPerSample(), format.isBigEndian());
            }
        }
    }
    else
    {
        const auto packing = format.getPackingOrder();
        if (format.getSubsampling() == Subsampling::YUV_422)
        {
            // The data is arranged in blocks of 4 samples. How many of these are there?
            // What are the offsets withing the 4 samples for the components?
            const int oY = (packing == PackingOrder::YUYV || packing == PackingOrder::YVYU) ? 0 : 1;
            const int oU = (packing == PackingOrder::UYVY)   ? 0
                                                             : (packing == PackingOrder::YUYV) ? 1
                                                                                               : (packing == PackingOrder::VYUY) ? 2
                                                                                                                                 : 3;
            const int oV = (packing == PackingOrder::VYUY)   ? 0
                                                             : (packing == PackingOrder::YVYU) ? 1
                                                                                               : (packing == PackingOrder::UYVY) ? 2
                                                                                                                                 : 3;

            if (format.isBytePacking() && format.getBitsPerSample() == 10)
            {
                // The format is 4 values in 40 bits (5 bytes) which fits exactly for 422 10 bit.
                auto offsetInInput = pixelPos.y() * (pixelPos.x() / 2) * 5;
                const unsigned char *restrict src =
                        (unsigned char *)currentFrameRawData.data() + offsetInInput;

                unsigned short values[4];
                values[0] = (src[0] << 2) + (src[1] >> 6);
                values[1] = ((src[1] & 0x3f) << 4) + (src[2] >> 4);
                values[2] = ((src[2] & 0x0f) << 6) + (src[3] >> 2);
                values[3] = ((src[3] & 0x03) << 8) + src[4];

                if (pixelPos.x() % 2 == 0)
                    value.Y = values[oY];
                else
                    value.Y = values[oY + 2];
                value.U = values[oU];
                value.V = values[oV];
            }
            else
            {
                // The offset of the pixel in bytes
                const unsigned offsetCoordinate4Block = (w * 2 * pixelPos.y() + (pixelPos.x() / 2 * 4)) *
                        (format.getBitsPerSample() > 8 ? 2 : 1);
                const unsigned char *restrict src =
                        (unsigned char *)currentFrameRawData.data() + offsetCoordinate4Block;

                value.Y = getValueFromSource(src,
                                             (pixelPos.x() % 2 == 0) ? oY : oY + 2,
                                             format.getBitsPerSample(),
                                             format.isBigEndian());
                value.U = getValueFromSource(src, oU, format.getBitsPerSample(), format.isBigEndian());
                value.V = getValueFromSource(src, oV, format.getBitsPerSample(), format.isBigEndian());
            }
        }
        else if (format.getSubsampling() == Subsampling::YUV_444)
        {
            // The samples are packed in 4:4:4.
            // What are the offsets withing the 3 or 4 bytes per sample?
            const int oY = (packing == PackingOrder::AYUV) ? 1 : (packing == PackingOrder::VUYA) ? 2 : 0;
            const int oU = (packing == PackingOrder::YUV || packing == PackingOrder::YUVA ||
                            packing == PackingOrder::VUYA)
                    ? 1
                    : 2;
            const int oV = (packing == PackingOrder::YVU)    ? 1
                                                             : (packing == PackingOrder::AYUV) ? 3
                                                                                               : (packing == PackingOrder::VUYA) ? 0
                                                                                                                                 : 2;

            // How many bytes to the next sample?
            const int offsetNext =
                    (packing == PackingOrder::YUV || packing == PackingOrder::YVU ? 3 : 4) *
                    (format.getBitsPerSample() > 8 ? 2 : 1);
            const int offsetSrc               = (w * pixelPos.y() + pixelPos.x()) * offsetNext;
            const unsigned char *restrict src = (unsigned char *)currentFrameRawData.data() + offsetSrc;

            value.Y = getValueFromSource(src, oY, format.getBitsPerSample(), format.isBigEndian());
            value.U = getValueFromSource(src, oU, format.getBitsPerSample(), format.isBigEndian());
            value.V = getValueFromSource(src, oV, format.getBitsPerSample(), format.isBigEndian());
        }
    }

    return value;
}

bool videoHandlerYUV::markDifferencesYUVPlanarToRGB(const QByteArray     &sourceBuffer,
                                                    unsigned char        *targetBuffer,
                                                    const Size            curFrameSize,
                                                    const PixelFormatYUV &sourceBufferFormat) const
{
    // These are constant for the runtime of this function. This way, the compiler can optimize the
    // hell out of this function.
    const auto format = sourceBufferFormat;
    const auto w      = curFrameSize.width;
    const auto h      = curFrameSize.height;

    const int bps   = format.getBitsPerSample();
    const int cZero = 128 << (bps - 8);

    // Other bit depths not (yet) supported. w and h must be divisible by the subsampling.
    assert(bps >= 8 && bps <= 16 && (w % format.getSubsamplingHor()) == 0 &&
           (h % format.getSubsamplingVer()) == 0);

    // The luma component has full resolution. The size of each chroma components depends on the
    // subsampling.
    const int componentSizeLuma = (w * h);
    const int componentSizeChroma =
            (w / format.getSubsamplingHor()) * (h / format.getSubsamplingVer());

    // How many bytes are in each component?
    const int nrBytesLumaPlane   = (bps > 8) ? componentSizeLuma * 2 : componentSizeLuma;
    const int nrBytesChromaPlane = (bps > 8) ? componentSizeChroma * 2 : componentSizeChroma;

    // Is this big endian (actually the difference buffer should always be big endian)
    const bool bigEndian = format.isBigEndian();

    // A pointer to the output
    unsigned char *restrict dst = targetBuffer;

    // Get the pointers to the source planes (8 bit per sample)
    const unsigned char *restrict srcY = (unsigned char *)sourceBuffer.data();
    const unsigned char *restrict srcU =
            (format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY + nrBytesLumaPlane
            : srcY + nrBytesLumaPlane + nrBytesChromaPlane;
    const unsigned char *restrict srcV =
            (format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY + nrBytesLumaPlane + nrBytesChromaPlane
            : srcY + nrBytesLumaPlane;

    const int sampleBlocksX = format.getSubsamplingHor();
    const int sampleBlocksY = format.getSubsamplingVer();

    const int strideC = w / sampleBlocksX; // How many samples to the next y line?
    for (unsigned y = 0; y < h; y += sampleBlocksY)
        for (unsigned x = 0; x < w; x += sampleBlocksX)
        {
            // Get the U/V difference value. For all values within the sub-block this is constant.
            int uvIndex = (y / sampleBlocksY) * strideC + x / sampleBlocksX;
            int valU    = getValueFromSource(srcU, uvIndex, bps, bigEndian);
            int valV    = getValueFromSource(srcV, uvIndex, bps, bigEndian);

            for (int yInBlock = 0; yInBlock < sampleBlocksY; yInBlock++)
            {
                for (int xInBlock = 0; xInBlock < sampleBlocksX; xInBlock++)
                {
                    // Get the Y difference value
                    int valY = getValueFromSource(srcY, (y + yInBlock) * w + x + xInBlock, bps, bigEndian);

                    // select RGB color
                    unsigned char R = 0, G = 0, B = 0;
                    if (valY == cZero)
                    {
                        G = (valU == cZero) ? 0 : 70;
                        B = (valV == cZero) ? 0 : 70;
                    }
                    else
                    {
                        // Y difference
                        if (valU == cZero && valV == cZero)
                        {
                            R = 70;
                            G = 70;
                            B = 70;
                        }
                        else
                        {
                            G = (valU == cZero) ? 0 : 255;
                            B = (valV == cZero) ? 0 : 255;
                        }
                    }

                    // Set the RGB value for the output
                    dst[((y + yInBlock) * w + x + xInBlock) * 4]     = B;
                    dst[((y + yInBlock) * w + x + xInBlock) * 4 + 1] = G;
                    dst[((y + yInBlock) * w + x + xInBlock) * 4 + 2] = R;
                    dst[((y + yInBlock) * w + x + xInBlock) * 4 + 3] = 255;
                }
            }
        }

    return true;
}

QImage videoHandlerYUV::calculateDifference(FrameHandler    *item2,
                                            const int        frameIdxItem0,
                                            const int        frameIdxItem1,
                                            QList<InfoItem> &differenceInfoList,
                                            const int        amplificationFactor,
                                            const bool       markDifference)
{
    this->diffReady = false;

    videoHandlerYUV *yuvItem2 = dynamic_cast<videoHandlerYUV *>(item2);
    if (yuvItem2 == nullptr)
        // The given item is not a YUV source. We cannot compare YUV values to non YUV values.
        // Call the base class comparison function to compare the items using the RGB values.
        return videoHandler::calculateDifference(item2,
                                                 frameIdxItem0,
                                                 frameIdxItem1,
                                                 differenceInfoList,
                                                 amplificationFactor,
                                                 markDifference);

    if (srcPixelFormat.getSubsampling() != yuvItem2->srcPixelFormat.getSubsampling())
        // The two items have different subsampling modes. Compare RGB values instead.
        return videoHandler::calculateDifference(item2,
                                                 frameIdxItem0,
                                                 frameIdxItem1,
                                                 differenceInfoList,
                                                 amplificationFactor,
                                                 markDifference);

    // Get/Set the bit depth of the input and output
    // If the bit depth of the two items is different, we will scale the item with the lower bit depth
    // up.
    const unsigned bps_in[2] = {srcPixelFormat.getBitsPerSample(),
                                yuvItem2->srcPixelFormat.getBitsPerSample()};
    const auto     bps_out   = std::max(bps_in[0], bps_in[1]);

    const unsigned bitDepthScale[2] = {bps_out - bps_in[0], bps_out - bps_in[1]};
    if (bitDepthScale[0] > 0 || bitDepthScale[1] > 0)
        differenceInfoList.append(
                    InfoItem("Warning"sv,
                             "The bit depth of the two items differs.",
                             "The bit depth of the two input items is different. The lower bit depth will be "
                             "scaled up and the difference is calculated."));

    // What are the maximum and middle value for the output bit depth
    const int diffZero = 128 << (bps_out - 8);
    const int maxVal   = (1 << bps_out) - 1;

    // Do we amplify the values?
    const bool amplification = (amplificationFactor != 1 && !markDifference);

    // Load the right raw YUV data (if not already loaded).
    // This will just update the raw YUV data. No conversion to image (RGB) is performed. This is
    // either done on request if the frame is actually shown or has already been done by the caching
    // process.
    if (!loadRawYUVData(frameIdxItem0))
        return QImage(); // Loading failed
    if (!yuvItem2->loadRawYUVData(frameIdxItem1))
        return QImage(); // Loading failed

    // Both YUV buffers are up to date. Really calculate the difference.
    DEBUG_YUV("videoHandlerYUV::calculateDifference frame idx item 0 "
              << frameIdxItem0 << " - item 1 " << frameIdxItem1);

    // The items can be of different size (we then calculate the difference of the top left aligned
    // part)
    const unsigned w_in[] = {frameSize.width, yuvItem2->frameSize.width};
    const unsigned h_in[] = {frameSize.height, yuvItem2->frameSize.height};
    const auto     w_out  = std::min(w_in[0], w_in[1]);
    const auto     h_out  = std::min(h_in[0], h_in[1]);
    // Append a warning if the frame sizes are different
    if (frameSize != yuvItem2->frameSize)
        differenceInfoList.append(
                    InfoItem("Warning"sv,
                             "The size of the two items differs.",
                             "The size of the two input items is different. The difference of the top left "
                             "aligned part that overlaps will be calculated."));

    PixelFormatYUV tmpDiffYUVFormat(srcPixelFormat.getSubsampling(), bps_out, PlaneOrder::YUV, true);
    diffYUVFormat = tmpDiffYUVFormat;

    if (!tmpDiffYUVFormat.canConvertToRGB(Size(w_out, h_out)))
        return QImage();

    // Get subsampling modes (they are identical for both inputs and the output)
    const auto subH = srcPixelFormat.getSubsamplingHor();
    const auto subV = srcPixelFormat.getSubsamplingVer();

    // Get the endianness of the inputs
    const bool bigEndian[2] = {srcPixelFormat.isBigEndian(), yuvItem2->srcPixelFormat.isBigEndian()};

    // Get pointers to the inputs
    const unsigned componentSizeLuma_In[2]   = {w_in[0] * h_in[0], w_in[1] * h_in[1]};
    const unsigned componentSizeChroma_In[2] = {(w_in[0] / subH) * (h_in[0] / subV),
                                                (w_in[1] / subH) * (h_in[1] / subV)};
    const unsigned nrBytesLumaPlane_In[2]    = {
        bps_in[0] > 8 ? 2 * componentSizeLuma_In[0] : componentSizeLuma_In[0],
        bps_in[1] > 8 ? 2 * componentSizeLuma_In[1] : componentSizeLuma_In[1]};
    const unsigned nrBytesChromaPlane_In[2] = {
        bps_in[0] > 8 ? 2 * componentSizeChroma_In[0] : componentSizeChroma_In[0],
        bps_in[1] > 8 ? 2 * componentSizeChroma_In[1] : componentSizeChroma_In[1]};
    // Current item
    const unsigned char *restrict srcY1 = (unsigned char *)currentFrameRawData.data();
    const unsigned char *restrict srcU1 =
            (srcPixelFormat.getPlaneOrder() == PlaneOrder::YUV ||
             srcPixelFormat.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY1 + nrBytesLumaPlane_In[0]
              : srcY1 + nrBytesLumaPlane_In[0] + nrBytesChromaPlane_In[0];
    const unsigned char *restrict srcV1 =
            (srcPixelFormat.getPlaneOrder() == PlaneOrder::YUV ||
             srcPixelFormat.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY1 + nrBytesLumaPlane_In[0] + nrBytesChromaPlane_In[0]
              : srcY1 + nrBytesLumaPlane_In[0];
    // The other item
    const unsigned char *restrict srcY2 = (unsigned char *)yuvItem2->currentFrameRawData.data();
    const unsigned char *restrict srcU2 =
            (yuvItem2->srcPixelFormat.getPlaneOrder() == PlaneOrder::YUV ||
             yuvItem2->srcPixelFormat.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY2 + nrBytesLumaPlane_In[1]
              : srcY2 + nrBytesLumaPlane_In[1] + nrBytesChromaPlane_In[1];
    const unsigned char *restrict srcV2 =
            (yuvItem2->srcPixelFormat.getPlaneOrder() == PlaneOrder::YUV ||
             yuvItem2->srcPixelFormat.getPlaneOrder() == PlaneOrder::YUVA)
            ? srcY2 + nrBytesLumaPlane_In[1] + nrBytesChromaPlane_In[1]
              : srcY2 + nrBytesLumaPlane_In[1];

    // Get pointers to the output
    const int componentSizeLuma_out   = w_out * h_out * (bps_out > 8 ? 2 : 1); // Size in bytes
    const int componentSizeChroma_out = (w_out / subH) * (h_out / subV) * (bps_out > 8 ? 2 : 1);
    // Resize the output buffer to the right size
    diffYUV.resize(componentSizeLuma_out + 2 * componentSizeChroma_out);
    unsigned char *restrict dstY = (unsigned char *)diffYUV.data();
    unsigned char *restrict dstU = dstY + componentSizeLuma_out;
    unsigned char *restrict dstV = dstU + componentSizeChroma_out;

    // Also calculate the MSE while we're at it (Y,U,V)
    // TODO: Bug: MSE is not scaled correctly in all YUV format cases
    int64_t mseAdd[3] = {0, 0, 0};

    // Calculate Luma sample difference
    const unsigned stride_in[2] = {bps_in[0] > 8 ? w_in[0] * 2 : w_in[0],
                                   bps_in[1] > 8 ? w_in[1] * 2
                                   : w_in[1]}; // How many bytes to the next y line?
    for (unsigned y = 0; y < h_out; y++)
    {
        for (unsigned x = 0; x < w_out; x++)
        {
            auto val1 = getValueFromSource(srcY1, x, bps_in[0], bigEndian[0]);
            auto val2 = getValueFromSource(srcY2, x, bps_in[1], bigEndian[1]);

            // Scale (if necessary)
            val1 = val1 << bitDepthScale[0];
            val2 = val2 << bitDepthScale[1];

            // Calculate the difference, add MSE, (amplify) and clip the difference value
            auto diff = val1 - val2;
            mseAdd[0] += diff * diff;
            if (amplification)
                diff *= amplificationFactor;
            diff = functions::clip(diff + diffZero, 0, maxVal);

            setValueInBuffer(dstY, diff, 0, bps_out, true);
            dstY += (bps_out > 8) ? 2 : 1;
        }

        // Goto the next y line
        srcY1 += stride_in[0];
        srcY2 += stride_in[1];
    }

    // Next U/V
    const unsigned strideC_in[2] = {
        w_in[0] / subH * (bps_in[0] > 8 ? 2 : 1),
        w_in[1] / subH * (bps_in[1] > 8 ? 2 : 1)}; // How many bytes to the next U/V y line
    for (unsigned y = 0; y < h_out / subV; y++)
    {
        for (unsigned x = 0; x < w_out / subH; x++)
        {
            auto valU1 = getValueFromSource(srcU1, x, bps_in[0], bigEndian[0]);
            auto valU2 = getValueFromSource(srcU2, x, bps_in[1], bigEndian[1]);
            auto valV1 = getValueFromSource(srcV1, x, bps_in[0], bigEndian[0]);
            auto valV2 = getValueFromSource(srcV2, x, bps_in[1], bigEndian[1]);

            // Scale (if necessary)
            valU1 = valU1 << bitDepthScale[0];
            valV1 = valV1 << bitDepthScale[0];
            valU2 = valU2 << bitDepthScale[1];
            valV2 = valV2 << bitDepthScale[1];

            // Calculate the difference, add MSE, (amplify) and clip the difference value
            auto diffU = valU1 - valU2;
            auto diffV = valV1 - valV2;
            mseAdd[1] += diffU * diffU;
            mseAdd[2] += diffV * diffV;
            if (amplification)
            {
                diffU *= amplificationFactor;
                diffV *= amplificationFactor;
            }
            diffU = functions::clip(diffU + diffZero, 0, maxVal);
            diffV = functions::clip(diffV + diffZero, 0, maxVal);

            setValueInBuffer(dstU, diffU, 0, bps_out, true);
            setValueInBuffer(dstV, diffV, 0, bps_out, true);
            dstU += (bps_out > 8) ? 2 : 1;
            dstV += (bps_out > 8) ? 2 : 1;
        }

        // Goto the next y line
        srcU1 += strideC_in[0];
        srcV1 += strideC_in[0];
        srcU2 += strideC_in[1];
        srcV2 += strideC_in[1];
    }

    // Next we convert the difference YUV image to RGB, either using the normal conversion function or
    // another function that only marks the difference values.

    // Create the output image in the right format
    // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA
    // (each 8 bit).
    QImage outputImage;
    if (is_Q_OS_WIN)
        outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32_Premultiplied);
    else if (is_Q_OS_MAC)
        outputImage = QImage(QSize(w_out, h_out), QImage::Format_RGB32);
    else if (is_Q_OS_LINUX)
    {
        auto format = functionsGui::platformImageFormat(tmpDiffYUVFormat.hasAlpha());
        if (format == QImage::Format_ARGB32_Premultiplied)
            outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32_Premultiplied);
        if (format == QImage::Format_ARGB32)
            outputImage = QImage(QSize(w_out, h_out), QImage::Format_ARGB32);
        else
            outputImage = QImage(QSize(w_out, h_out), QImage::Format_RGB32);
    }

    if (markDifference)
        // We don't want to see the actual difference but just where differences are.
        markDifferencesYUVPlanarToRGB(
                    diffYUV, outputImage.bits(), Size(w_out, h_out), tmpDiffYUVFormat);
    else
    {
        // Get the format of the tmpDiffYUV buffer and convert it to RGB
        ConversionSettings conversionSettings;
        conversionSettings.mathParameters[Component::Luma]   = MathParameters(1, 125, false);
        conversionSettings.mathParameters[Component::Chroma] = MathParameters(1, 128, false);
        convertYUVPlanarToRGB(
                    diffYUV, outputImage.bits(), Size(w_out, h_out), tmpDiffYUVFormat, conversionSettings);
    }

    differenceInfoList.append(InfoItem(
                                  "Difference Type", "YUV " + formatSubsamplingWithColons(srcPixelFormat.getSubsampling())));

    {
        const auto nrPixelsLuma = w_out * h_out;

        const auto mseY = double(mseAdd[0]) / nrPixelsLuma;
        differenceInfoList.append(InfoItem("MSE/PSNR Y", formatMSEandPSNR(mseY, bps_out)));

        if (srcPixelFormat.getSubsampling() != Subsampling::YUV_400)
        {
            auto nrPixelsChroma = w_out / subH * h_out / subV;

            auto mseU = double(mseAdd[1]) / nrPixelsChroma;
            differenceInfoList.append(InfoItem("MSE/PSNR U", formatMSEandPSNR(mseU, bps_out)));

            auto mseV = double(mseAdd[2]) / nrPixelsChroma;
            differenceInfoList.append(InfoItem("MSE/PSNR V", formatMSEandPSNR(mseV, bps_out)));

            auto mseAvg = double(mseAdd[0] + mseAdd[1] + mseAdd[2]) / (nrPixelsLuma + 2 * nrPixelsChroma);
            differenceInfoList.append(InfoItem("MSE/PSNR Avg", formatMSEandPSNR(mseAvg, bps_out)));
        }
    }

    if (is_Q_OS_LINUX)
    {
        // On linux, we may have to convert the image to the platform image format if it is not one of
        // the RGBA formats.
        auto format = functionsGui::platformImageFormat(tmpDiffYUVFormat.hasAlpha());
        if (format != QImage::Format_ARGB32_Premultiplied && format != QImage::Format_ARGB32 &&
                format != QImage::Format_RGB32)
            return outputImage.convertToFormat(format);
    }

    // we have a yuv differance available
    this->diffReady = true;
    return outputImage;
}

void videoHandlerYUV::setPixelFormatYUV(const PixelFormatYUV &newFormat, bool emitSignal)
{
    if (!newFormat.isValid())
        return;

    if (newFormat != this->srcPixelFormat)
    {
        if (this->ui.created())
        {
            const auto isInPresetList = vectorContains(videoHandlerYUV::formatPresetList, newFormat);
            if (!isInPresetList)
            {
                videoHandlerYUV::formatPresetList.push_back(newFormat);
                const auto           insertPositionBeforeCustom = (ui.yuvFormatComboBox->count() - 1);
                const QSignalBlocker blocker(ui.yuvFormatComboBox);
                ui.yuvFormatComboBox->insertItem(insertPositionBeforeCustom,
                                                 QString::fromStdString(newFormat.getName()));
            }

            if (const auto presetIndex = vectorIndexOf(videoHandlerYUV::formatPresetList, newFormat))
            {
                const QSignalBlocker blocker(ui.yuvFormatComboBox);
                ui.yuvFormatComboBox->setCurrentIndex(static_cast<int>(*presetIndex));
            }
        }

        setSrcPixelFormat(newFormat, emitSignal);
    }
}

void videoHandlerYUV::setYUVColorConversion(ColorConversion conversion)
{
    if (conversion != this->conversionSettings.colorConversion)
    {
        this->conversionSettings.colorConversion = conversion;

        if (ui.created())
            ui.colorConversionComboBox->setCurrentIndex(
                        int(ColorConversionMapper.indexOf(this->conversionSettings.colorConversion)));
    }
}

void videoHandlerYUV::applyDefaultColorConversionForDisplayMode(bool hdrEnabled)
{
    const ColorConversion target = hdrEnabled ? ColorConversion::BT2020_LimitedRange
                                              : ColorConversion::BT709_LimitedRange;
    if (this->conversionSettings.colorConversion == target)
        return;

    this->conversionSettings.colorConversion = target;

    if (!ui.created())
        return;

    {
        const QSignalBlocker blocker(ui.colorConversionComboBox);
        ui.colorConversionComboBox->setCurrentIndex(int(ColorConversionMapper.indexOf(target)));
    }

    this->currentImageIndex       = -1;
    this->currentImage_frameIndex = -1;
    this->setCacheInvalid();
    emit signalHandlerChanged(true, RECACHE_CLEAR);
}

void videoHandlerYUV::savePlaylist(YUViewDomElement &element) const
{
    FrameHandler::savePlaylist(element);
    element.appendProperiteChild("pixelFormat", this->getRawPixelFormatYUVName());

    auto ml = this->conversionSettings.mathParameters.at(Component::Luma);
    element.appendProperiteChild("math.luma.scale", QString::number(ml.scale));
    element.appendProperiteChild("math.luma.offset", QString::number(ml.offset));
    element.appendProperiteChild("math.luma.invert", to_string(ml.invert));

    auto mc = this->conversionSettings.mathParameters.at(Component::Chroma);
    element.appendProperiteChild("math.chroma.scale", QString::number(mc.scale));
    element.appendProperiteChild("math.chroma.offset", QString::number(mc.offset));
    element.appendProperiteChild("math.chroma.invert", to_string(mc.invert));

    element.appendProperiteChild("isp.enabled", to_string(this->conversionSettings.ispModeEnabled));
    element.appendProperiteChild("isp.bayerPattern",
                                 BayerPatternMapper.getName(this->conversionSettings.bayerPattern));
    element.appendProperiteChild("isp.cellSize",
                                 ISPCellSizeMapper.getName(this->conversionSettings.ispCellSize));
}

void videoHandlerYUV::loadPlaylist(const YUViewDomElement &element)
{
    FrameHandler::loadPlaylist(element);

    auto sourcePixelFormat = element.findChildValue("pixelFormat");
    this->setPixelFormatYUVByName(sourcePixelFormat);

    auto lumaScale = element.findChildValue("math.luma.scale");
    if (!lumaScale.isEmpty())
        this->conversionSettings.mathParameters[Component::Luma].scale = lumaScale.toInt();
    auto lumaOffset = element.findChildValue("math.luma.offset");
    if (!lumaOffset.isEmpty())
        this->conversionSettings.mathParameters[Component::Luma].offset = lumaOffset.toInt();
    this->conversionSettings.mathParameters[Component::Luma].invert =
            (element.findChildValue("math.luma.invert") == "True");

    auto chromaScale = element.findChildValue("math.chroma.scale");
    if (!chromaScale.isEmpty())
        this->conversionSettings.mathParameters[Component::Chroma].scale = chromaScale.toInt();
    auto chromaOffset = element.findChildValue("math.chroma.offset");
    if (!chromaOffset.isEmpty())
        this->conversionSettings.mathParameters[Component::Chroma].offset = chromaOffset.toInt();
    this->conversionSettings.mathParameters[Component::Chroma].invert =
            (element.findChildValue("math.chroma.invert") == "True");

    const auto ispEnabled = element.findChildValue("isp.enabled");
    if (!ispEnabled.isEmpty())
        this->conversionSettings.ispModeEnabled = (ispEnabled == "True");

    const auto bayerName = element.findChildValue("isp.bayerPattern");
    if (!bayerName.isEmpty())
    {
        if (const auto pattern = BayerPatternMapper.getValue(bayerName.toStdString()))
            this->conversionSettings.bayerPattern = *pattern;
    }

    const auto cellSizeName = element.findChildValue("isp.cellSize");
    if (!cellSizeName.isEmpty())
    {
        if (const auto cellSize = ISPCellSizeMapper.getValue(cellSizeName.toStdString()))
            this->conversionSettings.ispCellSize = *cellSize;
    }
}

} 
