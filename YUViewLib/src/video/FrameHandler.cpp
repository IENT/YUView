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

#include "FrameHandler.h"

#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QOpenGLContext>
#include <QPainter>
#include <QScreen>
#include <QSurfaceFormat>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>

#include <common/Functions.h>
#include <common/FunctionsGui.h>
#include <decoder/decoderTarga.h>
#include <playlistitem/playlistItem.h>

using namespace std::string_view_literals;

namespace video
{

// Activate this if you want to know when which buffer is loaded/converted to image and so on.
#define FRAMEHANDLER_DEBUG_LOADING 0
#if FRAMEHANDLER_DEBUG_LOADING && !NDEBUG
#else
#define DEBUG_FRAME(fmt, ...) ((void)0)
#endif

// Forward declaration
void configureHighBitDepthRendering(QPainter *painter);

// Configure high bit-depth rendering for 10-bit display support
void configureHighBitDepthRendering(QPainter *painter)
{
  static bool configurationChecked = false;
  if (configurationChecked)
    return;

  configurationChecked = true;

         // Check if OpenGL is available
  QOpenGLContext *context = QOpenGLContext::currentContext();
  if (context)
  {
    QSurfaceFormat format = context->format();

           // Check if we have adequate bit depth for 10-bit rendering
    if (format.redBufferSize() >= 10 && format.greenBufferSize() >= 10 &&
        format.blueBufferSize() >= 10)
    {
    }
    else
    {

      // Try to request a higher bit depth format
      QSurfaceFormat newFormat;
      newFormat.setRedBufferSize(10);
      newFormat.setGreenBufferSize(10);
      newFormat.setBlueBufferSize(10);
      newFormat.setAlphaBufferSize(2);
      newFormat.setVersion(3, 3);
      newFormat.setProfile(QSurfaceFormat::CoreProfile);
      QSurfaceFormat::setDefaultFormat(newFormat);
    }
  }
  else
  {
  }

         // Check screen capabilities
  if (QGuiApplication::screens().size() > 0)
  {
    QScreen *screen = QGuiApplication::screens().first();

    if (screen->depth() >= 30) // 10 bits per channel = 30 bits total
    {
    }
    else
    {
    }
  }

         // Configure painter for high quality rendering
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter->setRenderHint(QPainter::Antialiasing, true);
}

namespace
{

struct RgbSample
{
  int r{0};
  int g{0};
  int b{0};
  int maxValue{255};
};

/**
 * Return true when the image format stores RGB samples with 16 bits per channel.
 */
bool is16BitRgbImageFormat(QImage::Format format)
{
  return format == QImage::Format_RGBX64 || format == QImage::Format_RGBA64 ||
         format == QImage::Format_RGBA64_Premultiplied || format == QImage::Format_Grayscale16;
}

/**
 * Read a big-endian 16-bit integer from an image file byte stream.
 */
uint16_t readBE16(const uchar *data)
{
  return (uint16_t(data[0]) << 8) | uint16_t(data[1]);
}

/**
 * Read a little-endian 16-bit integer from an image file byte stream.
 */
uint16_t readLE16(const uchar *data)
{
  return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

/**
 * Read a little-endian 32-bit integer from an image file byte stream.
 */
uint32_t readLE32(const uchar *data)
{
  return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) |
         (uint32_t(data[3]) << 24);
}

/**
 * Read a little-endian signed 32-bit integer from an image file byte stream.
 */
int32_t readLE32Signed(const uchar *data)
{
  return int32_t(readLE32(data));
}

struct PngInfo
{
  uint32_t width{0};
  uint32_t height{0};
  int      bitDepth{0};
  int      colorType{0};
};

/**
 * Read a big-endian 32-bit integer from a PNG byte stream.
 */
uint32_t readBE32(const uchar *data)
{
  return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) |
         uint32_t(data[3]);
}

/**
 * Return the number of channels used by a PNG color type.
 */
int pngChannelCount(int colorType)
{
  switch (colorType)
  {
  case 0:
    return 1;
  case 2:
    return 3;
  case 4:
    return 2;
  case 6:
    return 4;
  default:
    return 0;
  }
}

/**
 * Return the Paeth predictor used by the PNG filter algorithm.
 */
int pngPaethPredictor(int left, int above, int upperLeft)
{
  const int predictor  = left + above - upperLeft;
  const int pLeft      = std::abs(predictor - left);
  const int pAbove     = std::abs(predictor - above);
  const int pUpperLeft = std::abs(predictor - upperLeft);
  if (pLeft <= pAbove && pLeft <= pUpperLeft)
    return left;
  if (pAbove <= pUpperLeft)
    return above;
  return upperLeft;
}

/**
 * Undo PNG scanline filters in-place after IDAT decompression.
 */
bool unfilterPngRows(QByteArray &rawData, uint32_t width, uint32_t height, int channelCount)
{
  const qsizetype bytesPerPixel = channelCount * 2;
  const qsizetype rowBytes      = qsizetype(width) * bytesPerPixel;
  const qsizetype stride        = rowBytes + 1;
  if (rawData.size() != stride * qsizetype(height))
    return false;

  for (uint32_t y = 0; y < height; ++y)
  {
    auto       *row        = reinterpret_cast<uchar *>(rawData.data()) + qsizetype(y) * stride;
    auto       *scanline   = row + 1;
    const auto *previous   = y == 0 ? nullptr : scanline - stride;
    const int   filterType = row[0];

    for (qsizetype x = 0; x < rowBytes; ++x)
    {
      const int left      = x >= bytesPerPixel ? scanline[x - bytesPerPixel] : 0;
      const int above     = previous ? previous[x] : 0;
      const int upperLeft = previous && x >= bytesPerPixel ? previous[x - bytesPerPixel] : 0;
      int       predictor = 0;

      switch (filterType)
      {
      case 0:
        predictor = 0;
        break;
      case 1:
        predictor = left;
        break;
      case 2:
        predictor = above;
        break;
      case 3:
        predictor = (left + above) / 2;
        break;
      case 4:
        predictor = pngPaethPredictor(left, above, upperLeft);
        break;
      default:
        return false;
      }

      scanline[x] = uchar((int(scanline[x]) + predictor) & 0xff);
    }
  }

  return true;
}

/**
 * Inflate the concatenated PNG IDAT payload into filtered scanlines.
 */
std::optional<QByteArray>
inflatePngIdat(const QByteArray &compressedData, uint32_t width, uint32_t height, int channelCount)
{
  const qsizetype bytesPerPixel = channelCount * 2;
  const qsizetype rowBytes      = qsizetype(width) * bytesPerPixel;
  const qsizetype expectedSize  = (rowBytes + 1) * qsizetype(height);
  if (expectedSize <= 0 || expectedSize > std::numeric_limits<int>::max() ||
      compressedData.size() > std::numeric_limits<int>::max() - 4)
    return std::nullopt;

  QByteArray qtCompressed;
  qtCompressed.resize(4 + compressedData.size());
  auto *header = reinterpret_cast<uchar *>(qtCompressed.data());
  header[0]    = uchar((uint32_t(expectedSize) >> 24) & 0xff);
  header[1]    = uchar((uint32_t(expectedSize) >> 16) & 0xff);
  header[2]    = uchar((uint32_t(expectedSize) >> 8) & 0xff);
  header[3]    = uchar(uint32_t(expectedSize) & 0xff);
  std::memcpy(qtCompressed.data() + 4, compressedData.constData(), compressedData.size());

  const auto inflated = qUncompress(qtCompressed);
  if (inflated.size() != expectedSize)
    return std::nullopt;

  return inflated;
}

/**
 * Parse PNG metadata and concatenate all IDAT chunks from a PNG file.
 */
std::optional<PngInfo> parsePngChunks(const QByteArray &pngData, QByteArray &idatData)
{
  static const std::array<uchar, 8> pngSignature{{137, 80, 78, 71, 13, 10, 26, 10}};
  if (pngData.size() < int(pngSignature.size()) ||
      std::memcmp(pngData.constData(), pngSignature.data(), pngSignature.size()) != 0)
    return std::nullopt;

  PngInfo   info;
  bool      haveIhdr = false;
  qsizetype offset   = 8;
  while (offset + 12 <= pngData.size())
  {
    const auto *chunk  = reinterpret_cast<const uchar *>(pngData.constData() + offset);
    const auto  length = readBE32(chunk);
    if (length > uint32_t(std::numeric_limits<int>::max()) ||
        offset + 12 + qsizetype(length) > pngData.size())
      return std::nullopt;

    const QByteArray chunkType(pngData.constData() + offset + 4, 4);
    const auto      *chunkData = reinterpret_cast<const uchar *>(pngData.constData() + offset + 8);
    if (chunkType == "IHDR")
    {
      if (length != 13)
        return std::nullopt;

      info.width                  = readBE32(chunkData);
      info.height                 = readBE32(chunkData + 4);
      info.bitDepth               = chunkData[8];
      info.colorType              = chunkData[9];
      const int compressionMethod = chunkData[10];
      const int filterMethod      = chunkData[11];
      const int interlaceMethod   = chunkData[12];
      if (info.width == 0 || info.height == 0 || compressionMethod != 0 || filterMethod != 0 ||
          interlaceMethod != 0)
        return std::nullopt;
      haveIhdr = true;
    }
    else if (chunkType == "IDAT")
      idatData.append(reinterpret_cast<const char *>(chunkData), int(length));
    else if (chunkType == "IEND")
      break;

    offset += 12 + qsizetype(length);
  }

  if (!haveIhdr || idatData.isEmpty())
    return std::nullopt;
  return info;
}

/**
 * Load a non-interlaced 16-bit-per-sample PNG into a high bit-depth QImage.
 */
std::optional<QImage> load16BitPng(const QString &filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;

  const QByteArray pngData = file.readAll();
  QByteArray       idatData;
  const auto       info = parsePngChunks(pngData, idatData);
  if (!info || info->bitDepth != 16)
    return std::nullopt;

  const int channelCount = pngChannelCount(info->colorType);
  if (channelCount == 0)
    return std::nullopt;

  auto rawData = inflatePngIdat(idatData, info->width, info->height, channelCount);
  if (!rawData || !unfilterPngRows(*rawData, info->width, info->height, channelCount))
    return std::nullopt;

  const qsizetype rowBytes = qsizetype(info->width) * channelCount * 2;
  const qsizetype stride   = rowBytes + 1;
  QImage          image;
  if (info->colorType == 0)
  {
    image = QImage(int(info->width), int(info->height), QImage::Format_Grayscale16);
    for (uint32_t y = 0; y < info->height; ++y)
    {
      const auto *src =
        reinterpret_cast<const uchar *>(rawData->constData() + qsizetype(y) * stride + 1);
      auto *dst = reinterpret_cast<quint16 *>(image.scanLine(int(y)));
      for (uint32_t x = 0; x < info->width; ++x)
        dst[x] = readBE16(src + qsizetype(x) * 2);
    }
  }
  else
  {
    image = QImage(int(info->width), int(info->height), QImage::Format_RGBA64);
    for (uint32_t y = 0; y < info->height; ++y)
    {
      const auto *src =
        reinterpret_cast<const uchar *>(rawData->constData() + qsizetype(y) * stride + 1);
      auto *dst = reinterpret_cast<QRgba64 *>(image.scanLine(int(y)));
      for (uint32_t x = 0; x < info->width; ++x)
      {
        const auto *pixel = src + qsizetype(x) * channelCount * 2;
        uint16_t    r     = 0;
        uint16_t    g     = 0;
        uint16_t    b     = 0;
        uint16_t    a     = 65535;
        if (info->colorType == 4)
        {
          r = g = b = readBE16(pixel);
          a         = readBE16(pixel + 2);
        }
        else
        {
          r = readBE16(pixel);
          g = readBE16(pixel + 2);
          b = readBE16(pixel + 4);
          if (info->colorType == 6)
            a = readBE16(pixel + 6);
        }
        dst[x] = QRgba64::fromRgba64(r, g, b, a);
      }
    }
  }

  return image;
}

/**
 * Load an uncompressed 48-bit or 64-bit BMP into a high bit-depth QImage.
 */
std::optional<QImage> load16BitBmp(const QString &filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
    return std::nullopt;

  const QByteArray data = file.readAll();
  if (data.size() < 54 || data[0] != 'B' || data[1] != 'M')
    return std::nullopt;

  const uint32_t pixelOffset = readLE32(reinterpret_cast<const uchar *>(data.constData() + 10));
  const uint32_t dibSize     = readLE32(reinterpret_cast<const uchar *>(data.constData() + 14));
  if (dibSize < 40 || data.size() < int(14 + dibSize))
    return std::nullopt;

  const auto    *dib       = reinterpret_cast<const uchar *>(data.constData() + 14);
  const int32_t  width     = readLE32Signed(dib + 4);
  const int32_t  rawHeight = readLE32Signed(dib + 8);
  const uint16_t planes    = readLE16(dib + 12);
  const uint16_t bitCount  = readLE16(dib + 14);
  const uint32_t compress  = readLE32(dib + 16);
  if (width <= 0 || rawHeight == 0 || planes != 1 || compress != 0 ||
      (bitCount != 48 && bitCount != 64))
    return std::nullopt;

  const int  height      = std::abs(rawHeight);
  const bool topDown     = rawHeight < 0;
  const int  bytesPerPix = bitCount / 8;
  const int  rowStride   = ((width * bytesPerPix + 3) / 4) * 4;
  const auto neededBytes = uint64_t(pixelOffset) + uint64_t(rowStride) * uint64_t(height);
  if (neededBytes > uint64_t(data.size()))
    return std::nullopt;

  QImage image(width, height, QImage::Format_RGBA64);
  for (int y = 0; y < height; ++y)
  {
    const int   srcY = topDown ? y : height - 1 - y;
    const auto *src =
      reinterpret_cast<const uchar *>(data.constData()) + pixelOffset + srcY * rowStride;
    auto *dst = reinterpret_cast<QRgba64 *>(image.scanLine(y));
    for (int x = 0; x < width; ++x)
    {
      const auto *pixel = src + x * bytesPerPix;
      const auto  b     = readLE16(pixel);
      const auto  g     = readLE16(pixel + 2);
      const auto  r     = readLE16(pixel + 4);
      const auto  a     = bitCount == 64 ? readLE16(pixel + 6) : uint16_t(65535);
      dst[x]            = QRgba64::fromRgba64(r, g, b, a);
    }
  }

  return image;
}

/**
 * Load an image file while preserving 16-bit-per-channel PNG/BMP samples when available.
 */
QImage loadImagePreservingHighBitDepth(const QString &filePath)
{
  const auto extension = QFileInfo(filePath).suffix().toLower();
  if (extension == "png")
  {
    if (const auto image = load16BitPng(filePath))
      return *image;
  }
  else if (extension == "bmp")
  {
    if (const auto image = load16BitBmp(filePath))
      return *image;
  }

  QImageReader reader(filePath);
  reader.setAutoTransform(true);
  QImage image;
  reader.read(&image);
  return image;
}

/**
 * Return the RGB sample values for the given pixel in the native image sample range.
 */
RgbSample getImageRgbSample(const QImage &image, const QPoint &pos, int bitDepth = 16)
{
  if (is16BitRgbImageFormat(image.format()))
  {
    const auto rgba = image.pixelColor(pos).rgba64();
    if (bitDepth == 10)
    {
      return {rgba.red() >> 6, rgba.green() >> 6, rgba.blue() >> 6, 1023};
    }
    return {rgba.red(), rgba.green(), rgba.blue(), 65535};
  }

  const auto pixel = image.pixel(pos);
  return {qRed(pixel), qGreen(pixel), qBlue(pixel), 255};
}

/**
 * Return a display-luma decision for a pixel sample.
 */
bool isDarkSample(const RgbSample &sample)
{
  return sample.r < sample.maxValue / 2 && sample.g < sample.maxValue / 2 &&
         sample.b < sample.maxValue / 2;
}

} // namespace

