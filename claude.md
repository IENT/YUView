### Program background
At its core, YUView is a YUV player and analysis tool using **QT framework**.

### Our GOALs:
Implement native 10-bit display support in YUView.

### Conclusion:
A comprehensive analysis, undertaken by a colleague, has concluded that the implementation of native 10-bit display support within YUView is technically feasible. The project, however, entails a significant volume of code modification, which in turn requires the allocation of a senior C++ developer for a timeframe projected to be in excess of one month.

### I. Inheritance Relationship Between Objects

```
videoHandlerYUV <- videoHandler -< FrameHandler
```

### II. Important Class Members and variables

*   **`videoHandler`:**
    *   `QImage doubleBufferImage;`
*   **`FrameHandler`**
    *   `QImage currentImage;`

### III. YUV -> RGB Flow

**Step 1: YUV420 -> RGB Fast Conversion Algorithm**

The file `YUViewLib\src\video\yuv\videoHandlerYUV.cpp` is responsible for implementing the YUV420 to RGB fast conversion algorithm:

```cpp
template <int bitDepth>
bool convertYUV420ToRGB(const QByteArray &sourceBuffer,
                        unsigned char *targetBuffer,
                        const Size &size,
                        const PixelFormatYUV &format,
                        const ConversionSettings &conversionSettings)
```

*   This function uses template specialization to determine the processing logic for YUV precision. For 8-bit files, it directly uses the original YUV values for the YUV to RGB conversion. For 10-bit files, it right-shifts the YUV values by two bits to approximate an 8-bit data conversion.
*   It processes a 2x2 YUV unit in parallel at a time. The function employs several optimization techniques to improve processing efficiency, such as using bitwise shifts and integer arithmetic instead of floating-point operations, and lookup tables instead of branching structures.

The conversion table is defind in  `YUViewLib\src\video\yuv\PixelFormatYUV.cpp`:
```cpp

void getColorConversionCoefficients(ColorConversion colorConversion, int RGBConv[5])
{
  // The conversion parameters for the components of the different supported YUV->RGB conversions
  // The first index is the index of the ColorConversion enum. The second index is [Y, cRV, cGU,
  // cGV, cBU].
  const int yuvRgbConvCoeffs[6][5] = {
      {76309, 117489, -13975, -34925, 138438}, // BT709_LimitedRange
      {65536, 103206, -12276, -30679, 121608}, // BT709_FullRange
      {76309, 104597, -25675, -53279, 132201}, // BT601_LimitedRange
      {65536, 91881, -22553, -46802, 116129},  // BT601_FullRange
      {76309, 110013, -12276, -42626, 140363}, // BT2020_LimitedRange
      {65536, 96638, -10783, -37444, 123299}   // BT2020_FullRange
  };
  const auto index = ColorConversionMapper.indexOf(colorConversion);
  for (unsigned i = 0; i < 5; i++)
    RGBConv[i] = yuvRgbConvCoeffs[index][i];
}
```

and in :
```cpp
enum class ColorConversion
{
  BT709_LimitedRange,
  BT709_FullRange,
  BT601_LimitedRange,
  BT601_FullRange,
  BT2020_LimitedRange,
  BT2020_FullRange,
};

constexpr EnumMapper<ColorConversion, 6> ColorConversionMapper = {
    std::make_pair(ColorConversion::BT709_LimitedRange, "ITU-R.BT709"),
    std::make_pair(ColorConversion::BT709_FullRange, "ITU-R.BT709 Full Range"),
    std::make_pair(ColorConversion::BT601_LimitedRange, "ITU-R.BT601"),
    std::make_pair(ColorConversion::BT601_FullRange, "ITU-R.BT601 Full Range"),
    std::make_pair(ColorConversion::BT2020_LimitedRange, "ITU-R.BT2020"),
    std::make_pair(ColorConversion::BT2020_FullRange, "ITU-R.BT2020 Full Range")};
```
**Step 2: YUV to Image Conversion Function**

The `convertYUVToImage` function determines which conversion algorithm to use based on the YUV format:

