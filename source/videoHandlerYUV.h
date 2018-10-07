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

#ifndef VIDEOHANDLERYUV_H
#define VIDEOHANDLERYUV_H

#include "videoHandler.h"
#include "ui_videoHandlerYUV.h"
#include "ui_videoHandlerYUV_CustomFormatDialog.h"

// The YUV_Internals namespace. We use this namespace because of the dialog. We want to be able to pass a yuvPixelFormat to the dialog and keep the
// global namespace clean but we are not able to use nested classes because of the Q_OBJECT macro. So the dialog and the yuvPixelFormat is inside
// of this namespace.
namespace YUV_Internals
{
  typedef enum
  {
    Luma = 0,
    Chroma = 1
  } Component;

  typedef enum
  {
    BT709_LimitedRange,
    BT709_FullRange,
    BT601_LimitedRange,
    BT601_FullRange,
    BT2020_LimitedRange,
    BT2020_FullRange,
  } ColorConversion;

  // How to perform up-sampling (chroma subsampling)
  typedef enum
  {
    NearestNeighborInterpolation,
    BiLinearInterpolation,
    InterstitialInterpolation
  } InterpolationMode;

  class yuvMathParameters
  {
  public:
    yuvMathParameters() : scale(1), offset(128), invert(false) {}
    yuvMathParameters(int scale, int offset, bool invert) : scale(scale), offset(offset), invert(invert) {}
    // Do we need to apply any transform to the raw YUV data before conversion to RGB?
    bool yuvMathRequired() const { return scale != 1 || invert; }

    int scale, offset;
    bool invert;
  };

  typedef enum
  {
    YUV_444,  // No subsampling
    YUV_422,  // Chroma: half horizontal resolution
    YUV_420,  // Chroma: half vertical and horizontal resolution
    YUV_440,  // Chroma: half vertical resolution
    YUV_410,  // Chroma: quarter vertical, quarter horizontal resolution
    YUV_411,  // Chroma: quarter horizontal resolution
    YUV_400,  // Luma only
    YUV_NUM_SUBSAMPLINGS
  } YUVSubsamplingType;

  typedef enum
  {
    Order_YUV,
    Order_YVU,
    Order_YUVA,
    Order_YVUA,
    Order_NUM
  } YUVPlaneOrder;

  typedef enum
  {
    Packing_YUV,      // 444
    Packing_YVU,      // 444
    Packing_AYUV,     // 444
    Packing_YUVA,     // 444
    Packing_UYVY,     // 422
    Packing_VYUY,     // 422
    Packing_YUYV,     // 422
    Packing_YVYU,     // 422
    //Packing_YYYYUV,   // 420
    //Packing_YYUYYV,   // 420
    //Packing_UYYVYY,   // 420
    //Packing_VYYUYY,   // 420
    Packing_NUM
  } YUVPackingOrder;

  // This class defines a specific YUV format with all properties like pixels per sample, subsampling of chroma
  // components and so on.
  class yuvPixelFormat
  {
  public:
    // The default constructor (will create an "Unknown Pixel Format")
    yuvPixelFormat();
    yuvPixelFormat(const QString &name);  // Set the pixel format by name. The name should have the format that is returned by getName().
    yuvPixelFormat(YUVSubsamplingType subsampling, int bitsPerSample, YUVPlaneOrder planeOrder=Order_YUV, bool bigEndian=false) : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(true), planeOrder(planeOrder), uvInterleaved(false) { setDefaultChromaOffset(); }
    yuvPixelFormat(YUVSubsamplingType subsampling, int bitsPerSample, YUVPackingOrder packingOrder, bool bytePacking, bool bigEndian=false) : subsampling(subsampling), bitsPerSample(bitsPerSample), bigEndian(bigEndian), planar(false), uvInterleaved(false), packingOrder(packingOrder), bytePacking(bytePacking) { setDefaultChromaOffset(); }
    bool isValid() const;
    int64_t bytesPerFrame(const QSize &frameSize) const;
    QString getName() const;
    int getSubsamplingHor() const;
    int getSubsamplingVer() const;
    void setDefaultChromaOffset(); // Set the default chroma offset values from the subsampling
    bool subsampled() const { return subsampling != YUV_444; }
    bool operator==(const yuvPixelFormat& a) const { return getName() == a.getName(); } // Comparing names should be enough since you are not supposed to create your own rgbPixelFormat instances anyways.
    bool operator!=(const yuvPixelFormat& a) const { return getName()!= a.getName(); }
    bool operator==(const QString& a) const { return getName() == a; }
    bool operator!=(const QString& a) const { return getName() != a; }

