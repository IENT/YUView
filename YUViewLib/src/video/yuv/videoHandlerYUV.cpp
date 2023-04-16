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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <vector>

#if SSE_CONVERSION_420_ALT
#include <xmmintrin.h>
#endif
#include <QDir>
#include <QPainter>

#include <common/FileInfo.h>
#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <video/LimitedRangeToFullRange.h>
#include <video/yuv/PixelFormatYUVGuess.h>
#include <video/yuv/videoHandlerYUVCustomFormatDialog.h>

namespace video::yuv
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define VIDEOHANDLERYUV_DEBUG_LOADING 0
#if VIDEOHANDLERYUV_DEBUG_LOADING && !NDEBUG
#include <QDebug>
#define DEBUG_YUV(message) qDebug() << message;
#else
#define DEBUG_YUV(message) ((void)0)
#endif

namespace
{

// Compute the MSE between the given char sources for numPixels bytes
template <typename T> double computeMSE(T ptr, T ptr2, int numPixels)
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

yuva_t
getPixelValueV210(const QByteArray &sourceBuffer, const Size &curFrameSize, const QPoint &pixelPos)
{
  auto widthRoundUp = (((curFrameSize.width + 48 - 1) / 48) * 48);
  auto strideIn     = widthRoundUp / 6 * 16;

  auto startInBuffer = (unsigned(pixelPos.y()) * strideIn) + unsigned(pixelPos.x()) / 6 * 16;

  const unsigned char *restrict src = (unsigned char *)sourceBuffer.data();

  yuva_t ret;
  auto   xSub = unsigned(pixelPos.x()) % 6;
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

inline int interpolateUVSample(const ChromaInterpolation mode, const int sample1, const int sample2)
{
  if (mode == ChromaInterpolation::Bilinear)
    // Interpolate linearly between sample1 and sample2
    return ((sample1 + sample2) + 1) >> 1;
  return sample1; // Sample and hold
}

// Convert the given raw YUV data in sourceBuffer (using srcPixelFormat) to image (RGB-888), using
// the buffer tmpRGBBuffer for intermediate RGB values.
void convertYUVToImage(const QByteArray &                    sourceBuffer,
                       QImage &                              outputImage,
                       const PixelFormatYUV &                yuvFormat,
                       const Size &                          curFrameSize,
                       const conversion::ConversionSettings &conversionSettings)
{
  if (!yuvFormat.canConvertToRGB(curFrameSize) || sourceBuffer.isEmpty())
  {
    outputImage = QImage();
    return;
  }

  DEBUG_YUV("videoHandlerYUV::convertYUVToImage");

  // Create the output image in the right format.
  // In both cases, we will set the alpha channel to 255. The format of the raw buffer is: BGRA
  // (each 8 bit). Internally, this is how QImage allocates the number of bytes per line (with depth
  // = 32): const int bytes_per_line = ((width * depth + 31) >> 5) << 2; // bytes per scanline (must
  // be multiple of 4)
  auto qFrameSize          = QSize(int(curFrameSize.width), int(curFrameSize.height));
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

  auto src = reinterpret_cast<conversion::ConstDataPointer>(sourceBuffer.data());
  conversion::convertYUVToARGB(src, conversionSettings, outputImage.bits());

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

videoHandlerYUV::videoHandlerYUV() : videoHandler()
{
  // Set the default YUV transformation parameters.
  this->mathParametersPerComponent[conversion::LUMA]   = MathParameters(1, 125, false);
  this->mathParametersPerComponent[conversion::CHROMA] = MathParameters(1, 128, false);

  // If we know nothing about the YUV format, assume YUV 4:2:0 8 bit planar by default.
  this->srcPixelFormat = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV);

  this->presetList.append(PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV)); // YUV 4:2:0
  this->presetList.append(
      PixelFormatYUV(Subsampling::YUV_420, 10, PlaneOrder::YUV)); // YUV 4:2:0 10 bit
  this->presetList.append(PixelFormatYUV(Subsampling::YUV_422, 8, PlaneOrder::YUV)); // YUV 4:2:2
  this->presetList.append(PixelFormatYUV(Subsampling::YUV_444, 8, PlaneOrder::YUV)); // YUV 4:4:4
  for (auto e : PredefinedPixelFormatMapper.getEnums())
    this->presetList.append(e);
}

videoHandlerYUV::~videoHandlerYUV()
{
  DEBUG_YUV("videoHandlerYUV destruction");
}

unsigned videoHandlerYUV::getCachingFrameSize() const
{
  auto hasAlpha = this->srcPixelFormat.hasAlpha();
  auto bytes    = functionsGui::bytesPerPixel(functionsGui::platformImageFormat(hasAlpha));
  return this->frameSize.width * this->frameSize.height * bytes;
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
  std::string msg;
  if (!srcPixelFormat.canConvertToRGB(frameSize, &msg))
  {
    // The conversion to RGB can not be performed. Draw a text about this
    msg = "With the given settings, the YUV data can not be converted to RGB:\n" + msg;
    // Get the size of the text and create a QRect of that size which is centered at (0,0)
    QFont displayFont = painter->font();
    displayFont.setPointSizeF(painter->font().pointSizeF() * zoomFactor);
    painter->setFont(displayFont);
    QSize textSize = painter->fontMetrics().size(0, QString::fromStdString(msg));
    QRect textRect;
    textRect.setSize(textSize);
    textRect.moveCenter(QPoint(0, 0));

    // Draw the text
    painter->drawText(textRect, QString::fromStdString(msg));
  }
  else
    videoHandler::drawFrame(painter, frameIdx, zoomFactor, drawRawData);
}

/// --- Convert from the current YUV input format to YUV 444

#if SSE_CONVERSION_420_ALT
void videoHandlerYUV::yuv420_to_argb8888(quint8 *yp,
                                         quint8 *up,
                                         quint8 *vp,
                                         quint32 sy,
                                         quint32 suv,
                                         int     width,
                                         int     height,
                                         quint8 *rgb,
                                         quint32 srgb)
{
  __m128i  y0r0, y0r1, u0, v0;
  __m128i  y00r0, y01r0, y00r1, y01r1;
  __m128i  u00, u01, v00, v01;
  __m128i  rv00, rv01, gu00, gu01, gv00, gv01, bu00, bu01;
  __m128i  r00, r01, g00, g01, b00, b01;
  __m128i  rgb0123, rgb4567, rgb89ab, rgbcdef;
  __m128i  gbgb;
  __m128i  ysub, uvsub;
  __m128i  zero, facy, facrv, facgu, facgv, facbu;
  __m128i *srcy128r0, *srcy128r1;
  __m128i *dstrgb128r0, *dstrgb128r1;
  __m64 *  srcu64, *srcv64;

  //    Implement the following conversion:
  //    B = 1.164(Y - 16)                   + 2.018(U - 128)
  //    G = 1.164(Y - 16) - 0.813(V - 128)  - 0.391(U - 128)
  //    R = 1.164(Y - 16) + 1.596(V - 128)

  int x, y;
  // constants
  ysub  = _mm_set1_epi32(0x00100010); // value 16 for subtraction
  uvsub = _mm_set1_epi32(0x00800080); // value 128

  // multiplication factors bit shifted by 6
  facy  = _mm_set1_epi32(0x004a004a);
  facrv = _mm_set1_epi32(0x00660066);
  facgu = _mm_set1_epi32(0x00190019);
  facgv = _mm_set1_epi32(0x00340034);
  facbu = _mm_set1_epi32(0x00810081);

  zero = _mm_set1_epi32(0x00000000);

  for (y = 0; y < height; y += 2)
  {
    srcy128r0 = (__m128i *)(yp + sy * y);
    srcy128r1 = (__m128i *)(yp + sy * y + sy);
    srcu64    = (__m64 *)(up + suv * (y / 2));
    srcv64    = (__m64 *)(vp + suv * (y / 2));

    // dst row 0 and row 1
    dstrgb128r0 = (__m128i *)(rgb + srgb * y);
    dstrgb128r1 = (__m128i *)(rgb + srgb * y + srgb);

    for (x = 0; x < width; x += 16)
    {
      u0 = _mm_loadl_epi64((__m128i *)srcu64);
      srcu64++;
      v0 = _mm_loadl_epi64((__m128i *)srcv64);
      srcv64++;

      y0r0 = _mm_load_si128(srcy128r0++);
      y0r1 = _mm_load_si128(srcy128r1++);

      // expand to 16 bit, subtract and multiply constant y factors
      y00r0 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(y0r0, zero), ysub), facy);
      y01r0 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(y0r0, zero), ysub), facy);
      y00r1 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpacklo_epi8(y0r1, zero), ysub), facy);
      y01r1 = _mm_mullo_epi16(_mm_sub_epi16(_mm_unpackhi_epi8(y0r1, zero), ysub), facy);

      // expand u and v so they're aligned with y values
      u0  = _mm_unpacklo_epi8(u0, zero);
      u00 = _mm_sub_epi16(_mm_unpacklo_epi16(u0, u0), uvsub);
      u01 = _mm_sub_epi16(_mm_unpackhi_epi16(u0, u0), uvsub);

      v0  = _mm_unpacklo_epi8(v0, zero);
      v00 = _mm_sub_epi16(_mm_unpacklo_epi16(v0, v0), uvsub);
      v01 = _mm_sub_epi16(_mm_unpackhi_epi16(v0, v0), uvsub);

      // common factors on both rows.
      rv00 = _mm_mullo_epi16(facrv, v00);
      rv01 = _mm_mullo_epi16(facrv, v01);
      gu00 = _mm_mullo_epi16(facgu, u00);
      gu01 = _mm_mullo_epi16(facgu, u01);
      gv00 = _mm_mullo_epi16(facgv, v00);
      gv01 = _mm_mullo_epi16(facgv, v01);
      bu00 = _mm_mullo_epi16(facbu, u00);
      bu01 = _mm_mullo_epi16(facbu, u01);

      // add together and bit shift to the right
      r00 = _mm_srai_epi16(_mm_add_epi16(y00r0, rv00), 6);
      r01 = _mm_srai_epi16(_mm_add_epi16(y01r0, rv01), 6);
      g00 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y00r0, gu00), gv00), 6);
      g01 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y01r0, gu01), gv01), 6);
      b00 = _mm_srai_epi16(_mm_add_epi16(y00r0, bu00), 6);
      b01 = _mm_srai_epi16(_mm_add_epi16(y01r0, bu01), 6);

      r00 = _mm_packus_epi16(r00, r01);
      g00 = _mm_packus_epi16(g00, g01);
      b00 = _mm_packus_epi16(b00, b01);

      // shuffle back together to lower 0rgb0rgb...
      r01     = _mm_unpacklo_epi8(r00, zero);  // 0r0r...
      gbgb    = _mm_unpacklo_epi8(b00, g00);   // gbgb...
      rgb0123 = _mm_unpacklo_epi16(gbgb, r01); // lower 0rgb0rgb...
      rgb4567 = _mm_unpackhi_epi16(gbgb, r01); // upper 0rgb0rgb...

      // shuffle back together to upper 0rgb0rgb...
      r01     = _mm_unpackhi_epi8(r00, zero);
      gbgb    = _mm_unpackhi_epi8(b00, g00);
      rgb89ab = _mm_unpacklo_epi16(gbgb, r01);
      rgbcdef = _mm_unpackhi_epi16(gbgb, r01);

      // write to dst
      _mm_store_si128(dstrgb128r0++, rgb0123);
      _mm_store_si128(dstrgb128r0++, rgb4567);
      _mm_store_si128(dstrgb128r0++, rgb89ab);
      _mm_store_si128(dstrgb128r0++, rgbcdef);

      // row 1
      r00 = _mm_srai_epi16(_mm_add_epi16(y00r1, rv00), 6);
      r01 = _mm_srai_epi16(_mm_add_epi16(y01r1, rv01), 6);
      g00 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y00r1, gu00), gv00), 6);
      g01 = _mm_srai_epi16(_mm_sub_epi16(_mm_sub_epi16(y01r1, gu01), gv01), 6);
      b00 = _mm_srai_epi16(_mm_add_epi16(y00r1, bu00), 6);
      b01 = _mm_srai_epi16(_mm_add_epi16(y01r1, bu01), 6);

      r00 = _mm_packus_epi16(r00, r01);
      g00 = _mm_packus_epi16(g00, g01);
      b00 = _mm_packus_epi16(b00, b01);

      r01     = _mm_unpacklo_epi8(r00, zero);
      gbgb    = _mm_unpacklo_epi8(b00, g00);
      rgb0123 = _mm_unpacklo_epi16(gbgb, r01);
      rgb4567 = _mm_unpackhi_epi16(gbgb, r01);

      r01     = _mm_unpackhi_epi8(r00, zero);
      gbgb    = _mm_unpackhi_epi8(b00, g00);
      rgb89ab = _mm_unpacklo_epi16(gbgb, r01);
      rgbcdef = _mm_unpackhi_epi16(gbgb, r01);

      _mm_store_si128(dstrgb128r1++, rgb0123);
      _mm_store_si128(dstrgb128r1++, rgb4567);
      _mm_store_si128(dstrgb128r1++, rgb89ab);
      _mm_store_si128(dstrgb128r1++, rgbcdef);
    }
  }
}
#endif

