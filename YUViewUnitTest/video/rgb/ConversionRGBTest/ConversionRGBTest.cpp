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

using LimitedRange          = bool;
using PremultiplyAlpha      = bool;
using ScalingPerComponent   = std::array<int, 4>;
using InversionPerComponent = std::array<bool, 4>;

std::string to_string(const ScalingPerComponent &scaling)
{
  return "[" + std::to_string(scaling.at(0)) + "," + std::to_string(scaling.at(1)) + "," +
         std::to_string(scaling.at(2)) + "," + std::to_string(scaling.at(3)) + "]";
}

std::string to_string(const InversionPerComponent &inversion)
{
  return "[" + std::to_string(inversion.at(0)) + "," + std::to_string(inversion.at(1)) + "," +
         std::to_string(inversion.at(2)) + "," + std::to_string(inversion.at(3)) + "]";
}

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

void checkOutputValues(const std::vector<unsigned char> &data,
                       const int                         bitDepth,
                       const ScalingPerComponent &       scaling,
                       const InversionPerComponent &     inversion,
                       const bool                        alphaShouldBeSet)
{
  constexpr auto nrValues = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

  for (int i = 0; i < nrValues; ++i)
  {
    const auto pixelOffset = i * 4;

    auto expectedValue = TEST_VALUES_12BIT.at(i);

    auto scaleShiftClipInvertValue =
        [&bitDepth](const int value, const int scale, const bool invert) {
          const auto valueOriginalDepth = (value >> (12 - bitDepth));
          const auto valueScaled        = valueOriginalDepth * scale;
          const auto value8BitDepth     = (valueScaled >> (bitDepth - 8));
          const auto valueClipped       = functions::clip(value8BitDepth, 0, 255);
          return invert ? (255 - valueClipped) : valueClipped;
        };

    expectedValue.R = scaleShiftClipInvertValue(expectedValue.R, scaling[0], inversion[0]);
    expectedValue.G = scaleShiftClipInvertValue(expectedValue.G, scaling[1], inversion[1]);
    expectedValue.B = scaleShiftClipInvertValue(expectedValue.B, scaling[2], inversion[2]);
    expectedValue.A = scaleShiftClipInvertValue(expectedValue.A, scaling[3], inversion[3]);

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
  void testBasicConvertsion();
  void testConversionWithInversion();
  void testConversionWithLimitedToFullRange();
};

void ConversionRGBTest::testBasicConvertsion()
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
            for (const auto componentScale : {ScalingPerComponent({1, 1, 1, 1}),
                                              ScalingPerComponent({2, 1, 1, 1}),
                                              ScalingPerComponent({1, 2, 1, 1}),
                                              ScalingPerComponent({1, 1, 2, 1}),
                                              ScalingPerComponent({1, 1, 1, 2}),
                                              ScalingPerComponent({1, 8, 1, 1})})
            {
              for (const auto inversion : {InversionPerComponent({false, false, false, false}),
                                           InversionPerComponent({true, false, false, false}),
                                           InversionPerComponent({false, true, false, false}),
                                           InversionPerComponent({false, false, true, false}),
                                           InversionPerComponent({false, false, false, true}),
                                           InversionPerComponent({true, true, true, true})})
              {
                constexpr auto nrBytes = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height * 4;

                std::vector<unsigned char> outputBuffer;
                outputBuffer.resize(nrBytes);

                convertInputRGBToARGB(data,
                                      format,
                                      outputBuffer.data(),
                                      TEST_FRAME_SIZE,
                                      inversion.data(),
                                      componentScale.data(),
                                      LimitedRange(false),
                                      outputHasAlpha,
                                      PremultiplyAlpha(false));

                try
                {
                  const auto alphaShouldBeSet =
                      (outputHasAlpha && alphaMode.value != AlphaMode::None);
                  checkOutputValues(
                      outputBuffer, bitDepth, componentScale, inversion, alphaShouldBeSet);
                }
                catch (const std::exception &e)
                {
                  const auto errorMessage = "Error checking " + std::string(e.what()) +
                                            " for bitDepth " + std::to_string(bitDepth) +
                                            " outputHasAlpga " + std::to_string(outputHasAlpha) +
                                            " alphaMode " + alphaMode.name + " dataLayout " +
                                            dataLayout.name + " channelOrder " + channelOrder.name +
                                            " scaling " + to_string(componentScale) +
                                            " inversion " + to_string(inversion);
                  QFAIL(errorMessage.c_str());
                }
              }
            }
          }
        }
      }
    }
  }
}

void ConversionRGBTest::testConversionWithInversion()
{
}

void ConversionRGBTest::testConversionWithLimitedToFullRange()
{
}

QTEST_MAIN(ConversionRGBTest)

#include "ConversionRGBTest.moc"