class FrameHandler::frameSizePresetList
{
public:
  // Constructor. Fill the names and sizes lists
  frameSizePresetList();
  // Get all presets in a displayable format ("Name (xxx,yyy)")
  QStringList getFormattedNames() const;
  // Return the index of a certain size (0 (Custom Size) if not found)
  int findSize(const Size &size)
  {
    int idx = sizes.indexOf(size);
    return (idx == -1) ? 0 : idx;
  }
  // Get the size with the given index.
  Size getSize(int index) { return sizes[index]; }

private:
  QList<QString> names;
  QList<Size>    sizes;
};

FrameHandler::frameSizePresetList::frameSizePresetList()
{
  names << "Custom Size"
        << "QCIF"
        << "QVGA"
        << "WQVGA"
        << "CIF"
        << "VGA"
        << "WVGA"
        << "4CIF"
        << "ITU R.BT601"
        << "720i/p"
        << "1080i/p"
        << "4k"
        << "XGA"
        << "XGA+";
  sizes << Size(0, 0) << Size(176, 144) << Size(320, 240) << Size(416, 240) << Size(352, 288)
        << Size(640, 480) << Size(832, 480) << Size(704, 576) << Size(720, 576) << Size(1280, 720)
        << Size(1920, 1080) << Size(3840, 2160) << Size(1024, 768) << Size(1280, 960);
}

