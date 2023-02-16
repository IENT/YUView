#include <QtTest>

#include <video/rgb/ConversionRGB.h>

using namespace video;
using namespace video::rgb;

namespace
{

constexpr std::array<rgba_t, 16> TEST_VALUES_12BIT = {{{0, 0, 0, 0},
                                                       {156, 0, 0, 0},
                                                       {560, 0, 0, 98},
                                                       {1023, 0, 0, 700},
                                                       {0, 156, 0, 852},
                                                       {0, 760, 0, 0},
                                                       {0, 1023, 0, 230},
                                                       {0, 0, 156, 0},
                                                       {0, 0, 576, 0},
                                                       {0, 0, 1023, 0},
                                                       {213, 214, 265, 1023},
                                                       {1023, 78, 234, 1023},
                                                       {1023, 1023, 3, 0},
                                                       {16, 1023, 22, 0},
                                                       {1023, 1023, 1023, 0},
                                                       {1023, 1023, 1023, 1023}}};
constexpr Size                   TEST_FRAME_SIZE   = {4, 4};

void scaleValueToBitDepthAndPushIntoArrayLittleEndian(QByteArray &   data,
                                                      const unsigned value,
                                                      const int      bitDepth)
{
  const auto shift = 12 - bitDepth;
  if (bitDepth == 8)
    data.push_back(value >> shift);
  else
  {
    const auto scaledValue = (value >> shift);
    const auto upperByte   = ((scaledValue & 0xff00) >> 8);
    const auto lowerByte   = (scaledValue & 0xff);
    data.push_back(lowerByte);
    data.push_back(upperByte);
  }
}

auto createRawRGBData(const PixelFormatRGB &format) -> QByteArray
{
  QByteArray data;
  const auto bitDepth = format.getBitsPerSample();

  if (format.getDataLayout() == DataLayout::Packed)
  {
    for (auto value : TEST_VALUES_12BIT)
    {
      for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
           channelPosition++)
      {
        const auto channel = format.getChannelAtPosition(channelPosition);
        scaleValueToBitDepthAndPushIntoArrayLittleEndian(data, value[channel], bitDepth);
      }
    }
  }
  else
  {
    for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
         channelPosition++)
    {
      const auto channel = format.getChannelAtPosition(channelPosition);
      for (auto value : TEST_VALUES_12BIT)
        scaleValueToBitDepthAndPushIntoArrayLittleEndian(data, value[channel], bitDepth);
    }
  }

  data.squeeze();
  return data;
}

void checkOutputValues(const std::vector<unsigned char> &data, const bool alphaShouldBeSet)
{
  constexpr auto nrValues = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

  for (int i = 0; i < nrValues; ++i)
  {
    const auto pixelOffset = i * 4;

    const auto originalValue       = TEST_VALUES_12BIT.at(i);
    const auto bitDepthShiftTo8Bit = 4;
    rgba_t     expectedValue({originalValue.R >> bitDepthShiftTo8Bit,
                          originalValue.G >> bitDepthShiftTo8Bit,
                          originalValue.B >> bitDepthShiftTo8Bit,
                          originalValue.A >> bitDepthShiftTo8Bit});

    // Conversion output is ARGB Little Endian
    const rgba_t actualValue = {data.at(pixelOffset + 2),
                                data.at(pixelOffset + 1),
                                data.at(pixelOffset),
                                data.at(pixelOffset + 3)};

    if (!alphaShouldBeSet)
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
  void testBasicConvertInputRGBToARGB_AllValuesShouldBeIdentical();
};

void ConversionRGBTest::testBasicConvertInputRGBToARGB_AllValuesShouldBeIdentical()
{

  for (auto bitDepth : {8, 10, 12})
  {
    for (const auto alphaMode : AlphaModeMapper.entries())
    {
      for (const auto dataLayout : DataLayoutMapper.entries())
      {
        for (const auto channelOrder : ChannelOrderMapper.entries())
        {
          PixelFormatRGB format(bitDepth, dataLayout.value, channelOrder.value, alphaMode.value);
          const auto     data = createRawRGBData(format);

          for (const auto outputHasAlpha : {false, true})
          {
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
                                  outputHasAlpha,
                                  premultiplyAlpha);

            try
            {
              const auto alphaShouldBeSet = (outputHasAlpha && alphaMode.value != AlphaMode::None);
              checkOutputValues(outputBuffer, alphaShouldBeSet);
            }
            catch (const std::exception &e)
            {
              const auto errorMessage = "Error checking " + std::string(e.what()) +
                                        " for bitDepth " + std::to_string(bitDepth) +
                                        " outputHasAlpga " + std::to_string(outputHasAlpha) +
                                        " alphaMode " + alphaMode.name + " dataLayout " +
                                        dataLayout.name + " channelOrder " + channelOrder.name;
              QFAIL(errorMessage.c_str());
            }
          }
        }
      }
    }
  }
}

QTEST_MAIN(ConversionRGBTest)

#include "ConversionRGBTest.moc"
