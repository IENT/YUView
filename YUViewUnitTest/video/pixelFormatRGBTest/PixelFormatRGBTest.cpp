#include <QtTest>

#include <video/rgb/PixelFormatRGB.h>

using namespace video;
using namespace video::rgb;

class PixelFormatRGBTest : public QObject
{
  Q_OBJECT

public:
  PixelFormatRGBTest(){};
  ~PixelFormatRGBTest(){};

private slots:
  void testFormatFromToString();
  void testInvalidFormats();
};

QList<PixelFormatRGB> getAllFormats()
{
  QList<PixelFormatRGB> allFormats;

  for (int bitsPerPixel = 8; bitsPerPixel <= 16; bitsPerPixel++)
    for (auto dataLayout : {video::DataLayout::Packed, video::DataLayout::Planar})
      for (auto channelOrder : ChannelOrderMapper.getEnums())
        for (auto alphaMode : {AlphaMode::None, AlphaMode::First, AlphaMode::Last})
          for (auto endianness : {Endianness::Little, Endianness::Big})
            allFormats.append(
                PixelFormatRGB(bitsPerPixel, dataLayout, channelOrder, alphaMode, endianness));

  return allFormats;
}

void PixelFormatRGBTest::testFormatFromToString()
{
  for (auto fmt : getAllFormats())
  {
    QVERIFY(fmt.isValid());
    auto name = fmt.getName();
    if (name.empty())
    {
      QFAIL("Name empty");
    }
    auto fmtNew = PixelFormatRGB(name);
    if (fmt != fmtNew)
    {
      auto errorStr = "Comparison failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.getComponentPosition(Channel::Red) != fmtNew.getComponentPosition(Channel::Red) ||
        fmt.getComponentPosition(Channel::Green) != fmtNew.getComponentPosition(Channel::Green) ||
        fmt.getComponentPosition(Channel::Blue) != fmtNew.getComponentPosition(Channel::Blue) ||
        fmt.getComponentPosition(Channel::Alpha) != fmtNew.getComponentPosition(Channel::Alpha) ||
        fmt.getBitsPerSample() != fmtNew.getBitsPerSample() ||
        fmt.getDataLayout() != fmtNew.getDataLayout())
    {
      auto errorStr = "Comparison of parameters failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.hasAlpha() && (fmt.nrChannels() != 4 || !fmt.hasAlpha()))
    {
      auto errorStr = "Alpha channel indicated wrong - " + name;
      QFAIL(errorStr.c_str());
    }
    if (!fmt.hasAlpha() && (fmt.nrChannels() != 3 || fmt.hasAlpha()))
    {
      auto errorStr = "Alpha channel indicated wrong - %1" + name;
      QFAIL(errorStr.c_str());
    }
  }
}

void PixelFormatRGBTest::testInvalidFormats()
{
  QList<PixelFormatRGB> invalidFormats;
  invalidFormats.append(PixelFormatRGB(0, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(PixelFormatRGB(1, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(PixelFormatRGB(7, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(PixelFormatRGB(17, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(PixelFormatRGB(200, video::DataLayout::Packed, ChannelOrder::RGB));

  for (auto fmt : invalidFormats)
    QVERIFY(!fmt.isValid());
}

QTEST_MAIN(PixelFormatRGBTest)

#include "PixelFormatRGBTest.moc"