    YUVSubsamplingType subsampling;
    int bitsPerSample;
    bool bigEndian;
    bool planar;
    // The chroma offset in x and y direction. The vales (0...4) define the offsets [0, 1/2, 1, 3/2] samples towards the right and bottom.
    int chromaOffset[2];

    // if planar is set
    YUVPlaneOrder planeOrder;
    bool uvInterleaved;       //< If set, the UV (and A if present) planes are interleaved

    // if planar is not set
    YUVPackingOrder packingOrder;
    bool bytePacking;
  };

  class videoHandlerYUV_CustomFormatDialog : public QDialog, public Ui::CustomYUVFormatDialog
  {
    Q_OBJECT

  public:
    videoHandlerYUV_CustomFormatDialog(const yuvPixelFormat &yuvFormat);
    // This function provides the currently selected YUV format
    yuvPixelFormat getYUVFormat() const;
  private slots:
    void on_groupBoxPlanar_toggled(bool checked);
    void on_groupBoxPacked_toggled(bool checked) { groupBoxPlanar->setChecked(!checked); }
    void on_comboBoxChromaSubsampling_currentIndexChanged(int idx);
    void on_comboBoxBitDepth_currentIndexChanged(int idx);
  };

  // A (static) convenience QList class that handles the preset rgbPixelFormats
  class YUVFormatList : public QList<yuvPixelFormat>
  {
  public:
    // Default constructor. Fill the list with all the supported YUV formats.
    YUVFormatList();
    // Get all the YUV formats as a formatted list (for the drop down control)
    QStringList getFormattedNames();
    // Get the yuvPixelFormat with the given name
    yuvPixelFormat getFromName(const QString &name);
  };
}

/** The videoHandlerYUV can be used in any playlistItem to read/display YUV data. A playlistItem could even provide multiple YUV videos.
  * A videoHandlerYUV supports handling of YUV data and can return a specific frame as a image by calling getOneFrame.
  * All conversions from the various YUV formats to RGB are performed and handled here.
  */
class videoHandlerYUV : public videoHandler
{
  Q_OBJECT

public:
  videoHandlerYUV();
  ~videoHandlerYUV();

  // The format is valid if the frame width/height/pixel format are set
  virtual bool isFormatValid() const Q_DECL_OVERRIDE { return (frameHandler::isFormatValid() && canConvertToRGB(srcPixelFormat, frameSize)); }

  // Certain settings for a YUV source are invalid. In this case we will draw an error message instead of the image.
  virtual void drawFrame(QPainter *painter, int frameIdx, double zoomFactor, bool drawRawData) Q_DECL_OVERRIDE;

  // Return the YUV values for the given pixel
  // If a second item is provided, return the difference values to that item at the given position. If th second item
  // cannot be cast to a videoHandlerYUV, we call the frameHandler::getPixelValues function.
  virtual QStringPairList getPixelValues(const QPoint &pixelPos, int frameIdx, frameHandler *item2=nullptr, const int frameIdx1 = 0) Q_DECL_OVERRIDE;