```cpp
void convertYUVToImage(const QByteArray &sourceBuffer,
                       QImage &outputImage,
                       const PixelFormatYUV &yuvFormat,
                       const Size &curFrameSize,
                       const ConversionSettings &conversionSettings)
```

*   If it detects a YUV420 8-bit or 10-bit planar format, it calls the `convertYUV420ToRGB` fast algorithm mentioned above.
*   Otherwise, it calls the regular `convertYUVPlanarToRGB` algorithm.
*   Regardless of which path is taken, when processing precisions above 8-bit, the last 2 bits of precision are truncated.
*   After this step, the output parameter is an 8-bit RGBA format `QImage`, where the alpha component is always 255.

NOTE: Your task is to retain the 10 bit precision during the YUV->RGB conversion instead of truncated the last 2-bitDepth, remember to modify the **BOTH two branches**

### IV. RGB Rendering Flow

**Step 1: Loading Frame Data**

The `loadFrame` method in `YUViewLib\src\video\yuv\videoHandlerYUV.cpp` reads the raw YUV data of the current frame into the `QByteArray` `currentFrameRawData`.

```cpp
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

  // The data in currentFrameRawData is now up to date. If necessary
  // convert the data to RGB.
  if (loadToDoubleBuffer)
  {
    QImage newImage;
    convertYUVToImage(this->currentFrameRawData,
                      newImage,
                      this->srcPixelFormat,
                      this->frameSize,
                      this->conversionSettings);
    doubleBufferImage           = newImage;
    doubleBufferImageFrameIndex = frameIndex;
  }
  else if (currentImageIndex != frameIndex)
  {
    QImage newImage;
    convertYUVToImage(this->currentFrameRawData,
                      newImage,
                      this->srcPixelFormat,
                      this->frameSize,
                      this->conversionSettings);
    QMutexLocker setLock(&currentImageSetMutex);
    currentImage      = newImage;
    currentImageIndex = frameIndex;
  }
}
```

*   If the background caching option is enabled, the currently generated frame is written into the buffer. When the user reads this frame, the cached `doubleBufferImage` from `videoHandler` is loaded directly.
*   If the current frame is being displayed, it is loaded into the `currentImage` of `FrameHandler`.

**Step 2: Frame Drawing**

The `drawFrame` method of `FrameHandler` or `videoHandler` (using virtual functions for polymorphism, the specific method to be called is determined at runtime) is responsible for drawing the `currentImage` (`QImage` class) to the screen. The `drawPixelValues` method can overlay the original YUV values on the image, and the `zoomFactor` parameter can control the image's zoom level.

```cpp
void FrameHandler::drawFrame(QPainter *painter, double zoomFactor, bool drawRawValues)
{
  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(QSize(frameSize.width * zoomFactor, frameSize.height * zoomFactor));
  videoRect.moveCenter(QPoint(0, 0));

  // Draw the current image (currentFrame)
  painter->drawImage(videoRect, this->currentImage);

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, 0, videoRect, zoomFactor);
  }
}
```

*   During drawing, `QPainter`, which is the core class in Qt for low-level drawing operations, scales or stretches the entire `currentImage` (`QImage` class) to fit the size of `videoRect`, thus displaying the final image on the screen.
*   The `drawFrame` method is triggered when the user changes the image zoom level, drags the frame number, or opens a new image.

### V. QImage Supported Display Types

*   **`QImage::Format_ARGB32_Premultiplied`**: The type currently used by the program on Windows systems. It is a premultiplied alpha format, meaning each color component (R, G, B) value has been pre-multiplied by the Alpha channel's value.
*   **`QImage::Format_RGBX64`**: Supports 16-bit, with the alpha component always being 65535.
*   **`QImage::Format_RGBA64_Premultiplied`**: A 16-bit version of the premultiplied alpha format image, which is the most likely format for our new development.
*   **`QImage::Format_BGR30`**: Each of RGB is 10-bit, with no alpha channel and 2 bits reserved.
*   **`QImage::Format_A2BGR30_Premultiplied`**: Similar to `Format_BGR30`, but with a 2-bit alpha channel, allowing for 4 levels of transparency.

