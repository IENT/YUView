#include <QtTest>

#include <video/rgb/ConversionRGB.h>

using namespace video;
using namespace video::rgb;

namespace
{

constexpr std::array<rgba_t, 16> TEST_VALUES_8BIT = {{{0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0},
                                                      {0, 0, 0, 0}}};
constexpr Size                   TEST_FRAME_SIZE  = {4, 4};

auto createRawRGBData(const PixelFormatRGB &format) -> QByteArray
{
  QByteArray data;

  if (format.getDataLayout() == DataLayout::Packed)
  {
    for (const auto value : TEST_VALUES_8BIT)
    {
      data.push_back(value.R);
      data.push_back(value.G);
      data.push_back(value.B);
      if (format.hasAlpha())
        data.push_back(value.A);
    }
  }
  else
  {
    for (const auto value : TEST_VALUES_8BIT)
      data.push_back(value.R);
    for (const auto value : TEST_VALUES_8BIT)
      data.push_back(value.G);
    for (const auto value : TEST_VALUES_8BIT)
      data.push_back(value.B);
    if (format.hasAlpha())
      for (const auto value : TEST_VALUES_8BIT)
        data.push_back(value.A);
  }

  data.squeeze();

  return data;
}

} // namespace

class ConversionRGBTest : public QObject
{
  Q_OBJECT

private slots:
  void testBasicConvertInputRGBToRGBA();
};

void ConversionRGBTest::testBasicConvertInputRGBToRGBA()
{
  PixelFormatRGB format(8, DataLayout::Planar, ChannelOrder::RGB);
  const auto     data = createRawRGBData(format);

  constexpr auto nrBytes = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 4;

  std::vector<unsigned char> outputBuffer;
  outputBuffer.resize(nrBytes);

  const std::array<bool, 4> componentInvert  = {false};
  const std::array<int, 4>  componentScale   = {1, 1, 1, 1};
  const auto                limitedRange     = false;
  const auto                convertAlpha     = false;
  const auto                premultiplyAlpha = false;

  convertInputRGBToRGBA(data,
                        format,
                        outputBuffer.data(),
                        TEST_FRAME_SIZE,
                        componentInvert.data(),
                        componentScale.data(),
                        limitedRange,
                        convertAlpha,
                        premultiplyAlpha);

  // Todo: Check the output
  int debugStop  = data.capacity();
  int debugStop2 = 234;
}

QTEST_MAIN(ConversionRGBTest)

#include "ConversionRGBTest.moc"