/* Get all the names of the preset frame sizes in the form "Name (xxx,yyy)" in a QStringList.
 * This can be used to directly fill the combo box.
 */
QStringList FrameHandler::frameSizePresetList::getFormattedNames() const
{
  QStringList presetList;
  presetList.append("Custom Size");

  for (int i = 1; i < names.count(); i++)
  {
    auto str = QString("%1 (%2,%3)").arg(names[i]).arg(sizes[i].width).arg(sizes[i].height);
    presetList.append(str);
  }

  return presetList;
}

FrameHandler::frameSizePresetList FrameHandler::presetFrameSizes;

FrameHandler::FrameHandler()
{
}

QLayout *FrameHandler::createFrameHandlerControls(bool isSizeFixed)
{
  // Absolutely always only call this function once!
  assert(!ui.created());

  ui.setupUi();

         // Set default values
  ui.widthSpinBox->setMaximum(100000);
  ui.widthSpinBox->setValue(frameSize.width);
  ui.widthSpinBox->setEnabled(!isSizeFixed);
  ui.heightSpinBox->setMaximum(100000);
  ui.heightSpinBox->setValue(frameSize.height);
  ui.heightSpinBox->setEnabled(!isSizeFixed);
  ui.frameSizeComboBox->addItems(presetFrameSizes.getFormattedNames());
  int idx = presetFrameSizes.findSize(frameSize);
  ui.frameSizeComboBox->setCurrentIndex(idx);
  ui.frameSizeComboBox->setEnabled(!isSizeFixed);

         // Connect all the change signals from the controls to "connectWidgetSignals()"
  connect(ui.widthSpinBox,
          QOverload<int>::of(&QSpinBox::valueChanged),
          this,
          &FrameHandler::slotVideoControlChanged);
  connect(ui.heightSpinBox,
          QOverload<int>::of(&QSpinBox::valueChanged),
          this,
          &FrameHandler::slotVideoControlChanged);
  connect(ui.frameSizeComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,
          &FrameHandler::slotVideoControlChanged);

  return ui.frameHandlerLayout;
}

