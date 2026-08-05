/*  This file is part of YUView - The YUV player with advanced analytics toolset
 *   <https://github.com/IENT/YUView>
 *   Copyright (C) 2015  Institut für Nachrichtentechnik, RWTH Aachen University, GERMANY
 *
 *   YUView is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "YUVDataManager.h"

namespace video::yuv
{

YUVDataManager::YUVDataManager(QObject* parent)
    : QObject(parent)
{
}

int64_t YUVDataManager::getBytesPerFrame(const PixelFormatYUV& format, const Size& frameSize)
{
  if (!format.isValid())
    return 0;
  
  return format.bytesPerFrame(frameSize);
}

int64_t YUVDataManager::getCachingFrameSize(const PixelFormatYUV& format, 
                                            const Size& frameSize,
                                            bool useRawYUVCache)
{
  if (!format.isValid())
    return 0;
  
  if (useRawYUVCache)
  {
    // Raw YUV caching: store the raw YUV bytes
    return format.bytesPerFrame(frameSize);
  }
  else
  {
    // RGB caching: store BGRA image (4 bytes per pixel)
    return static_cast<int64_t>(frameSize.width) * frameSize.height * 4;
  }
}

bool YUVDataManager::shouldUseRawYUVCache(const PixelFormatYUV& format, bool hdrEnabled)
{
  if (!hdrEnabled)
    return false;
  
  // Only use raw YUV cache for 10-bit planar formats
  // These can be efficiently processed by GPU shaders
  if (!format.isPlanar())
    return false;
  
  const int bps = format.getBitsPerSample();
  if (bps != 10)
    return false;
  
  // Check for supported subsampling modes
  const auto subsampling = format.getSubsampling();
  return (subsampling == Subsampling::YUV_420 ||
          subsampling == Subsampling::YUV_422 ||
          subsampling == Subsampling::YUV_444);
}

QByteArray& YUVDataManager::prepareRawDataBuffer(const PixelFormatYUV& format, const Size& frameSize)
{
  const int64_t bytesNeeded = getBytesPerFrame(format, frameSize);
  
  if (m_rawData.size() < bytesNeeded)
    m_rawData.resize(bytesNeeded);
  
  return m_rawData;
}

} // namespace video::yuv
