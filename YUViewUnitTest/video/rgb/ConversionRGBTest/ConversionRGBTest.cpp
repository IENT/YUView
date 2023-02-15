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

void checkThatOutputIsIdenticalToOriginal(const std::vector<unsigned char> &data,
                                          const bool                        convertAlpha)
{
  constexpr auto nrValues = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

  for (int i = 0; i < nrValues; ++i)
  {
    const auto pixelOffset   = i * 4;
    auto       expectedValue = TEST_VALUES_8BIT.at(i);

    // Conversion output is ARGB Little Endian
    const rgba_t actualValue = {data.at(pixelOffset + 2),
                                data.at(pixelOffset + 1),
                                data.at(pixelOffset),
                                data.at(pixelOffset + 3)};

    if (!convertAlpha)
      expectedValue.A = 255;

    if (expectedValue != actualValue)
      throw std::runtime_error("Value " + std::to_string(i));
  }
}

} // namespace

class ConversionRGBTest : public QObject
{
  Q_OBJECT

private slots:
  void testBasicConvertInputRGB8BitToRGBA_AllValuesShouldBeIdentical();
};

void ConversionRGBTest::testBasicConvertInputRGB8BitToRGBA_AllValuesShouldBeIdentical()
{
  constexpr auto BIT_DEPTH = 8;

  for (const auto hasAlpha : {false, true})
  {
    for (const auto dataLayout : DataLayoutMapper.entries())
    {
      for (const auto channelOrder : ChannelOrderMapper.entries())
      {
        PixelFormatRGB format(BIT_DEPTH, dataLayout.value, channelOrder.value);
        const auto     data = createRawRGBData(format);

        constexpr auto nrBytes = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 4;

        std::vector<unsigned char> outputBuffer;
        outputBuffer.resize(nrBytes);

        const std::array<bool, 4> componentInvert  = {false};
        const std::array<int, 4>  componentScale   = {1, 1, 1, 1};
        const auto                limitedRange     = false;
        const auto                premultiplyAlpha = false;

        convertInputRGBToARGB(data,
                              format,
                              outputBuffer.data(),
                              TEST_FRAME_SIZE,
                              componentInvert.data(),
                              componentScale.data(),
                              limitedRange,
                              hasAlpha,
                              premultiplyAlpha);

        try
        {
          checkThatOutputIsIdenticalToOriginal(outputBuffer, hasAlpha);
        }
        catch (const std::exception &e)
        {
          const auto errorMessage = "Error checking " + std::string(e.what()) + " for alpha " +
                                    std::to_string(hasAlpha) + " dataLayout " + dataLayout.name +
                                    " channelOrder " + channelOrder.name;
          QFAIL(errorMessage.c_str());
        }
      }
    }
  }
}

QTEST_MAIN(ConversionRGBTest)

#include "ConversionRGBTest.moc"