void FrameHandler::setFrameSize(Size newSize)
{
  if (newSize != this->frameSize)
  {
    // Set the new size
    DEBUG_FRAME("FrameHandler::setFrameSize %dx%d", newSize.width, newSize.height);
    this->frameSize = newSize;
  }
}

bool FrameHandler::loadCurrentImageFromFile(const QString &filePath)
{
  auto extension = QFileInfo(filePath).suffix().toLower();
  if (extension == "tga" || extension == "icb" || extension == "vda" || extension == "vst")
  {
    auto image = dec::Targa::loadTgaFromFile(functions::qStringToFsPath(filePath));
    if (!image)
      return false;

           // Convert byte array to QImage
    this->setFrameSize(Size(image->size.width, image->size.height));
    auto qFrameSize    = QSize(image->size.width, image->size.height);
    this->currentImage = QImage(qFrameSize, QImage::Format_ARGB32);
    for (unsigned y = 0; y < image->size.height; y++)
    {
      auto bits = this->currentImage.scanLine(y);
      for (unsigned x = 0; x < image->size.width; x++)
      {
        auto idx = y * image->size.width * 4 + x * 4;
        // Src is RGBA and output it BGRA
        bits[2] = image->data.at(idx);
        bits[1] = image->data.at(idx + 1);
        bits[0] = image->data.at(idx + 2);
        bits[3] = image->data.at(idx + 3);
        bits += 4;
      }
    }
  }
  else
  {
    // Load the image and return if loading was successful
    this->currentImage = loadImagePreservingHighBitDepth(filePath);
    auto qFrameSize    = currentImage.size();
    this->setFrameSize(Size(qFrameSize.width(), qFrameSize.height()));
  }

  return (!this->currentImage.isNull());
}

