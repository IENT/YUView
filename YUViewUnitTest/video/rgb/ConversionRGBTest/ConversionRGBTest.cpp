#include <QtTest>

#include <video/LimitedRangeToFullRange.h>
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
constexpr auto TEST_FRAME_NR_VALUES                = TEST_FRAME_SIZE.width * TEST_FRAME_SIZE.height;

const std::map<Channel, int> ALL_CHANNELS = {
    {Channel::Red, 0}, {Channel::Green, 1}, {Channel::Blue, 2}, {Channel::Alpha, 3}};

using LimitedRange          = bool;
using PremultiplyAlpha      = bool;
using ScalingPerComponent   = std::array<int, 4>;
using InversionPerComponent = std::array<bool, 4>;
using UChaVector            = std::vector<unsigned char>;

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

int scaleShiftClipInvertValue(const int  value,
                              const int  bitDepth,
                              const int  scale,
                              const bool invert)
{
  const auto valueOriginalDepth = (value >> (12 - bitDepth));
  const auto valueScaled        = valueOriginalDepth * scale;
  const auto value8BitDepth     = (valueScaled >> (bitDepth - 8));
  const auto valueClipped       = functions::clip(value8BitDepth, 0, 255);
  return invert ? (255 - valueClipped) : valueClipped;
};

void scaleValueToBitDepthAndPushIntoArra(QByteArray &     data,
                                         const unsigned   value,
                                         const int        bitDepth,
                                         const Endianness endianness)
{
  const auto shift = 12 - bitDepth;
  if (bitDepth == 8)
    data.push_back(value >> shift);
  else
  {
    const auto scaledValue = (value >> shift);
    const auto upperByte   = ((scaledValue & 0xff00) >> 8);
    const auto lowerByte   = (scaledValue & 0xff);
    if (endianness == Endianness::Little)
    {
      data.push_back(lowerByte);
      data.push_back(upperByte);
    }
    else
    {
      data.push_back(upperByte);
      data.push_back(lowerByte);
    }
  }
}