  // Overload from playlistItemVideo. Calculate the difference of this playlistItemYuvSource
  // to another playlistItemVideo. If item2 cannot be converted to a playlistItemYuvSource,
  // we will use the playlistItemVideo::calculateDifference function to calculate the difference
  // using the RGB values.
  virtual QImage calculateDifference(frameHandler *item2, const int frameIdxItem0, const int frameIdxItem1, QList<infoItem> &differenceInfoList, const int amplificationFactor, const bool markDifference) Q_DECL_OVERRIDE;

  // Get the number of bytes for one YUV frame with the current format
  virtual int64_t getBytesPerFrame() const Q_DECL_OVERRIDE { return srcPixelFormat.bytesPerFrame(frameSize); }

  // If you know the frame size of the video, the file size (and optionally the bit depth) we can guess
  // the remaining values. The rate value is set if a matching format could be found.
  // If the sub format is "444" we will assume 4:4:4 input. Otherwise 4:2:0 will be assumed.
  virtual void setFormatFromSizeAndName(const QSize size, int bitDepth, int64_t fileSize, const QFileInfo &fileInfo) Q_DECL_OVERRIDE;

  // Try to guess and set the format (frameSize/srcPixelFormat) from the raw YUV data.
  // If a file size is given, it is tested if the YUV format and the file size match.
  virtual void setFormatFromCorrelation(const QByteArray &rawYUVData, int64_t fileSize=-1) Q_DECL_OVERRIDE;

  // Create the YUV controls and return a pointer to the layout.
  // yuvFormatFixed: For example a YUV file does not have a fixed format (the user can change this),
  // other sources might provide a fixed format which the user cannot change (HEVC file, ...)
  virtual QLayout *createVideoHandlerControls(bool isSizeFixed=false) Q_DECL_OVERRIDE;

  // Get the name of the currently selected YUV pixel format
  virtual QString getRawYUVPixelFormatName() const { return srcPixelFormat.getName(); }
  // Set the current YUV format and update the control. Only emit a signalHandlerChanged signal
  // if emitSignal is true.
  virtual void setYUVPixelFormatByName(const QString &name, bool emitSignal=false) { setYUVPixelFormat(YUV_Internals::yuvPixelFormat(name), emitSignal); }
  virtual void setYUVPixelFormat(const YUV_Internals::yuvPixelFormat &fmt, bool emitSignal=false);
  virtual void setYUVColorConversion(YUV_Internals::ColorConversion conversion);

  // When loading a videoHandlerYUV from playlist file, this can be used to set all the parameters at once
  void loadValues(const QSize &frameSize, const QString &sourcePixelFormat);

  // Draw the pixel values of the visible pixels in the center of each pixel. Only draw values for the given range of pixels.
  // Overridden from playlistItemVideo. This is a YUV source, so we can draw the YUV values.
  virtual void drawPixelValues(QPainter *painter, const int frameIdx, const QRect &videoRect, const double zoomFactor, frameHandler *item2 = nullptr, const bool markDifference = false, const int frameIdxItem1 = 0) Q_DECL_OVERRIDE;

  // Load the given frame and convert it to image. After this, currentFrameRawYUVData and currentFrame will
  // contain the frame with the given frame index.
  virtual void loadFrame(int frameIndex, bool loadToDoubleBuffer=false) Q_DECL_OVERRIDE;

  // If this is set, the pixel values drawn in the drawPixels function will be scaled according to the bit depth.
  // E.g: The bit depth is 8 and the pixel value is 127, then the value shown will be -1.
  bool showPixelValuesAsDiff {false};

  QByteArray getDiffYUV() const;

  YUV_Internals::yuvPixelFormat getDiffYUVFormat() const;

  bool getIs_YUV_diff() const;

protected:
  
  // How do we perform interpolation for the subsampled YUV formats?
  YUV_Internals::InterpolationMode interpolationMode;

  // Which components should we display
  typedef enum
  {
    DisplayAll,
    DisplayY,
    DisplayCb,
    DisplayCr
  } ComponentDisplayMode;
  ComponentDisplayMode componentDisplayMode;

