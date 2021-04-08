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

std::vector<YUV_Internals::YUVPixelFormat> getAllFormats()
{
  std::vector<YUV_Internals::YUVPixelFormat> allFormats;

  for (auto subsampling : YUV_Internals::SubsamplingMapper.getEnums())
  {
    for (auto bitsPerSample : YUV_Internals::BitDepthList)
    {
      auto endianList =
          (bitsPerSample > 8) ? std::vector<bool>({false, true}) : std::vector<bool>({false});

      // Planar
      for (auto planeOrder : YUV_Internals::PlaneOrderMapper.getEnums())
      {
        for (auto bigEndian : endianList)
        {
          auto pixelFormat =
              YUV_Internals::YUVPixelFormat(subsampling, bitsPerSample, planeOrder, bigEndian);
          allFormats.push_back(pixelFormat);
        }
      }
      // Packet
      for (auto packingOrder : YUV_Internals::getSupportedPackingFormats(subsampling))
      {
        for (auto bytePacking : {false, true})
        {
          for (auto bigEndian : endianList)
          {
            auto pixelFormat = YUV_Internals::YUVPixelFormat(
                subsampling, bitsPerSample, packingOrder, bytePacking, bigEndian);
            allFormats.push_back(pixelFormat);
          }
        }
      }
    }
  }

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
    auto fmtNew = YUV_Internals::YUVPixelFormat(name);
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
