#include <QtTest>

#include <video/rgb/ConversionRGB.h>

#include <random>

using namespace video;
using namespace video::rgb;

/* The Issue was that the conversion function reads from the input buffer at
 * a negative offset in case of planar if the input has no alpha channel.
 * Unfortunately, catching this invalid access is a bit hard. Here, I just
 * try to recreate this by using a big artificial frame size.
 * I don't see any other way currently to test for the bug. I was also only
 * able to see this fail on Windows. On mac and ubuntu it always succeeds.
 */

namespace
{

constexpr Size TEST_FRAME_SIZE = {3840, 2160};

auto createRandomRawRGBData() -> QByteArray
{
  QByteArray data;

  std::random_device                                       randomDevice;
  std::mt19937                                             randomNumberGenerator(randomDevice());
  std::uniform_int_distribution<std::mt19937::result_type> distribution(0, 255);

  constexpr auto NR_RGB_BYTES = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 3;
  for (unsigned i = 0; i < NR_RGB_BYTES; i++)
    data.push_back(distribution(randomNumberGenerator));

  data.squeeze();

  return data;
}

} // namespace

class Issue511RGBConversionInvalidMemoryAccess : public QObject
{
  Q_OBJECT

private slots:
  void testBasicConvertInputRGBToRGBA();
};

void Issue511RGBConversionInvalidMemoryAccess::testBasicConvertInputRGBToRGBA()
{
  PixelFormatRGB format(8, DataLayout::Planar, ChannelOrder::RGB);
  const auto     data = createRandomRawRGBData();

  constexpr auto NR_RGB_BYTES_OUTPUT = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 4;

  std::vector<unsigned char> outputBuffer;
  outputBuffer.resize(NR_RGB_BYTES_OUTPUT);

  const std::array<bool, 4> componentInvert  = {false};
  const std::array<int, 4>  componentScale   = {1, 1, 1, 1};
  const auto                limitedRange     = false;
  const auto                convertAlpha     = false;
  const auto                premultiplyAlpha = false;

  convertInputRGBToARGB(data,
                        format,
                        outputBuffer.data(),
                        TEST_FRAME_SIZE,
                        componentInvert.data(),
                        componentScale.data(),
                        limitedRange,
                        convertAlpha,
                        premultiplyAlpha);
}

QTEST_MAIN(Issue511RGBConversionInvalidMemoryAccess)

#include "Issue511RGBConversionInvalidMemoryAccess.moc"