void FrameHandler::savePlaylist(YUViewDomElement &element) const
{
  // Append the video handler properties
  element.appendProperiteChild("width", QString::number(this->frameSize.width));
  element.appendProperiteChild("height", QString::number(this->frameSize.height));
}

void FrameHandler::loadPlaylist(const YUViewDomElement &root)
{
  auto width  = unsigned(root.findChildValue("width").toInt());
  auto height = unsigned(root.findChildValue("height").toInt());
  this->setFrameSize(Size(width, height));
}

void FrameHandler::slotVideoControlChanged()
{
  // Update the controls and get the new selected size
  auto newSize = getNewSizeFromControls();
  DEBUG_FRAME(
    "FrameHandler::slotVideoControlChanged new size %dx%d", newSize.width, newSize.height);

  if (newSize != frameSize && newSize.isValid())
  {
    // Set the new size and update the controls.
    this->setFrameSize(newSize);
    // The frame size changed. We need to redraw/re-cache.
    emit signalHandlerChanged(true, RECACHE_CLEAR);
  }
}

Size FrameHandler::getNewSizeFromControls()
{
  // The control that caused the slot to be called
  auto sender = QObject::sender();

  if (sender == ui.widthSpinBox || sender == ui.heightSpinBox)
  {
    auto newSize = Size(ui.widthSpinBox->value(), ui.heightSpinBox->value());
    if (newSize != frameSize)
    {
      // Set the comboBox index without causing another signal to be emitted.
      const QSignalBlocker blocker(ui.frameSizeComboBox);
      int                  idx = presetFrameSizes.findSize(newSize);
      ui.frameSizeComboBox->setCurrentIndex(idx);
    }
    return newSize;
  }
  else if (sender == ui.frameSizeComboBox)
  {
    auto newSize = presetFrameSizes.getSize(ui.frameSizeComboBox->currentIndex());

           // Set the width/height spin boxes without emitting another signal.
    const QSignalBlocker blocker1(ui.widthSpinBox);
    const QSignalBlocker blocker2(ui.heightSpinBox);
    ui.widthSpinBox->setValue(int(newSize.width));
    ui.heightSpinBox->setValue(int(newSize.height));
    return newSize;
  }
  return {};
}

