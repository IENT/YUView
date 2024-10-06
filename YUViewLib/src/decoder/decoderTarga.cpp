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

/* The tga test suite: https://github.com/image-rs/image/tree/master/tests/images/tga/testsuite
 */

#include "decoderTarga.h"

#include <fstream>
#include <iostream>

namespace
{

using color_t = uint32_t;

const color_t color_r_shift = 0;
const color_t color_g_shift = 8;
const color_t color_b_shift = 16;
const color_t color_a_shift = 24;

inline uint8_t getr(color_t c) { return (c >> color_r_shift) & 0xff; }
inline uint8_t getg(color_t c) { return (c >> color_g_shift) & 0xff; }
inline uint8_t getb(color_t c) { return (c >> color_b_shift) & 0xff; }
inline uint8_t geta(color_t c) { return (c >> color_a_shift) & 0xff; }
inline color_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
  return ((r << color_r_shift) | (g << color_g_shift) | (b << color_b_shift) |
          (a << color_a_shift));
}

static inline uint8_t scale_5bits_to_8bits(uint8_t v)
{
  assert(v < 32);
  return (v << 3) | (v >> 2);
}

enum class ImageType
{
  NoImage,
  UncompressedIndexed,
  UncompressedRgb,
  UncompressedGray,
  RleIndexed,
  RleRgb,
  RleGray,
};

class Colormap
{
public:
  Colormap() = default;
  Colormap(const int n) : m_color(n) {}

  int size() const { return int(m_color.size()); }

  const color_t &operator[](int i) const { return m_color[i]; }

  color_t &operator[](int i) { return m_color[i]; }

  bool operator==(const Colormap &o) const
  {
    for (int i = 0; i < int(m_color.size()); ++i)
    {
      if (m_color[i] != o[i])
        return false;
    }
    return true;
  }

  bool operator!=(const Colormap &o) const { return !operator==(o); }

private:
  std::vector<color_t> m_color;
};

struct Header
{
  uint8_t     idLength{};
  uint8_t     colormapType{};
  ImageType   imageType{};
  uint16_t    colormapOrigin{};
  uint16_t    colormapLength{};
  uint8_t     colormapDepth{};
  uint16_t    xOrigin{};
  uint16_t    yOrigin{};
  uint16_t    width{};
  uint16_t    height{};
  uint8_t     bitsPerPixel{};
  uint8_t     imageDescriptor{};
  std::string imageId{};
  Colormap    colormap{};

  bool hasAlpha{};
  bool leftToRight{};
  bool topToBottom{};

  bool hasColormap() const { return (colormapLength > 0); }

  bool isRgb() const
  {
    return (imageType == ImageType::UncompressedRgb || imageType == ImageType::RleRgb);
  }

  bool isIndexed() const
  {
    return (imageType == ImageType::UncompressedIndexed || imageType == ImageType::RleIndexed);
  }

  bool isGray() const
  {
    return (imageType == ImageType::UncompressedGray || imageType == ImageType::RleGray);
  }

  bool isUncompressed() const
  {
    return (imageType == ImageType::UncompressedIndexed ||
            imageType == ImageType::UncompressedRgb || imageType == ImageType::UncompressedGray);
  }

  bool isRle() const
  {
    return (imageType == ImageType::RleIndexed || imageType == ImageType::RleRgb ||
            imageType == ImageType::RleGray);
  }

  bool validColormapType() const
  {
    return
        // Indexed with palette
        (isIndexed() && bitsPerPixel == 8 && colormapType == 1) ||
        // Grayscale without palette
        (isGray() && bitsPerPixel == 8 && colormapType == 0) ||
        // Non-indexed without palette
        (bitsPerPixel > 8 && colormapType == 0);
  }

