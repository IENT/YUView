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

#include "ChromaResampling.h"

namespace video::yuv::conversion
{

namespace
{

// Depending on offsetX8 (which can be 1 to 7), interpolate one of the 6 given positions between
// prev and cur.
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
  Q_ASSERT(false); // offsetX8 should always be between 1 and 7 (inclusive)
  return 0;
}

} // namespace

// Re-sample the chroma component so that the chroma samples and the luma samples are aligned after
// this operation.
void UVPlaneResamplingChromaOffset(const PixelFormatYUV &pixelFormat,
                                   const Size            frameSize,
                                   ConstDataPointer      srcU,
                                   ConstDataPointer      srcV,
                                   DataPointer           dstU,
                                   DataPointer           dstV)
{
  const auto subsampling  = pixelFormat.getSubsampling();
  const auto chromaOffset = pixelFormat.getChromaOffset();

  const auto widthChroma  = frameSize.width / pixelFormat.getSubsamplingHor();
  const auto heightChroma = frameSize.height / pixelFormat.getSubsamplingVer();

  // We can perform linear interpolation for 7 positions (6 in between) two pixels.
  // Which of these position is needed depends on the chromaOffset and the subsampling.
  const auto possibleValsX = getMaxPossibleChromaOffsetValues(true, subsampling);
  const auto possibleValsY = getMaxPossibleChromaOffsetValues(false, subsampling);
  const int  offsetX8      = (possibleValsX == 1)   ? chromaOffset.x * 4
                             : (possibleValsX == 3) ? chromaOffset.x * 2
                                                    : chromaOffset.x;
  const int  offsetY8      = (possibleValsY == 1)   ? chromaOffset.y * 4
                             : (possibleValsY == 3) ? chromaOffset.y * 2
                                                    : chromaOffset.y;

  const auto bigEndian       = pixelFormat.isBigEndian();
  const auto bitsPerSample   = pixelFormat.getBitsPerSample();
  const auto strideInSamples = getChromaStrideInSamples(pixelFormat, frameSize);
  const auto strideInBytes   = strideInSamples * pixelFormat.getBytesPerSample();
  const auto offsetToNextValue =
      (pixelFormat.getChromaPacking() == ChromaPacking::PerValue ? 2 : 1);

  if (offsetX8 != 0)
  {
    // Perform horizontal re-sampling
    for (int y = 0; y < heightChroma; y++)
    {
      // On the left side, there is no previous sample, so the first value is never changed.
      const int srcIdx = y * strideInBytes;
      int       prevU  = getValueFromSource(srcU, srcIdx, bitsPerSample, bigEndian);
      int       prevV  = getValueFromSource(srcV, srcIdx, bitsPerSample, bigEndian);
      setValueInBuffer(dstU, prevU, y * strideInBytes, bitsPerSample, bigEndian);
      setValueInBuffer(dstV, prevV, y * strideInBytes, bitsPerSample, bigEndian);

      for (int x = 0; x < widthChroma - 1; x++)
      {
        // Calculate the new current value using the previous and the current value
        const int srcIdxInLine = srcIdx + (x + 1) * offsetToNextValue;
        int       curU         = getValueFromSource(srcU, srcIdxInLine, bitsPerSample, bigEndian);
        int       curV         = getValueFromSource(srcV, srcIdxInLine, bitsPerSample, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetX8);
        int newV = interpolateUV8Pos(prevV, curV, offsetX8);
        setValueInBuffer(dstU, newU, y * strideInBytes + x, bitsPerSample, bigEndian);
        setValueInBuffer(dstV, newV, y * strideInBytes + x, bitsPerSample, bigEndian);

        prevU = curU;
        prevV = curV;
      }
    }
  }

  // For the second step, use the filtered values (or the source if no filtering was applied)
  const auto srcUStep2              = (offsetX8 == 0) ? srcU : dstU;
  const auto srcVStep2              = (offsetX8 == 0) ? srcV : dstV;
  const auto offsetToNextValueStep2 = (offsetX8 == 0) ? offsetToNextValue : 1;

  if (offsetY8 != 0)
  {
    // Perform vertical re-sampling. It works exactly like horizontal up-sampling but x and y are
    // switched.
    for (int x = 0; x < widthChroma; x++)
    {
      // On the top, there is no previous sample, so the first value is never changed.
      auto prevU =
          getValueFromSource(srcUStep2, x * offsetToNextValueStep2, bitsPerSample, bigEndian);
      auto prevV =
          getValueFromSource(srcVStep2, x * offsetToNextValueStep2, bitsPerSample, bigEndian);
      setValueInBuffer(dstU, prevU, x, bitsPerSample, bigEndian);
      setValueInBuffer(dstV, prevV, x, bitsPerSample, bigEndian);

      for (int y = 0; y < heightChroma - 1; y++)
      {
        // Calculate the new current value using the previous and the current value
        const int srcIdx = (y + 1) * widthChroma + x;
        int       curU   = getValueFromSource(
            srcUStep2, srcIdx * offsetToNextValueStep2, bitsPerSample, bigEndian);
        int curV = getValueFromSource(
            srcVStep2, srcIdx * offsetToNextValueStep2, bitsPerSample, bigEndian);

        // Perform interpolation and save the value for the current UV value. Goto next value.
        int newU = interpolateUV8Pos(prevU, curU, offsetY8);
        int newV = interpolateUV8Pos(prevV, curV, offsetY8);
        setValueInBuffer(dstU, newU, srcIdx, bitsPerSample, bigEndian);
        setValueInBuffer(dstV, newV, srcIdx, bitsPerSample, bigEndian);

        prevU = curU;
        prevV = curV;
      }
    }
  }
}

} // namespace video::yuv::conversion