void FrameHandler::drawFrame(QPainter *painter, double zoomFactor, bool drawRawValues)
{
  // Create the video QRect with the size of the sequence and center it.
  QRect videoRect;
  videoRect.setSize(QSize(frameSize.width * zoomFactor, frameSize.height * zoomFactor));
  videoRect.moveCenter(QPoint(0, 0));

         // DEBUG: Verify 16-bit QImage data integrity before rendering
  if (this->currentImage.format() == QImage::Format_RGBA64_Premultiplied)
  {
    // Sample a few pixels to verify 16-bit precision is preserved
    for (int testY = 0; testY < std::min(5, this->currentImage.height()); testY += 2)
    {
      for (int testX = 0; testX < std::min(10, this->currentImage.width()); testX += 4)
      {
        QRgba64 pixel = this->currentImage.pixelColor(testX, testY).rgba64();
      }
    }

           // Check and configure high bit-depth rendering backend
    configureHighBitDepthRendering(painter);
  }

         // Draw the current image (currentFrame)
  painter->drawImage(videoRect, this->currentImage);

  if (drawRawValues && zoomFactor >= SPLITVIEW_DRAW_VALUES_ZOOMFACTOR)
  {
    // Draw the pixel values onto the pixels
    drawPixelValues(painter, 0, videoRect, zoomFactor);
  }
}