QLayout *videoHandlerYUV::createVideoHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  QVBoxLayout *newVBoxLayout = nullptr;
  if (!isSizeFixed)
  {
    // Our parent (videoHandler) also has controls to add. Create a new vBoxLayout and append the
    // parent controls and our controls into that layout, separated by a line. Return that layout
    newVBoxLayout = new QVBoxLayout;
    newVBoxLayout->addLayout(FrameHandler::createFrameHandlerControls(isSizeFixed));

    QFrame *line = new QFrame;
    line->setObjectName(QStringLiteral("line"));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    newVBoxLayout->addWidget(line);
  }

  // Create the UI and setup all the controls
  ui.setupUi();

  // Add the preset YUV formats. If the current format is in the list, add it and select it.
  for (auto format : presetList)
    ui.yuvFormatComboBox->addItem(QString::fromStdString(format.getName()));

  int idx = presetList.indexOf(srcPixelFormat);
  if (idx == -1 && srcPixelFormat.isValid())
  {
    // The currently set pixel format is not in the presets list. Add and select it.
    ui.yuvFormatComboBox->addItem(QString::fromStdString(srcPixelFormat.getName()));
    presetList.append(srcPixelFormat);
    idx = presetList.indexOf(srcPixelFormat);
  }
  ui.yuvFormatComboBox->setCurrentIndex(idx);
  // Add the custom... entry that allows the user to add custom formats
  ui.yuvFormatComboBox->addItem("Custom...");
  ui.yuvFormatComboBox->setEnabled(!isSizeFixed);

  // Set all the values of the properties widget to the values of this class
  ui.colorComponentsComboBox->addItems(
      functions::toQStringList(conversion::ComponentDisplayModeMapper.getNames()));
  ui.colorComponentsComboBox->setCurrentIndex(
      int(conversion::ComponentDisplayModeMapper.indexOf(this->componentDisplayMode)));
  ui.chromaInterpolationComboBox->addItems(
      functions::toQStringList(ChromaInterpolationMapper.getNames()));
  ui.chromaInterpolationComboBox->setCurrentIndex(
      int(ChromaInterpolationMapper.indexOf(this->chromaInterpolation)));
  ui.chromaInterpolationComboBox->setEnabled(srcPixelFormat.isChromaSubsampled());
  ui.colorConversionComboBox->addItems(functions::toQStringList(ColorConversionMapper.getNames()));
  ui.colorConversionComboBox->setCurrentIndex(
      int(ColorConversionMapper.indexOf(this->colorConversion)));
  ui.lumaScaleSpinBox->setValue(this->mathParametersPerComponent[conversion::LUMA].scale);
  ui.lumaOffsetSpinBox->setMaximum(1000);
  ui.lumaOffsetSpinBox->setValue(this->mathParametersPerComponent[conversion::LUMA].offset);
  ui.lumaInvertCheckBox->setChecked(this->mathParametersPerComponent[conversion::LUMA].invert);
  ui.chromaScaleSpinBox->setValue(this->mathParametersPerComponent[conversion::CHROMA].scale);
  ui.chromaOffsetSpinBox->setMaximum(1000);
  ui.chromaOffsetSpinBox->setValue(this->mathParametersPerComponent[conversion::CHROMA].offset);
  ui.chromaInvertCheckBox->setChecked(this->mathParametersPerComponent[conversion::CHROMA].invert);

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

  if (!isSizeFixed && newVBoxLayout)
    newVBoxLayout->addLayout(ui.topVBoxLayout);

  return (isSizeFixed) ? ui.topVBoxLayout : newVBoxLayout;
}