  bool valid() const
  {
    switch (imageType)
    {
    case ImageType::UncompressedIndexed:
    case ImageType::RleIndexed:
      return (bitsPerPixel == 8);
    case ImageType::UncompressedRgb:
    case ImageType::RleRgb:
      return (bitsPerPixel == 15 || bitsPerPixel == 16 || bitsPerPixel == 24 || bitsPerPixel == 32);
    case ImageType::UncompressedGray:
    case ImageType::RleGray:
      return (bitsPerPixel == 8);
    default:
      return false;
    }
  }

  // Returns the number of bytes per pixel needed in an image
  // created with this Header information.
  int bytesPerPixel() const
  {
    if (isRgb())
      return 4;
    else
      return 1;
  }
};

uint8_t read8(std::ifstream &file)
{
  char c;
  file.get(c);
  return uint8_t(c);
}

uint16_t read16(std::ifstream &file)
{
  char b1, b2;
  file.get(b1);
  file.get(b2);
  auto     u1  = uint8_t(b1);
  auto     u2  = uint8_t(b2);
  uint16_t out = (u2 << 8) + u1;
  return out; // Little endian
}

color_t read16AsRgb(std::ifstream &file, bool hasAlpha)
{
  const auto v = read16(file);
  uint8_t    a = 255;
  if (hasAlpha)
  {
    if ((v & 0x8000) == 0) // Transparent bit
      a = 0;
  }
  return rgba(scale_5bits_to_8bits((v >> 10) & 0x1F),
              scale_5bits_to_8bits((v >> 5) & 0x1F),
              scale_5bits_to_8bits(v & 0x1F),
              a);
}

color_t read24AsRgb(std::ifstream &file)
{
  const uint8_t b = read8(file);
  const uint8_t g = read8(file);
  const uint8_t r = read8(file);
  return rgba(r, g, b, 255);
}

color_t read32AsRgb(std::ifstream &file, bool hasAlpha)
{
  const uint8_t b = read8(file);
  const uint8_t g = read8(file);
  const uint8_t r = read8(file);
  uint8_t       a = read8(file);
  if (!hasAlpha)
    a = 255;
  return rgba(r, g, b, a);
}

void setColorInImage(
    dec::Targa::Image &image, color_t color, unsigned x, unsigned y, const Header &header)
{
  unsigned idx = 0;

  if (header.topToBottom)
    idx += y * header.width * 4;
  else
    idx += (header.height - y - 1) * header.width * 4;

  if (header.leftToRight)
    idx += x * 4;
  else
    idx += (header.width - x - 1) * 4;

  image.data.at(idx)     = getr(color);
  image.data.at(idx + 1) = getg(color);
  image.data.at(idx + 2) = getb(color);
  image.data.at(idx + 3) = geta(color);
}

void readColormap(std::ifstream &file, Header &header)
{
  header.colormap = Colormap(header.colormapLength);

  for (int i = 0; i < header.colormapLength; ++i)
  {
    switch (header.colormapDepth)
    {

    case 15:
    case 16:
    {
      const uint16_t c   = read16(file);
      header.colormap[i] = rgba(scale_5bits_to_8bits((c >> 10) & 0x1F),
                                scale_5bits_to_8bits((c >> 5) & 0x1F),
                                scale_5bits_to_8bits(c & 0x1F));
      break;
    }

    case 24:
    case 32:
    {
      const uint8_t b = read8(file);
      const uint8_t g = read8(file);
      const uint8_t r = read8(file);
      uint8_t       a;
      if (header.colormapDepth == 32)
        a = read8(file);
      else
        a = 255;
      header.colormap[i] = rgba(r, g, b, a);
      break;
    }
    }
  }
}

color_t readRunLengthColor(std::ifstream &file, const Header &header)
{
  color_t color{};
  if (header.imageType == ImageType::RleIndexed)
  {
    auto colormapIndex = read8(file);
    color              = header.colormap[colormapIndex];
  }
  else if (header.imageType == ImageType::RleGray)
  {
    auto greyValue = read8(file);
    color          = rgba(greyValue, greyValue, greyValue);
  }
  else if (header.imageType == ImageType::RleRgb)
  {
    switch (header.bitsPerPixel)
    {
    case 15:
    case 16:
      color = read16AsRgb(file, header.hasAlpha);
      break;
    case 24:
      color = read24AsRgb(file);
      break;
    case 32:
      color = read32AsRgb(file, header.hasAlpha);
      break;
    default:
      assert(false);
      break;
    }
  }
  else
    assert(false);
  return color;
}