auto createRawRGBData(const PixelFormatRGB &format) -> QByteArray
{
  QByteArray data;
  const auto bitDepth   = format.getBitsPerSample();
  const auto endianness = format.getEndianess();

  if (format.getDataLayout() == DataLayout::Packed)
  {
    for (auto value : TEST_VALUES_12BIT)
    {
      for (int channelPosition = 0; channelPosition < static_cast<int>(format.nrChannels());
           channelPosition++)
      {
        const auto channel = format.getChannelAtPosition(channelPosition);
        scaleValueToBitDepthAndPushIntoArra(data, value[channel], bitDepth, endianness);
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
        scaleValueToBitDepthAndPushIntoArra(data, value[channel], bitDepth, endianness);
    }
  }

  data.squeeze();
  return data;
}

rgba_t getARGBValueFromDataLittleEndian(const UChaVector &data, const int i)
{
  const auto pixelOffset = i * 4;
  return rgba_t({data.at(pixelOffset + 2),
                 data.at(pixelOffset + 1),
                 data.at(pixelOffset),
                 data.at(pixelOffset + 3)});
}

void checkOutputValues(const UChaVector &           data,
                       const int                    bitDepth,
                       const ScalingPerComponent &  scaling,
                       const bool                   limitedRange,
                       const InversionPerComponent &inversion,
                       const bool                   alphaShouldBeSet)
{
  for (int i = 0; i < TEST_FRAME_NR_VALUES; ++i)
  {
    auto expectedValue = TEST_VALUES_12BIT.at(i);

    expectedValue.R =
        scaleShiftClipInvertValue(expectedValue.R, bitDepth, scaling[0], inversion[0]);
    expectedValue.G =
        scaleShiftClipInvertValue(expectedValue.G, bitDepth, scaling[1], inversion[1]);
    expectedValue.B =
        scaleShiftClipInvertValue(expectedValue.B, bitDepth, scaling[2], inversion[2]);
    expectedValue.A =
        scaleShiftClipInvertValue(expectedValue.A, bitDepth, scaling[3], inversion[3]);

    if (limitedRange)
    {
      expectedValue.R = LimitedRangeToFullRange.at(expectedValue.R);
      expectedValue.G = LimitedRangeToFullRange.at(expectedValue.G);
      expectedValue.B = LimitedRangeToFullRange.at(expectedValue.B);
      // No limited range for alpha
    }

    if (!alphaShouldBeSet)
      expectedValue.A = 255;

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("Value " + std::to_string(i));
  }
}

void checkOutputValuesForPlane(const UChaVector &           data,
                               const PixelFormatRGB &       pixelFormat,
                               const ScalingPerComponent &  scaling,
                               const bool                   limitedRange,
                               const InversionPerComponent &inversion,
                               const Channel                channel)
{
  for (int i = 0; i < TEST_FRAME_NR_VALUES; ++i)
  {
    auto expectedPlaneValue = TEST_VALUES_12BIT[i].at(channel);

    auto       channelIndex = ALL_CHANNELS.at(channel);
    const auto bitDepth     = pixelFormat.getBitsPerSample();

    expectedPlaneValue = scaleShiftClipInvertValue(
        expectedPlaneValue, bitDepth, scaling[channelIndex], inversion[channelIndex]);

    if (limitedRange)
      expectedPlaneValue = LimitedRangeToFullRange.at(expectedPlaneValue);

    const auto expectedValue =
        rgba_t({expectedPlaneValue, expectedPlaneValue, expectedPlaneValue, 255});

    const auto actualValue = getARGBValueFromDataLittleEndian(data, i);

    if (expectedValue != actualValue)
      throw std::runtime_error("Value " + std::to_string(i));
  }
}

void testConversionToRGBA(const QByteArray &           sourceBuffer,
                          const PixelFormatRGB &       srcPixelFormat,
                          const InversionPerComponent &inversion,
                          const ScalingPerComponent &  componentScale,
                          const bool                   limitedRange,
                          const bool                   outputHasAlpha)
{
  UChaVector outputBuffer;
  outputBuffer.resize(TEST_FRAME_NR_VALUES * 4);

  convertInputRGBToARGB(sourceBuffer,
                        srcPixelFormat,
                        outputBuffer.data(),
                        TEST_FRAME_SIZE,
                        inversion.data(),
                        componentScale.data(),
                        limitedRange,
                        outputHasAlpha,
                        PremultiplyAlpha(false));

  try
  {
    const auto alphaShouldBeSet = (outputHasAlpha && srcPixelFormat.hasAlpha());
    checkOutputValues(outputBuffer,
                      srcPixelFormat.getBitsPerSample(),
                      componentScale,
                      limitedRange,
                      inversion,
                      alphaShouldBeSet);
  }
  catch (const std::exception &e)
  {
    const auto errorMessage = "Error checking " + std::string(e.what()) + " for " +
                              srcPixelFormat.getName() + " outputHasAlpha " +
                              std::to_string(outputHasAlpha) + " scaling " +
                              to_string(componentScale) + " inversion " + to_string(inversion) +
                              " limitedRange " + std::to_string(limitedRange);
    throw std::exception(errorMessage.c_str());
  }
}

void testConversionToRGBASinglePlane(const QByteArray &           sourceBuffer,
                                     const PixelFormatRGB &       srcPixelFormat,
                                     const InversionPerComponent &inversion,
                                     const ScalingPerComponent &  componentScale,
                                     const bool                   limitedRange,
                                     const bool                   outputHasAlpha)
{
  for (const auto channel : ALL_CHANNELS)
  {
    if (channel.first == Channel::Alpha && !srcPixelFormat.hasAlpha())
      continue;

    UChaVector outputBuffer;
    outputBuffer.resize(TEST_FRAME_NR_VALUES * 4);

    const auto displayComponentOffset = srcPixelFormat.getChannelPosition(channel.first);
    convertSinglePlaneOfRGBToGreyscaleARGB(sourceBuffer,
                                           srcPixelFormat,
                                           outputBuffer.data(),
                                           TEST_FRAME_SIZE,
                                           componentScale[channel.second],
                                           inversion[channel.second],
                                           displayComponentOffset,
                                           limitedRange);

    try
    {
      checkOutputValuesForPlane(
          outputBuffer, srcPixelFormat, componentScale, limitedRange, inversion, channel.first);
    }
    catch (const std::exception &e)
    {
      const auto errorMessage = "Error checking " + std::string(e.what()) + " for " +
                                srcPixelFormat.getName() + " outputHasAlpha " +
                                std::to_string(outputHasAlpha) + " scaling " +
                                to_string(componentScale) + " inversion " + to_string(inversion) +
                                " limitedRange " + std::to_string(limitedRange);
      throw std::exception(errorMessage.c_str());
    }
  }
}

void testGetPixelValueFromBuffer(const QByteArray &    sourceBuffer,
                                 const PixelFormatRGB &srcPixelFormat)
{
  const auto bitDepth = srcPixelFormat.getBitsPerSample();
  const auto shift    = 12 - bitDepth;

  int testValueIndex = 0;
  for (int y : {0, 1, 2, 3})
  {
    for (int x : {0, 1, 2, 3})
    {
      const QPoint pixelPos(x, y);
      const auto   actualValue =
          getPixelValueFromBuffer(sourceBuffer, srcPixelFormat, TEST_FRAME_SIZE, pixelPos);

      const auto testValue     = TEST_VALUES_12BIT[testValueIndex++];
      auto       expectedValue = rgba_t(
          {testValue.R >> shift, testValue.G >> shift, testValue.B >> shift, testValue.A >> shift});
      if (!srcPixelFormat.hasAlpha())
        expectedValue.A = 0;

      if (actualValue != expectedValue)
      {
        const auto errorMessage = "Error checking pixel [" + std::to_string(x) + "," +
                                  std::to_string(y) + " format " + srcPixelFormat.getName();
        QFAIL(errorMessage.c_str());
      }
    }
  }
}

using TestingFunction = std::function<void(const QByteArray &,
                                           const PixelFormatRGB &,
                                           const InversionPerComponent &,
                                           const ScalingPerComponent &,
                                           const bool,
                                           const bool)>;

void runTestForAllParameters(TestingFunction testingFunction)
{
  for (const auto endianness : {Endianness::Little, Endianness::Big})
  {
    for (const auto bitDepth : {8, 10, 12})
    {
      for (const auto alphaMode : AlphaModeMapper.entries())
      {
        for (const auto dataLayout : DataLayoutMapper.entries())
        {
          for (const auto channelOrder : ChannelOrderMapper.entries())
          {
            const PixelFormatRGB format(
                bitDepth, dataLayout.value, channelOrder.value, alphaMode.value, endianness);
            const auto data = createRawRGBData(format);

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
                  for (const auto limitedRange : {false, true})
                  {
                    try
                    {
                      testingFunction(
                          data, format, inversion, componentScale, limitedRange, outputHasAlpha);
                    }
                    catch (const std::exception &e)
                    {
                      QFAIL(e.what());
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

} // namespace

class ConversionRGBTest : public QObject
{
  Q_OBJECT

private slots:
  void testBasicConvertsion();
  void testConversionSingleComponent();
  void testGetPixelValue();
};

void ConversionRGBTest::testBasicConvertsion()
{
  runTestForAllParameters(testConversionToRGBA);
}

void ConversionRGBTest::testConversionSingleComponent()
{
  runTestForAllParameters(testConversionToRGBASinglePlane);
}

void ConversionRGBTest::testGetPixelValue()
{
  for (const auto endianness : {Endianness::Little, Endianness::Big})
  {
    for (auto bitDepth : {8, 10, 12})
    {
      for (const auto alphaMode : AlphaModeMapper.entries())
      {
        for (const auto dataLayout : DataLayoutMapper.entries())
        {
          for (const auto channelOrder : ChannelOrderMapper.entries())
          {
            const PixelFormatRGB format(
                bitDepth, dataLayout.value, channelOrder.value, alphaMode.value, endianness);
            const auto data = createRawRGBData(format);

            testGetPixelValueFromBuffer(data, format);
          }
        }
      }
    }
  }
}

QTEST_MAIN(ConversionRGBTest)

#include "ConversionRGBTest.moc"
