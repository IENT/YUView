#include <QtTest>

#include <video/rgbPixelFormat.h>

using namespace video::rgb;

class rgbPixelFormatTest : public QObject
{
  Q_OBJECT

public:
  rgbPixelFormatTest(){};
  ~rgbPixelFormatTest(){};

private slots:
  void testFormatFromToString();
  void testInvalidFormats();
};

QList<rgbPixelFormat> getAllFormats()
{
  QList<rgbPixelFormat> allFormats;

  for (int bitsPerPixel = 8; bitsPerPixel <= 16; bitsPerPixel++)
  {
    for (auto dataLayout : {video::DataLayout::Packed, video::DataLayout::Planar})
    {
      // No alpha
      for (auto channelOrder : ChannelOrderMapper.getEnums())
        allFormats.append(rgbPixelFormat(bitsPerPixel, dataLayout, channelOrder));
      // With alpha
      for (auto channelOrder : ChannelOrderMapper.getEnums())
      {
        allFormats.append(rgbPixelFormat(bitsPerPixel, dataLayout, channelOrder, AlphaMode::First));
        allFormats.append(rgbPixelFormat(bitsPerPixel, dataLayout, channelOrder, AlphaMode::Last));
      }
    }
  }

  return allFormats;
}

void rgbPixelFormatTest::testFormatFromToString()
{
  for (auto fmt : getAllFormats())
  {
    QVERIFY(fmt.isValid());
    auto name = fmt.getName();
    if (name.empty())
    {
      QFAIL("Name empty");
    }
    auto fmtNew = rgbPixelFormat(name);
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

void rgbPixelFormatTest::testInvalidFormats()
{
  QList<rgbPixelFormat> invalidFormats;
  invalidFormats.append(rgbPixelFormat(0, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(rgbPixelFormat(1, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(rgbPixelFormat(7, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(rgbPixelFormat(17, video::DataLayout::Packed, ChannelOrder::RGB));
  invalidFormats.append(rgbPixelFormat(200, video::DataLayout::Packed, ChannelOrder::RGB));

  for (auto fmt : invalidFormats)
    QVERIFY(!fmt.isValid());
}

QTEST_MAIN(rgbPixelFormatTest)

#include "rgbPixelFormatTest.moc"