// In the best case (TGA 2.0 spec) this should read just one
// scanline, but in old TGA versions (1.0) it was possible to save
// several scanlines with the same RLE data.
//
// Returns true when are are done.
bool readRunLengthDataIntoImage(std::ifstream &    file,
                                const Header &     header,
                                dec::Targa::Image &image,
                                unsigned           yStart)
{
  unsigned y = yStart;
  unsigned x = 0;
  for (int j = 0; j < header.width && !file.eof();)
  {
    int c = read8(file);
    if (c & 0x80)
    {
      c = (c & 0x7f) + 1;
      j += c;

      auto color = readRunLengthColor(file, header);
      while (c-- > 0)
      {
        setColorInImage(image, color, x, y, header);
        x++;
        if (x >= header.width)
        {
          y++;
          x = 0;
        }
      }
    }
    else
    {
      ++c;
      j += c;
      while (c-- > 0)
      {
        auto color = readRunLengthColor(file, header);
        setColorInImage(image, color, x, y, header);
        x++;
        if (x >= header.width)
        {
          y++;
          x = 0;
        }
      }
    }
  }
  return false;
}

std::optional<Header> readHeader(std::ifstream &file)
{
  Header header;
  header.idLength     = read8(file);
  header.colormapType = read8(file);

  auto imageTypeIdx = read8(file);
  switch (imageTypeIdx)
  {
  case 0:
    header.imageType = ImageType::NoImage;
    break;
  case 1:
    header.imageType = ImageType::UncompressedIndexed;
    break;
  case 2:
    header.imageType = ImageType::UncompressedRgb;
    break;
  case 3:
    header.imageType = ImageType::UncompressedGray;
    break;
  case 9:
    header.imageType = ImageType::RleIndexed;
    break;
  case 10:
    header.imageType = ImageType::RleRgb;
    break;
  case 11:
    header.imageType = ImageType::RleGray;
    break;
  default:
    break;
  }

  header.colormapOrigin  = read16(file);
  header.colormapLength  = read16(file);
  header.colormapDepth   = read8(file);
  header.xOrigin         = read16(file);
  header.yOrigin         = read16(file);
  header.width           = read16(file);
  header.height          = read16(file);
  header.bitsPerPixel    = read8(file);
  header.imageDescriptor = read8(file);

  header.leftToRight = (!(header.imageDescriptor & 0x10));
  header.topToBottom = (header.imageDescriptor & 0x20);

  // Invalid image size
  if (header.width == 0 || header.height == 0)
    return {};

  // Skip ID string (idLength bytes)
  if (header.idLength > 0)
  {
    auto i = header.idLength;
    while (i--)
    {
      auto chr = read8(file);
      header.imageId.push_back(chr);
    }
  }

#if 0
  // In the best case the "alphaBits" should be valid, but there are
  // invalid TGA files out there which don't indicate the
  // "alphaBits" correctly, so they could be 0 and use the alpha
  // channel anyway on each pixel.
  int alphaBits = (header.imageDescriptor & 15);
  header.hasAlpha =
    (header.bitsPerPixel == 32 && alphaBits == 8) ||
    (header.bitsPerPixel == 16 && alphaBits == 1);
#else
  // So to detect if a 32bpp or 16bpp TGA image has alpha, we'll use
  // the "alpha histogram" in postProcessImage() to check if there are
  // different alpha values. If there is only one alpha value (all 0
  // or all 255), we create an opaque image anyway. The only exception
  // to this rule is when all pixels are black and transparent
  // (RGBA=0), that is the only case when an image is fully
  // transparent.
  //
  // Note: This same heuristic is used in apps like macOS Preview:
  // https://twitter.com/davidcapello/status/1242803110868893697
  header.hasAlpha = (header.bitsPerPixel == 32) || (header.bitsPerPixel == 16);
#endif

  // Read colormap
  if (header.colormapType == 1)
    readColormap(file, header);

  if (!header.validColormapType() || !header.valid())
    return {};

  return header;
}