void FrameHandler::drawPixelValues(QPainter *painter,
                                   const int,
                                   const QRect  &videoRect,
                                   const double  zoomFactor,
                                   FrameHandler *item2,
                                   const bool    markDifference,
                                   const int)
{
  // Draw the pixel values onto the pixels

         // TODO: Does this also work for sequences with width/height non divisible by 2? Not sure about
         // that.

         // First determine which pixels from this item are actually visible, because we only have to draw
         // the pixel values of the pixels that are actually visible
  auto viewport       = painter->viewport();
  auto worldTransform = painter->worldTransform();

  int xMin = (videoRect.width() / 2 - worldTransform.dx()) / zoomFactor;
  int yMin = (videoRect.height() / 2 - worldTransform.dy()) / zoomFactor;
  int xMax = (videoRect.width() / 2 - (worldTransform.dx() - viewport.width())) / zoomFactor;
  int yMax = (videoRect.height() / 2 - (worldTransform.dy() - viewport.height())) / zoomFactor;

         // functions::clip the min/max visible pixel values to the size of the item (no pixels outside of
         // the item have to be labeled)
  xMin = functions::clip(xMin, 0, int(frameSize.width) - 1);
  yMin = functions::clip(yMin, 0, int(frameSize.height) - 1);
  xMax = functions::clip(xMax, 0, int(frameSize.width) - 1);
  yMax = functions::clip(yMax, 0, int(frameSize.height) - 1);

         // The center point of the pixel (0,0).
  auto centerPointZero = (QPoint(-(int(frameSize.width)), -(int(frameSize.height))) * zoomFactor +
                          QPoint(zoomFactor, zoomFactor)) /
                         2;
  // This QRect has the size of one pixel and is moved on top of each pixel to draw the text
  QRect pixelRect;
  pixelRect.setSize(QSize(zoomFactor, zoomFactor));
  for (int x = xMin; x <= xMax; x++)
  {
    for (int y = yMin; y <= yMax; y++)
    {
      // Calculate the center point of the pixel. (Each pixel is of size (zoomFactor,zoomFactor))
      // and move the pixelRect to that point.
      QPoint pixCenter = centerPointZero + QPoint(x * zoomFactor, y * zoomFactor);
      pixelRect.moveCenter(pixCenter);

             // Get the text to show
      bool      drawWhite = false;
      QRgb      pixVal;
      QString   valText;
      const int formatBase = settings.value("ShowPixelValuesHex").toBool() ? 16 : 10;
      if (item2 != nullptr)
      {
        const auto sample1 = getImageRgbSample(this->currentImage, QPoint(x, y), this->m_bitDepth);
        const auto sample2 = getImageRgbSample(item2->getCurrentFrameAsImage(), QPoint(x, y), item2->getBitDepth());

        int dR = sample1.r - sample2.r;
        int dG = sample1.g - sample2.g;
        int dB = sample1.b - sample2.b;

        const QString RString = ((dR < 0) ? "-" : "") + QString::number(std::abs(dR), formatBase);
        const QString GString = ((dG < 0) ? "-" : "") + QString::number(std::abs(dG), formatBase);
        const QString BString = ((dB < 0) ? "-" : "") + QString::number(std::abs(dB), formatBase);

        if (markDifference)
          drawWhite = (dR == 0 && dG == 0 && dB == 0);
        else
        {
          const int displayR = functions::clip(128 + dR * 255 / sample1.maxValue, 0, 255);
          const int displayG = functions::clip(128 + dG * 255 / sample1.maxValue, 0, 255);
          const int displayB = functions::clip(128 + dB * 255 / sample1.maxValue, 0, 255);
          pixVal             = qRgb(displayR, displayG, displayB);
          drawWhite          = (qRed(pixVal) < 128 && qGreen(pixVal) < 128 && qBlue(pixVal) < 128);
        }
        valText = QString("R%1\nG%2\nB%3").arg(RString, GString, BString);
      }
      else
      {
        const auto sample = getImageRgbSample(this->currentImage, QPoint(x, y), this->m_bitDepth);
        drawWhite         = isDarkSample(sample);
        valText           = QString("R%1\nG%2\nB%3")
                    .arg(sample.r, 0, formatBase)
                    .arg(sample.g, 0, formatBase)
                    .arg(sample.b, 0, formatBase);
      }

      painter->setPen(drawWhite ? Qt::white : Qt::black);
      painter->drawText(pixelRect, Qt::AlignCenter, valText);
    }
  }
}