  // How to convert from YUV to RGB
  // How to convert from YUV to RGB
  /*
  kr/kg/kb matrix (Rec. ITU-T H.264 03/2010, p. 379):
  R = Y                  + V*(1-Kr)
  G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg
  B = Y + U*(1-Kb)
  To respect value range of Y in [16:235] and U/V in [16:240], the matrix entries need to be scaled by 255/219 for Y and 255/112 for U/V
  In this software color conversion is performed with 16bit precision. Thus, further scaling with 2^16 is performed to get all factors as integers.
  */
  YUV_Internals::ColorConversion yuvColorConversionType;

  // Parameters for the YUV transformation (like scaling, invert, offset). For Luma ([0]) and chroma([1]).
  YUV_Internals::yuvMathParameters mathParameters[2];

  // The currently selected YUV format
  YUV_Internals::yuvPixelFormat srcPixelFormat;

  // A static list of preset YUV formats. These are the formats that are shown in the YUV format selection comboBox.
  YUV_Internals::YUVFormatList yuvPresetsList;

  // Get the YUV values for the given pixel.
  virtual void getPixelValue(const QPoint &pixelPos, unsigned int &Y, unsigned int &U, unsigned int &V);

  // Load the given frame and return it for caching. The current buffers (currentFrameRawYUVData and currentFrame)
  // will not be modified.
  virtual void loadFrameForCaching(int frameIndex, QImage &frameToCache) Q_DECL_OVERRIDE;

private:

  // Load the raw YUV data for the given frame index into currentFrameRawYUVData.
  // Return false is loading failed.
  bool loadRawYUVData(int frameIndex);

  // Convert from YUV (which ever format is selected) to image (RGB-888)
  void convertYUVToImage(const QByteArray &sourceBuffer, QImage &outputImage, const YUV_Internals::yuvPixelFormat &yuvFormat, const QSize &curFrameSize);

  // Set the new pixel format thread save (lock the mutex). We should also emit that something changed (can be disabled).
  void setSrcPixelFormat(YUV_Internals::yuvPixelFormat newFormat, bool emitChangedSignal=true);

  bool canConvertToRGB(YUV_Internals::yuvPixelFormat format, QSize imageSize, QString *whyNot=nullptr) const;

#if SSE_CONVERSION
  bool convertYUV420ToRGB(const byteArrayAligned &sourceBuffer, byteArrayAligned &targetBuffer);
#else
  bool convertYUV420ToRGB(const QByteArray &sourceBuffer, unsigned char *targetBuffer, const QSize &size, const YUV_Internals::yuvPixelFormat format);
#endif

  bool convertYUVPackedToPlanar(const QByteArray &sourceBuffer, QByteArray &targetBuffer, const QSize &frameSize, YUV_Internals::yuvPixelFormat &sourceBufferFormat);
  bool convertYUVPlanarToRGB(const QByteArray &sourceBuffer, unsigned char *targetBuffer, const QSize &frameSize, const YUV_Internals::yuvPixelFormat &sourceBufferFormat) const;
  bool markDifferencesYUVPlanarToRGB(const QByteArray &sourceBuffer, unsigned char *targetBuffer, const QSize &frameSize, const YUV_Internals::yuvPixelFormat &sourceBufferFormat) const;

#if SSE_CONVERSION_420_ALT
  void yuv420_to_argb8888(quint8 *yp, quint8 *up, quint8 *vp,
                          quint32 sy, quint32 suv,
                           int width, int height,
                           quint8 *rgb, quint32 srgb);
#endif

  SafeUi<Ui::videoHandlerYUV> ui;

  bool is_YUV_diff;
  QByteArray diffYUV;
  YUV_Internals::yuvPixelFormat diffYUVFormat;

private slots:

  // All the valueChanged() signals from the controls are connected here.
  void slotYUVControlChanged();
  // The YUV format combo box was changed
  void slotYUVFormatControlChanged(int idx);

};

#endif // VIDEOHANDLERYUV_H