dec::Targa::Image readImage(std::ifstream &file, const Header &header)
{
  dec::Targa::Image image;
  image.size.width  = header.width;
  image.size.height = header.height;
  image.data.resize(header.width * 4 * header.height);

  // Bit 4 means right-to-left, else left-to-right
  // Bit 5 means top-to-bottom, else bottom-to-top

  for (int y = 0; y < header.height; ++y)
  {
    if (header.imageType == ImageType::UncompressedIndexed)
    {
      assert(header.bitsPerPixel == 8);
      for (int x = 0; x < header.width; ++x)
      {
        auto colormapIdx = read8(file);
        auto color       = header.colormap[colormapIdx];
        setColorInImage(image, color, x, y, header);
      }
    }
    else if (header.imageType == ImageType::UncompressedRgb)
    {
      color_t color{};
      for (int x = 0; x < header.width; ++x)
      {
        switch (header.bitsPerPixel)
        {
        case 15:
        case 16:
          color = read16AsRgb(file, header.hasAlpha);
          break;
        case 24:
          color = read24AsRgb(file);
          break;
        case 32:
          color = read32AsRgb(file, header.hasAlpha);
          break;
        default:
          assert(false);
          break;
        }
        setColorInImage(image, color, x, y, header);
      }
    }
    else if (header.imageType == ImageType::UncompressedGray)
    {
      assert(header.bitsPerPixel == 8);
      for (int x = 0; x < header.width; ++x)
      {
        auto greyValue = read8(file);
        setColorInImage(image, rgba(greyValue, greyValue, greyValue, 255), x, y, header);
      }
    }
    else if (header.imageType == ImageType::RleIndexed || header.imageType == ImageType::RleGray)
    {
      assert(header.bitsPerPixel == 8);
      readRunLengthDataIntoImage(file, header, image, y);
    }
    else if (header.imageType == ImageType::RleRgb)
    {
      readRunLengthDataIntoImage(file, header, image, y);
    }
  }

  return image;
}

void postProcessImage(dec::Targa::Image &image, const Header &header)
{
  // The post-processing is only for RGB images with possible invalid
  // alpha information.
  if (!header.isRgb() || !header.hasAlpha)
    return;

  bool transparentImage = true;
  bool blackImage       = true;

  for (unsigned y = 0; y < header.height; ++y)
  {
    for (unsigned x = 0; x < header.width; ++x)
    {
      auto idx = y * header.width * 4 + x * 4;
      auto r   = image.data.at(idx);
      auto g   = image.data.at(idx + 1);
      auto b   = image.data.at(idx + 2);
      auto a   = image.data.at(idx + 3);
      if (transparentImage && a != 0)
      {
        transparentImage = false;
      }
      if (blackImage && (r != 0 || g != 0 || b != 0))
      {
        blackImage = false;
      }
    }
  }

  // If the image is fully transparent (all pixels with alpha=0) and
  // there are pixels with RGB != 0 (!blackImage), we have to make the
  // image completely opaque (alpha=255).
  if (transparentImage && !blackImage)
  {
    for (unsigned y = 0; y < header.height; ++y)
    {
      for (unsigned x = 0; x < header.width; ++x)
      {
        auto idx               = y * header.width * 4 + x * 4;
        image.data.at(idx + 3) = 255;
      }
    }
  }
}

} // namespace

std::optional<dec::Targa::Image> dec::Targa::loadTgaFromFile(std::string filename)
{
  std::ifstream tgaFile(filename, std::ios::binary);

  auto header = readHeader(tgaFile);
  if (!header)
    return {};

  try
  {
    auto image = readImage(tgaFile, *header);
    postProcessImage(image, *header);
    return image;
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
  }

  return {};
}
