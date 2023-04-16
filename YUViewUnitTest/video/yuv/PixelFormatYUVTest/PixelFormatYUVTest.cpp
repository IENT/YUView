#include <QtTest>

#include <video/yuv/PixelFormatYUV.h>

using namespace video::yuv;

class PixelFormatYUVTest : public QObject
{
  Q_OBJECT

public:
  PixelFormatYUVTest(){};
  ~PixelFormatYUVTest(){};

private slots:
  void testFormatFromToString();
};

std::vector<PixelFormatYUV> getAllFormats()
{
  std::vector<PixelFormatYUV> allFormats;

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
          auto pixelFormat = PixelFormatYUV(subsampling, bitsPerSample, planeOrder, bigEndian);
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
            auto pixelFormat =
                PixelFormatYUV(subsampling, bitsPerSample, packingOrder, bytePacking, bigEndian);
            allFormats.push_back(pixelFormat);
          }
        }
      }
    }
  }

  for (auto predefinedFormat : PredefinedPixelFormatMapper.getEnums())
    allFormats.push_back(PixelFormatYUV(predefinedFormat));

  return allFormats;
}

void PixelFormatYUVTest::testFormatFromToString()
{
  for (auto fmt : getAllFormats())
  {
    QVERIFY(fmt.isValid());
    auto name = fmt.getName();
    if (name.empty())
    {
      QFAIL("Name empty");
    }
    auto fmtNew = PixelFormatYUV(name);
    if (fmt != fmtNew)
    {
      auto errorStr = "Comparison failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.getSubsampling() != fmtNew.getSubsampling() ||
        fmt.getBitsPerSample() != fmtNew.getBitsPerSample() ||
        fmt.isPlanar() != fmtNew.isPlanar() || fmt.getChromaOffset() != fmtNew.getChromaOffset() ||
        fmt.getPlaneOrder() != fmtNew.getPlaneOrder() ||
        fmt.getChromaPacking() != ChromaPacking::Planar ||
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

QTEST_MAIN(PixelFormatYUVTest)

#include "PixelFormatYUVTest.moc"
