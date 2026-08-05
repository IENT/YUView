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

#include "PixelFormatYUV.h"
#include <QByteArray>
#include <QObject>

namespace video::yuv
{

/**
 * @brief YUV data management and loading controller
 * 
 * This class manages raw YUV data loading, caching decisions, and frame data lifecycle.
 * Extracted from videoHandlerYUV for Single Responsibility Principle compliance.
 * 
 * Key responsibilities:
 * - Raw YUV data buffer management
 * - Cache size calculations
 * - Cache mode decisions (RGB vs raw YUV)
 * - Frame data request coordination
 */
class YUVDataManager : public QObject
{
  Q_OBJECT

public:
  /**
   * @brief Construct a new YUV Data Manager
   * 
   * @param parent Parent QObject
   */
  explicit YUVDataManager(QObject* parent = nullptr);

  /**
   * @brief Get number of bytes per frame for the current format
   * 
   * Calculates the total bytes needed to store one YUV frame
   * based on the pixel format and frame size.
   * 
   * @param format YUV pixel format
   * @param frameSize Frame dimensions
   * @return Number of bytes per frame
   */
  static int64_t getBytesPerFrame(const PixelFormatYUV& format, const Size& frameSize);

  /**
   * @brief Calculate caching frame size
   * 
   * Returns the size of data to cache per frame, depending on cache mode.
   * For RGB cache mode: returns bytes for RGBA image
   * For raw YUV cache mode: returns bytes for raw YUV data
   * 
   * @param format YUV pixel format
   * @param frameSize Frame dimensions
   * @param useRawYUVCache Whether to use raw YUV caching
   * @return Bytes to cache per frame
   */
  static int64_t getCachingFrameSize(const PixelFormatYUV& format, 
                                     const Size& frameSize,
                                     bool useRawYUVCache);

  /**
   * @brief Check if raw YUV caching should be used
   * 
   * Determines whether to cache raw YUV data instead of converted RGB.
   * Raw YUV caching is preferred when:
   * - HDR 10-bit display is enabled
   * - Format is 10-bit planar
   * - GPU can handle YUV->RGB conversion
   * 
   * @param format YUV pixel format
   * @param hdrEnabled Whether HDR display is enabled
   * @return true if raw YUV cache should be used
   */
  static bool shouldUseRawYUVCache(const PixelFormatYUV& format, bool hdrEnabled);

  /**
   * @brief Prepare raw data buffer for loading
   * 
   * Ensures the internal buffer is sized correctly for the frame format.
   * 
   * @param format YUV pixel format
   * @param frameSize Frame dimensions
   * @return Reference to the prepared buffer
   */
  QByteArray& prepareRawDataBuffer(const PixelFormatYUV& format, const Size& frameSize);

  /**
   * @brief Get reference to current raw data
   * 
   * @return Const reference to raw YUV data buffer
   */
  const QByteArray& getRawData() const { return m_rawData; }

  /**
   * @brief Get mutable reference to raw data buffer
   * 
   * Used by data loading functions to fill the buffer.
   * 
   * @return Mutable reference to raw data buffer
   */
  QByteArray& rawDataBuffer() { return m_rawData; }

  /**
   * @brief Get frame index of currently loaded raw data
   * 
   * @return Frame index, or -1 if no valid data loaded
   */
  int getRawDataFrameIndex() const { return m_rawDataFrameIndex; }

  /**
   * @brief Set frame index of loaded raw data
   * 
   * @param frameIndex Frame index
   */
  void setRawDataFrameIndex(int frameIndex) { m_rawDataFrameIndex = frameIndex; }

  /**
   * @brief Invalidate cached raw data
   * 
   * Called when format changes or cache needs to be refreshed.
   */
  void invalidateRawData() { m_rawDataFrameIndex = -1; }

  /**
   * @brief Check if valid raw data is loaded
   * 
   * @param frameIndex Expected frame index
   * @return true if raw data for the specified frame is available
   */
  bool hasValidRawData(int frameIndex) const { return m_rawDataFrameIndex == frameIndex; }

signals:
  /**
   * @brief Request raw data load from source
   * 
   * Emitted when raw frame data needs to be loaded from the data source.
   * Connected to playlistItem's data loading mechanism.
   * 
   * @param frameIndex Frame index to load
   */
  void requestRawDataLoad(int frameIndex);

private:
  QByteArray m_rawData;          ///< Buffer for raw YUV frame data
  int m_rawDataFrameIndex{-1};   ///< Frame index of currently loaded data
};

} // namespace video::yuv