Reference: [qt.developpez.com](http://qt.developpez.com)

### VI. Plan for Implementing 10-bit Native Display Support

To natively support 10-bit image viewing in YUView, significant changes are required across multiple files. Here is a breakdown of the necessary modifications.

#### 1. UI Changes

*   A "10bit display support" checkbox or button needs to be added to the UI, for example in the `Settings` or `View` menu.
*   This UI element will control a new setting, let's call it `enable10BitDisplay`.
*   The state of this setting will be used to conditionally switch between the existing 8-bit rendering pipeline and the new 10-bit pipeline.

#### 2. Modifying `convertYUVToImage`

This function acts as a dispatcher and needs to be updated to handle the new 10-bit path.

**File:** `YUViewLib\src\video\yuv\videoHandlerYUV.cpp`

```cpp
void convertYUVToImage(const QByteArray         &sourceBuffer,
                       QImage                   &outputImage,
                       const PixelFormatYUV     &yuvFormat,
                       const Size               &curFrameSize,
                       const ConversionSettings &conversionSettings,
                       bool                     enable10BitDisplay) // New parameter
{
    // If 10-bit display is enabled and the source is a 10-bit format
    if (enable10BitDisplay && yuvFormat.getBitDepth() == 10)
    {
        // Allocate a QImage with a 16-bit format
        outputImage = QImage(curFrameSize.width, curFrameSize.height, QImage::Format_RGBA64_Premultiplied);

        // Check if it's YUV420 Planar
        if (yuvFormat.getChromaSubsampling() == ChromaSubsampling::I420 && yuvFormat.getPlaneOrder() != PlaneOrder::packed)
        {
            // Call the new 10-bit to 16-bit RGB conversion function
            convertYUV420ToRGB16Bit(sourceBuffer,
                                    outputImage.bits(),
                                    curFrameSize,
                                    yuvFormat,
                                    conversionSettings);
        }
        else
        {
            // Call a generic 10-bit to 16-bit RGB conversion for other formats
            convertYUVPlanarToRGB16Bit(sourceBuffer,
                                       outputImage.bits(),
                                       curFrameSize,
                                       yuvFormat,
                                       conversionSettings);
        }
    }
    else
    {
        // Existing 8-bit conversion logic
        // If it's YUV420 8-bit or 10-bit (downsampled), use the fast algorithm
        if (yuvFormat.getChromaSubsampling() == ChromaSubsampling::I420 &&
            yuvFormat.getPlaneOrder() != PlaneOrder::packed &&
            (yuvFormat.getBitDepth() == 8 || yuvFormat.getBitDepth() == 10))
        {
             outputImage = QImage(curFrameSize.width, curFrameSize.height, QImage::Format_ARGB32_Premultiplied);
             if(yuvFormat.getBitDepth() == 8)
             {
                convertYUV420ToRGB<8>(sourceBuffer, outputImage.bits(), ...);
             }
             else // 10-bit is down-sampled
             {
                convertYUV420ToRGB<10>(sourceBuffer, outputImage.bits(), ...);
             }
        }
        else
        {
             // Fallback to the generic planar converter which also truncates to 8-bit
             convertYUVPlanarToRGB(sourceBuffer, outputImage, ...);
        }
    }
}
```

*   A new boolean parameter `enable10BitDisplay` is added.
*   When `enable10BitDisplay` is true and the source is 10-bit, the function will create a `QImage` with a 16-bit format like `QImage::Format_RGBA64_Premultiplied`.
*   This will require new conversion functions (`convertYUV420ToRGB16Bit`, `convertYUVPlanarToRGB16Bit`) that output 16-bit RGB data instead of 8-bit.

#### 3. Modifying `convertYUV420ToRGB` for Native 10-bit Support

A new function, `convertYUV420ToRGB16Bit`, must be created. It will be based on `convertYUV420ToRGB` but will be modified to handle 10-bit data without precision loss and output to a 16-bit buffer.

**File:** `YUViewLib\src\video\yuv\videoHandlerYUV.cpp`



*   This new function will **not** perform the `>> 2` bit shift on the 10-bit YUV input data.
*   The YUV to RGB conversion math must be adjusted for 10-bit inputs.
*   The output RGB values must be scaled to fit the 16-bit range (0-65535). A simple left shift by 6 (`<< 6`) can be a starting point.
*   The `targetBuffer` will be treated as an array of `quint16`.
*   The alpha channel for `QImage::Format_RGBA64_Premultiplied` should be set to the maximum value (65535).
*   After the succession of loading 16-bit QImage, the next phase of the project is to implement native 10-bit image display in HDR mode, specifically for systems where Windows HDR is supported.

#### 4. Updating `videoHandlerYUV::loadFrame`

This method will need to pass the new `enable10BitDisplay` setting to `convertYUVToImage`.

**File:** `YUViewLib\src\video\yuv\videoHandlerYUV.cpp`

```cpp
void videoHandlerYUV::loadFrame(int frameIndex, bool loadToDoubleBuffer)
{
    // ... (existing code to load raw YUV data) ...

    // Get the state of the new UI setting
    bool use10BitDisplay = Settings::instance()->getEnable10BitDisplay(); // Assuming a global settings object

    if (loadToDoubleBuffer)
    {
        QImage newImage;
        convertYUVToImage(this->currentFrameRawData,
                          newImage,
                          this->srcPixelFormat,
                          this->frameSize,
                          this->conversionSettings,
                          use10BitDisplay); // Pass the setting
        doubleBufferImage           = newImage;
        doubleBufferImageFrameIndex = frameIndex;
    }
    else if (currentImageIndex != frameIndex)
    {
        QImage newImage;
        convertYUVToImage(this->currentFrameRawData,
                          newImage,
                          this->srcPixelFormat,
                          this->frameSize,
                          this->conversionSettings,
                          use10BitDisplay); // Pass the setting
        QMutexLocker setLock(&currentImageSetMutex);
        currentImage      = newImage;
        currentImageIndex = frameIndex;
    }
}
```

*   The `loadFrame` method must retrieve the `enable10BitDisplay` setting.
*   It then passes this setting down to `convertYUVToImage`.
*   The class members `doubleBufferImage` and `currentImage` will now hold either an 8-bit or 16-bit `QImage`, but the rest of the code that uses these `QImage` objects should handle them transparently thanks to Qt's `QImage` and `QPainter` classes.

#### 5. Verifying Rendering in `FrameHandler::drawFrame`

**File:** (Likely `YUViewLib\src\frame\FrameHandler.cpp`)

The `drawFrame` method should work without modification, as `QPainter::drawImage` is capable of rendering various `QImage` formats, including `QImage::Format_RGBA64_Premultiplied`.

```cpp
void FrameHandler::drawFrame(QPainter *painter, double zoomFactor, bool drawRawValues)
{
  // ...
  // This call should correctly render both 8-bit and 16-bit QImages.
  painter->drawImage(videoRect, this->currentImage);
  // ...
}
```

However, the `drawPixelValues` method, which likely reads pixel data directly, will need to be updated to handle the 16-bit `QImage` format when it is active. It will need to read `quint16` values instead of `quint8` and display them appropriately.

### Workflow
- Be sure to typecheck when you’re done making a series of code changes

### VII. Summary of Development Effort

As correctly concluded, this is a major undertaking.

*   **Core Logic:** Creating new, optimized 10-bit to 16-bit conversion routines (`convertYUV420ToRGB16Bit`) is complex and time-consuming. It requires careful handling of data types, color conversion math, and performance optimization.
*   **Code Refactoring:** The changes need to be propagated through several layers of the application, from the UI settings down to the low-level conversion functions.
*   **Dependency Management:** The inheritance structure means that changes in base classes (`FrameHandler`, `videoHandler`) must be compatible with derived classes (`videoHandlerYUV`).
*   **Testing:** Thorough testing will be required to ensure that:
    *   The 10-bit display is accurate.
    *   There is no performance regression in the existing 8-bit path.
    *   Switching between the two modes works seamlessly.
    *   Features like `drawPixelValues` and zooming work correctly with both image formats.

The estimated development cycle of **over one month** is a reasonable assessment for a single developer to properly implement and test this feature.