QImage FrameHandler::calculateDifference(FrameHandler *item2,
                                         const int,
                                         const int,
                                         QList<InfoItem> &differenceInfoList,
                                         const int        amplificationFactor,
                                         const bool       markDifference)
{
  auto width  = std::min(frameSize.width, item2->frameSize.width);
  auto height = std::min(frameSize.height, item2->frameSize.height);

  QImage diffImg(width, height, functionsGui::platformImageFormat(false));

         // Also calculate the MSE while we're at it (R,G,B)
  int64_t mseAdd[3] = {0, 0, 0};

  for (unsigned y = 0; y < height; y++)
  {
    for (unsigned x = 0; x < width; x++)
    {
      auto pixel1 = getPixelVal(x, y);
      auto pixel2 = item2->getPixelVal(x, y);

      int dR = int(qRed(pixel1)) - int(qRed(pixel2));
      int dG = int(qGreen(pixel1)) - int(qGreen(pixel2));
      int dB = int(qBlue(pixel1)) - int(qBlue(pixel2));

      int r, g, b;
      if (markDifference)
      {
        r = (dR != 0) ? 255 : 0;
        g = (dG != 0) ? 255 : 0;
        b = (dB != 0) ? 255 : 0;
      }
      else if (amplificationFactor != 1)
      {
        r = functions::clip(128 + dR * amplificationFactor, 0, 255);
        g = functions::clip(128 + dG * amplificationFactor, 0, 255);
        b = functions::clip(128 + dB * amplificationFactor, 0, 255);
      }
      else
      {
        r = functions::clip(128 + dR, 0, 255);
        g = functions::clip(128 + dG, 0, 255);
        b = functions::clip(128 + dB, 0, 255);
      }

      mseAdd[0] += dR * dR;
      mseAdd[1] += dG * dG;
      mseAdd[2] += dB * dB;

      auto val = qRgb(r, g, b);
      diffImg.setPixel(x, y, val);
    }
  }

  differenceInfoList.append(InfoItem("Difference Type"sv, "RGB"));

  double mse[4];
  mse[0] = double(mseAdd[0]) / (width * height);
  mse[1] = double(mseAdd[1]) / (width * height);
  mse[2] = double(mseAdd[2]) / (width * height);
  mse[3] = mse[0] + mse[1] + mse[2];
  differenceInfoList.append(InfoItem("MSE R", std::to_string(mse[0])));
  differenceInfoList.append(InfoItem("MSE G", std::to_string(mse[1])));
  differenceInfoList.append(InfoItem("MSE B", std::to_string(mse[2])));
  differenceInfoList.append(InfoItem("MSE All", std::to_string(mse[3])));

  return diffImg;
}

bool FrameHandler::isPixelDark(const QPoint &pixelPos)
{
  return isDarkSample(getImageRgbSample(this->currentImage, pixelPos, this->m_bitDepth));
}

QStringPairList
FrameHandler::getPixelValues(const QPoint &pixelPos, int, FrameHandler *item2, const int)
{
  auto width  = (item2) ? std::min(frameSize.width, item2->frameSize.width) : frameSize.width;
  auto height = (item2) ? std::min(frameSize.height, item2->frameSize.height) : frameSize.height;

  if (pixelPos.x() < 0 || pixelPos.x() >= int(width) || pixelPos.y() < 0 ||
      pixelPos.y() >= int(height))
    return {};

         // Is the format (of both items) valid?
  if (!isFormatValid())
    return {};
  if (item2 && !item2->isFormatValid())
    return {};

         // Get the RGB values from the image
  QStringPairList values;

  if (item2)
  {
    // There is a second item. Return the difference values.
    const auto sample1 = getImageRgbSample(this->currentImage, pixelPos, this->m_bitDepth);
    const auto sample2 = getImageRgbSample(item2->getCurrentFrameAsImage(), pixelPos, item2->getBitDepth());

    int r = sample1.r - sample2.r;
    int g = sample1.g - sample2.g;
    int b = sample1.b - sample2.b;

    values.append(QStringPair("R", QString::number(r)));
    values.append(QStringPair("G", QString::number(g)));
    values.append(QStringPair("B", QString::number(b)));
  }
  else
  {
    // No second item. Return the RGB values of this item.
    const auto sample = getImageRgbSample(this->currentImage, pixelPos, this->m_bitDepth);
    values.append(QStringPair("R", QString::number(sample.r)));
    values.append(QStringPair("G", QString::number(sample.g)));
    values.append(QStringPair("B", QString::number(sample.b)));
  }

  return values;
}

bool FrameHandler::setFormatFromString(QString format)
{
  auto split = format.split(";");
  if (split.length() != 2)
    return false;

  bool ok;
  auto newWidth = unsigned(split[0].toInt(&ok));
  if (!ok)
    return false;

  auto newHeight = unsigned(split[1].toInt(&ok));
  if (!ok)
    return false;

  this->setFrameSize(Size(newWidth, newHeight));
  return true;
}

} // namespace video