void videoHandlerYUV::slotYUVFormatControlChanged(int idx)
{
  PixelFormatYUV newFormat;

  if (idx == presetList.count())
  {
    // The user selected the "custom format..." option
    videoHandlerYUVCustomFormatDialog dialog(srcPixelFormat);
    if (dialog.exec() == QDialog::Accepted && dialog.getSelectedYUVFormat().isValid())
    {
      // Set the custom format
      // Check if the user specified a new format
      newFormat = dialog.getSelectedYUVFormat();

      // Check if the custom format it in the presets list. If not, add it
      int idx = presetList.indexOf(newFormat);
      if (idx == -1 && newFormat.isValid())
      {
        // Valid pixel format with is not in the list. Add it...
        presetList.append(newFormat);
        int                  nrItems = ui.yuvFormatComboBox->count();
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->insertItem(nrItems - 1, QString::fromStdString(newFormat.getName()));
        // Select the added format
        idx = presetList.indexOf(newFormat);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
      else
      {
        // The format is already in the list. Select it without invoking another signal.
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
    }
    else
    {
      // The user pressed cancel. Go back to the old format
      int idx = presetList.indexOf(srcPixelFormat);
      Q_ASSERT(idx != -1); // The previously selected format should always be in the list
      const QSignalBlocker blocker(ui.yuvFormatComboBox);
      ui.yuvFormatComboBox->setCurrentIndex(idx);
    }
  }
  else
    // One of the preset formats was selected
    newFormat = presetList.at(idx);

  // Set the new format (if new) and emit a signal that a new format was selected.
  if (srcPixelFormat != newFormat && newFormat.isValid())
    setSrcPixelFormat(newFormat);
}

void videoHandlerYUV::setSrcPixelFormat(PixelFormatYUV format, bool emitSignal)
{
  // Store the number bytes per frame of the old pixel format
  auto oldFormatBytesPerFrame = srcPixelFormat.bytesPerFrame(frameSize);

  // Set the new pixel format. Lock the mutex, so that no background process is running wile the
  // format changes.
  srcPixelFormat = format;

  // Update the math parameter offset (the default offset depends on the bit depth and the range)
  int        shift                                            = format.getBitsPerSample() - 8;
  const bool fullRange                                        = isFullRange(this->colorConversion);
  this->mathParametersPerComponent[conversion::LUMA].offset   = (fullRange ? 128 : 125) << shift;
  this->mathParametersPerComponent[conversion::CHROMA].offset = 128 << shift;

  if (ui.created())
  {
    // Every time the pixel format changed, see if the interpolation combo box is enabled/disabled
    QSignalBlocker blocker1(ui.chromaInterpolationComboBox);
    QSignalBlocker blocker2(ui.lumaOffsetSpinBox);
    QSignalBlocker blocker3(ui.chromaOffsetSpinBox);
    ui.chromaInterpolationComboBox->setEnabled(format.isChromaSubsampled());
    ui.lumaOffsetSpinBox->setValue(this->mathParametersPerComponent[conversion::LUMA].offset);
    ui.chromaOffsetSpinBox->setValue(this->mathParametersPerComponent[conversion::CHROMA].offset);
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
    this->chromaInterpolation =
        *ChromaInterpolationMapper.at(ui.chromaInterpolationComboBox->currentIndex());
    this->componentDisplayMode =
        *conversion::ComponentDisplayModeMapper.at(ui.colorComponentsComboBox->currentIndex());
    this->colorConversion = *ColorConversionMapper.at(ui.colorConversionComboBox->currentIndex());

    this->mathParametersPerComponent[conversion::LUMA].scale   = ui.lumaScaleSpinBox->value();
    this->mathParametersPerComponent[conversion::LUMA].offset  = ui.lumaOffsetSpinBox->value();
    this->mathParametersPerComponent[conversion::LUMA].invert  = ui.lumaInvertCheckBox->isChecked();
    this->mathParametersPerComponent[conversion::CHROMA].scale = ui.chromaScaleSpinBox->value();
    this->mathParametersPerComponent[conversion::CHROMA].offset = ui.chromaOffsetSpinBox->value();
    this->mathParametersPerComponent[conversion::CHROMA].invert =
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

    auto thisValue  = getPixelValue(pixelPos);
    auto otherValue = yuvItem2->getPixelValue(pixelPos);

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

    const auto value = getPixelValue(pixelPos);

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
void videoHandlerYUV::drawPixelValues(QPainter *    painter,
                                      const int     frameIdx,
                                      const QRect & videoRect,
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

  const auto math = this->mathParametersPerComponent;

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
          drawWhite = (math.at(conversion::LUMA).invert) ? (Y > whiteLimit) : (Y < whiteLimit);
      }
      else if (showPixelValuesAsDiff)
      {
        const auto value = getPixelValue(QPoint(x, y));
        Y                = value.Y - differenceZeroValue;
        U                = value.U - differenceZeroValue;
        V                = value.V - differenceZeroValue;

        drawWhite = (math.at(conversion::LUMA).invert) ? (Y > 0) : (Y < 0);
      }
      else
      {
        const auto value = getPixelValue(QPoint(x, y));
        Y                = int(value.Y);
        U                = int(value.U);
        V                = int(value.V);
        drawWhite        = (math.at(conversion::LUMA).invert) ? (Y > whiteLimit) : (Y < whiteLimit);
      }

      const auto    formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
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

void videoHandlerYUV::setFormatFromSizeAndName(Size             frameSize,
                                               int              bitDepth,
                                               DataLayout       dataLayout,
                                               int64_t          fileSize,
                                               const QFileInfo &fileInfo)
{
  auto fmt = guessFormatFromSizeAndName(frameSize, bitDepth, dataLayout, fileSize, fileInfo);

  if (!fmt.isValid())
  {
    // Guessing failed. Set YUV 4:2:0 8 bit so that we can show something.
    // This will probably be wrong but we are out of options
    fmt = PixelFormatYUV(Subsampling::YUV_420, 8, PlaneOrder::YUV);
  }

  setSrcPixelFormat(fmt, false);
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
    for (const auto &subsampling : SubsamplingMapper.getEnums())
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
                      this->getConversionSettings());
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
                      this->getConversionSettings());
    QMutexLocker setLock(&currentImageSetMutex);
    currentImage      = newImage;
    currentImageIndex = frameIndex;
  }
}

void videoHandlerYUV::loadFrameForCaching(int frameIndex, QImage &frameToCache)
{
  DEBUG_YUV("videoHandlerYUV::loadFrameForCaching " << frameIndex);

  // Get the YUV format and the size here, so that the caching process does not crash if this
  // changes.
  const auto yuvFormat    = this->srcPixelFormat;
  const auto curFrameSize = this->frameSize;

  requestDataMutex.lock();
  emit       signalRequestRawData(frameIndex, true);
  QByteArray tmpBufferRawYUVDataCaching = rawData;
  requestDataMutex.unlock();

  if (frameIndex != rawData_frameIndex)
  {
    // Loading failed
    DEBUG_YUV("videoHandlerYUV::loadFrameForCaching Loading failed");
    return;
  }

  // Convert YUV to image. This can then be cached.
  convertYUVToImage(tmpBufferRawYUVDataCaching,
                    frameToCache,
                    yuvFormat,
                    curFrameSize,
                    this->getConversionSettings());
}

// Load the raw YUV data for the given frame index into currentFrameRawData.
bool videoHandlerYUV::loadRawYUVData(int frameIndex)
{
  if (currentFrameRawData_frameIndex == frameIndex && cacheValid)
    // Buffer already up to date
    return true;

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

  currentFrameRawData            = rawData;
  currentFrameRawData_frameIndex = frameIndex;
  requestDataMutex.unlock();

  DEBUG_YUV("videoHandlerYUV::loadRawYUVData " << frameIndex << " Done");
  return true;
}

conversion::ConversionSettings videoHandlerYUV::getConversionSettings()
{
  conversion::ConversionSettings settings;
  settings.frameSize                  = this->frameSize;
  settings.pixelFormat                = this->srcPixelFormat;
  settings.chromaInterpolation        = this->chromaInterpolation;
  settings.componentDisplayMode       = this->componentDisplayMode;
  settings.colorConversion            = this->colorConversion;
  settings.mathParametersPerComponent = this->mathParametersPerComponent;
  return settings;
}

yuva_t videoHandlerYUV::getPixelValue(const QPoint &pixelPos) const
{
  const auto format = this->srcPixelFormat;
  const auto w      = this->frameSize.width;
  const auto h      = this->frameSize.height;

  yuva_t value;

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
    auto srcY = reinterpret_cast<const unsigned char * restrict>(currentFrameRawData.data());
    const unsigned int offsetCoordinateY = w * pixelPos.y() + pixelPos.x();
    value.Y                              = conversion::getValueFromSource(
        srcY, offsetCoordinateY, format.getBitsPerSample(), format.isBigEndian());

    if (format.getSubsampling() != Subsampling::YUV_400)
    {
      // Now Chroma
      const bool uFirst =
          (format.getPlaneOrder() == PlaneOrder::YUV || format.getPlaneOrder() == PlaneOrder::YUVA);
      const bool hasAlpha = (format.getPlaneOrder() == PlaneOrder::YUVA ||
                             format.getPlaneOrder() == PlaneOrder::YVUA);
      if (format.getChromaPacking() != ChromaPacking::Planar)
      {
        // U, V (and alpha) are interleaved
        const unsigned char *restrict srcUVA = srcY + nrBytesLumaPlane;
        const unsigned int            mult   = hasAlpha ? 3 : 2;
        const unsigned int            offsetCoordinateUV =
            ((w / format.getSubsamplingHor() * (pixelPos.y() / format.getSubsamplingVer())) +
             pixelPos.x() / format.getSubsamplingHor()) *
            mult;

        value.U = conversion::getValueFromSource(srcUVA,
                                                 offsetCoordinateUV + (uFirst ? 0 : 1),
                                                 format.getBitsPerSample(),
                                                 format.isBigEndian());
        value.V = conversion::getValueFromSource(srcUVA,
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

        value.U = conversion::getValueFromSource(
            srcU, offsetCoordinateUV, format.getBitsPerSample(), format.isBigEndian());
        value.V = conversion::getValueFromSource(
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
        auto                          offsetInInput = pixelPos.y() * (pixelPos.x() / 2) * 5;
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

        value.Y = conversion::getValueFromSource(src,
                                                 (pixelPos.x() % 2 == 0) ? oY : oY + 2,
                                                 format.getBitsPerSample(),
                                                 format.isBigEndian());
        value.U = conversion::getValueFromSource(
            src, oU, format.getBitsPerSample(), format.isBigEndian());
        value.V = conversion::getValueFromSource(
            src, oV, format.getBitsPerSample(), format.isBigEndian());
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
      const int                     offsetSrc = (w * pixelPos.y() + pixelPos.x()) * offsetNext;
      const unsigned char *restrict src = (unsigned char *)currentFrameRawData.data() + offsetSrc;

      value.Y =
          conversion::getValueFromSource(src, oY, format.getBitsPerSample(), format.isBigEndian());
      value.U =
          conversion::getValueFromSource(src, oU, format.getBitsPerSample(), format.isBigEndian());
      value.V =
          conversion::getValueFromSource(src, oV, format.getBitsPerSample(), format.isBigEndian());
    }
  }

  return value;
}

bool videoHandlerYUV::markDifferencesYUVPlanarToRGB(const QByteArray &    sourceBuffer,
                                                    unsigned char *       targetBuffer,
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
      int valU    = conversion::getValueFromSource(srcU, uvIndex, bps, bigEndian);
      int valV    = conversion::getValueFromSource(srcV, uvIndex, bps, bigEndian);

      for (int yInBlock = 0; yInBlock < sampleBlocksY; yInBlock++)
      {
        for (int xInBlock = 0; xInBlock < sampleBlocksX; xInBlock++)
        {
          // Get the Y difference value
          int valY = conversion::getValueFromSource(
              srcY, (y + yInBlock) * w + x + xInBlock, bps, bigEndian);

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

QImage videoHandlerYUV::calculateDifference(FrameHandler *   item2,
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
  // If the bit depth if the two items is different, we will scale the item with the lower bit depth
  // up.
  const unsigned bps_in[2] = {srcPixelFormat.getBitsPerSample(),
                              yuvItem2->srcPixelFormat.getBitsPerSample()};
  const auto     bps_out   = std::max(bps_in[0], bps_in[1]);
  // Which of the two input values has to be scaled up? Only one of these (or neither) can be set.
  const bool bitDepthScaling[2] = {bps_in[0] != bps_out, bps_in[1] != bps_out};
  // Scale the input up by this many bits
  const auto depthScale = bps_out - (bitDepthScaling[0] ? bps_in[0] : bps_in[1]);
  // Add a warning if the bit depths of the two inputs don't agree
  if (bitDepthScaling[0] || bitDepthScaling[1])
    differenceInfoList.append(
        InfoItem("Warning",
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
        InfoItem("Warning",
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
      auto val1 = conversion::getValueFromSource(srcY1, x, bps_in[0], bigEndian[0]);
      auto val2 = conversion::getValueFromSource(srcY2, x, bps_in[1], bigEndian[1]);

      // Scale (if necessary)
      if (bitDepthScaling[0])
        val1 = val1 << depthScale;
      else if (bitDepthScaling[1])
        val2 = val2 << depthScale;

      // Calculate the difference, add MSE, (amplify) and clip the difference value
      auto diff = val1 - val2;
      mseAdd[0] += diff * diff;
      if (amplification)
        diff *= amplificationFactor;
      diff = functions::clip(diff + diffZero, 0, maxVal);

      conversion::setValueInBuffer(dstY, diff, 0, bps_out, true);
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
      auto valU1 = conversion::getValueFromSource(srcU1, x, bps_in[0], bigEndian[0]);
      auto valU2 = conversion::getValueFromSource(srcU2, x, bps_in[1], bigEndian[1]);
      auto valV1 = conversion::getValueFromSource(srcV1, x, bps_in[0], bigEndian[0]);
      auto valV2 = conversion::getValueFromSource(srcV2, x, bps_in[1], bigEndian[1]);

      // Scale (if necessary)
      if (bitDepthScaling[0])
      {
        valU1 = valU1 << depthScale;
        valV1 = valV1 << depthScale;
      }
      else if (bitDepthScaling[1])
      {
        valU2 = valU2 << depthScale;
        valV2 = valV2 << depthScale;
      }

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

      conversion::setValueInBuffer(dstU, diffU, 0, bps_out, true);
      conversion::setValueInBuffer(dstV, diffV, 0, bps_out, true);
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
    conversion::ConversionSettings conversionSettings;
    conversionSettings.mathParametersPerComponent[conversion::LUMA] = MathParameters(1, 125, false);
    conversionSettings.mathParametersPerComponent[conversion::CHROMA] =
        MathParameters(1, 128, false);
    conversionSettings.frameSize   = Size(w_out, h_out);
    conversionSettings.pixelFormat = tmpDiffYUVFormat;

    auto src = reinterpret_cast<conversion::ConstDataPointer>(diffYUV.data());
    conversion::convertYUVToARGB(src, conversionSettings, outputImage.bits());
  }

  differenceInfoList.append(
      InfoItem("Difference Type",
               QString("YUV %1").arg(QString::fromStdString(
                   SubsamplingMapper.getText(srcPixelFormat.getSubsampling())))));

  {
    auto       nrPixelsLuma = w_out * h_out;
    const auto maxSquared   = ((1 << bps_out) - 1) * ((1 << bps_out) - 1);
    auto       mse          = double(mseAdd[0]) / nrPixelsLuma;
    auto       psnr         = 10 * std::log10(maxSquared / mse);
    differenceInfoList.append(
        InfoItem("MSE/PSNR Y", QString("%1 (%2dB)").arg(mse, 0, 'f', 1).arg(psnr, 0, 'f', 2)));

    if (srcPixelFormat.getSubsampling() != Subsampling::YUV_400)
    {
      auto nrPixelsChroma = w_out / subH * h_out / subV;

      auto mseU  = double(mseAdd[1]) / nrPixelsChroma;
      auto psnrU = 10 * std::log10(maxSquared / mseU);
      differenceInfoList.append(
          InfoItem("MSE/PSNR U", QString("%1 (%2dB)").arg(mseU, 0, 'f', 1).arg(psnrU, 0, 'f', 2)));

      auto mseV  = double(mseAdd[2]) / nrPixelsChroma;
      auto psnrV = 10 * std::log10(maxSquared / mseV);
      differenceInfoList.append(
          InfoItem("MSE/PSNR V", QString("%1 (%2dB)").arg(mseV, 0, 'f', 1).arg(psnrV, 0, 'f', 2)));

      auto mseAvg = double(mseAdd[0] + mseAdd[1] + mseAdd[2]) / (nrPixelsLuma + 2 * nrPixelsChroma);
      auto psnrAvg = 10 * std::log10(maxSquared / mseAvg);
      differenceInfoList.append(InfoItem(
          "MSE/PSNR Avg", QString("%1 (%2dB)").arg(mseAvg, 0, 'f', 1).arg(psnrAvg, 0, 'f', 2)));
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

  if (newFormat != srcPixelFormat)
  {
    if (ui.created())
    {
      // Check if the custom format is in the presets list. If not, add it.
      int idx = presetList.indexOf(newFormat);
      if (idx == -1)
      {
        // Valid pixel format with is not in the list. Add it...
        presetList.append(newFormat);
        int                  nrItems = ui.yuvFormatComboBox->count();
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->insertItem(nrItems - 1, QString::fromStdString(newFormat.getName()));
        // Select the added format
        idx = presetList.indexOf(newFormat);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
      else
      {
        // Just select the format in the combo box
        const QSignalBlocker blocker(ui.yuvFormatComboBox);
        ui.yuvFormatComboBox->setCurrentIndex(idx);
      }
    }

    setSrcPixelFormat(newFormat, emitSignal);
  }
}

void videoHandlerYUV::setYUVColorConversion(ColorConversion conversion)
{
  if (conversion != this->colorConversion)
  {
    this->colorConversion = conversion;

    if (ui.created())
      ui.colorConversionComboBox->setCurrentIndex(
          int(ColorConversionMapper.indexOf(this->colorConversion)));
  }
}

void videoHandlerYUV::savePlaylist(YUViewDomElement &element) const
{
  FrameHandler::savePlaylist(element);
  element.appendProperiteChild("pixelFormat", this->getRawPixelFormatYUVName());

  auto ml = this->mathParametersPerComponent.at(conversion::LUMA);
  element.appendProperiteChild("math.luma.scale", QString::number(ml.scale));
  element.appendProperiteChild("math.luma.offset", QString::number(ml.offset));
  element.appendProperiteChild("math.luma.invert", functions::boolToString(ml.invert));

  auto mc = this->mathParametersPerComponent.at(conversion::CHROMA);
  element.appendProperiteChild("math.chroma.scale", QString::number(mc.scale));
  element.appendProperiteChild("math.chroma.offset", QString::number(mc.offset));
  element.appendProperiteChild("math.chroma.invert", functions::boolToString(mc.invert));
}

void videoHandlerYUV::loadPlaylist(const YUViewDomElement &element)
{
  FrameHandler::loadPlaylist(element);

  auto sourcePixelFormat = element.findChildValue("pixelFormat");
  this->setPixelFormatYUVByName(sourcePixelFormat);

  auto lumaScale = element.findChildValue("math.luma.scale");
  if (!lumaScale.isEmpty())
    this->mathParametersPerComponent[conversion::LUMA].scale = lumaScale.toInt();
  auto lumaOffset = element.findChildValue("math.luma.offset");
  if (!lumaOffset.isEmpty())
    this->mathParametersPerComponent[conversion::LUMA].offset = lumaOffset.toInt();
  this->mathParametersPerComponent[conversion::LUMA].invert =
      (element.findChildValue("math.luma.invert") == "True");

  auto chromaScale = element.findChildValue("math.chroma.scale");
  if (!chromaScale.isEmpty())
    this->mathParametersPerComponent[conversion::CHROMA].scale = chromaScale.toInt();
  auto chromaOffset = element.findChildValue("math.chroma.offset");
  if (!chromaOffset.isEmpty())
    this->mathParametersPerComponent[conversion::CHROMA].offset = chromaOffset.toInt();
  this->mathParametersPerComponent[conversion::CHROMA].invert =
      (element.findChildValue("math.chroma.invert") == "True");
}

} // namespace video::yuv
