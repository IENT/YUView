#include <QtTest>

#include <video/rgb/ConversionRGB.h>

using namespace video;
using namespace video::rgb;

namespace
{

constexpr std::array<rgba_t, 16> TEST_VALUES_8BIT = {{{0, 0, 0, 0},
                                                      {33, 0, 0, 0},
                                                      {178, 0, 0, 22},
                                                      {255, 0, 0, 88},
                                                      {0, 33, 0, 123},
                                                      {0, 178, 0, 0},
                                                      {0, 255, 0, 89},
                                                      {0, 0, 33, 0},
                                                      {0, 0, 178, 0},
                                                      {0, 0, 255, 0},
                                                      {45, 43, 34, 255},
                                                      {255, 24, 129, 255},
                                                      {255, 255, 3, 0},
                                                      {3, 255, 22, 0},
                                                      {255, 255, 255, 0},
                                                      {255, 255, 255, 255}}};
constexpr Size                   TEST_FRAME_SIZE  = {4, 4};

auto createRawRGBData(const PixelFormatRGB &format) -> QByteArray
{
  QByteArray data;

  if (format.getDataLayout() == DataLayout::Packed)
  {
    for (const auto value : TEST_VALUES_8BIT)
    {
      for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
           channelPosition++)
      {
        const auto channel = format.getChannelAtPosition(channelPosition);
        switch (channel)
        {
        case Channel::Red:
          data.push_back(value.R);
          break;
        case Channel::Green:
          data.push_back(value.G);
          break;
        case Channel::Blue:
          data.push_back(value.B);
          break;
        case Channel::Alpha:
          data.push_back(value.A);
          break;
        }
      }
    }
  }
  else
  {
    for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
         channelPosition++)
    {
      const auto channel = format.getChannelAtPosition(channelPosition);
      if (channel == Channel::Red)
      {
        for (const auto value : TEST_VALUES_8BIT)
          data.push_back(value.R);
      }
      else if (channel == Channel::Green)
      {
        for (const auto value : TEST_VALUES_8BIT)
          data.push_back(value.G);
      }
      else if (channel == Channel::Blue)
      {
        for (const auto value : TEST_VALUES_8BIT)
          data.push_back(value.B);
      }
      else if (channel == Channel::Alpha)
      {
        for (const auto value : TEST_VALUES_8BIT)
          data.push_back(value.A);
      }
    }
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
