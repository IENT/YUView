#include <QtTest>

#include <video/YUVPixelFormat.h>

class YUVPixelFormatTest : public QObject
{
  Q_OBJECT

public:
  YUVPixelFormatTest(){};
  ~YUVPixelFormatTest(){};

private slots:
  void testFormatFromToString();
};

using namespace YUV_Internals;

std::vector<YUVPixelFormat> getAllFormats()
{
  std::vector<YUVPixelFormat> allFormats;

  for (auto subsampling : SubsamplingMapper.getEnums())
  {
    for (auto bitsPerSample : BitDepthList)
    {
      auto endianList =
          (bitsPerSample > 8) ? std::vector<bool>({false, true}) : std::vector<bool>({false});

      // Planar
      for (auto planeOrder : PlaneOrderMapper.getEnums())
      {
        for (auto bigEndian : endianList)
        {
          auto pixelFormat =
              YUVPixelFormat(subsampling, bitsPerSample, planeOrder, bigEndian);
          allFormats.push_back(pixelFormat);
        }
      }
      // Packet
      for (auto packingOrder : getSupportedPackingFormats(subsampling))
      {
        for (auto bytePacking : {false, true})
        {
          for (auto bigEndian : endianList)
          {
            auto pixelFormat = YUVPixelFormat(
                subsampling, bitsPerSample, packingOrder, bytePacking, bigEndian);
            allFormats.push_back(pixelFormat);
          }
        }
      }
    }
  }

  for (auto predefinedFormat : PredefinedPixelFormatMapper.getEnums())
    allFormats.push_back(YUVPixelFormat(predefinedFormat));

  return allFormats;
}

void YUVPixelFormatTest::testFormatFromToString()
{
  for (auto fmt : getAllFormats())
  {
    QVERIFY(fmt.isValid());
    auto name = fmt.getName();
    if (name.empty())
    {
      QFAIL("Name empty");
    }
    auto fmtNew = YUVPixelFormat(name);
    if (fmt != fmtNew)
    {
      auto errorStr = "Comparison failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.getSubsampling() != fmtNew.getSubsampling() ||
        fmt.getBitsPerSample() != fmtNew.getBitsPerSample() ||
        fmt.isPlanar() != fmtNew.isPlanar() || fmt.getChromaOffset() != fmtNew.getChromaOffset() ||
        fmt.getPlaneOrder() != fmtNew.getPlaneOrder() ||
        fmt.isUVInterleaved() != fmtNew.isUVInterleaved() ||
        fmt.getPackingOrder() != fmtNew.getPackingOrder() ||
        fmt.isBytePacking() != fmtNew.isBytePacking())
    {
      auto errorStr = "Comparison of parameters failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.getBitsPerSample() > 8 && fmt.isBigEndian() != fmtNew.isBigEndian())
    {
      auto errorStr = "Comparison of parameters failed. Endianness wrong. Names: " + name;
      QFAIL(errorStr.c_str());
    }
  }
}

QTEST_MAIN(YUVPixelFormatTest)

#include "YUVPixelFormatTest.moc"
