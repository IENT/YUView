#include <QtTest>

#include <video/YUVPixelFormatGuess.h>

class YUVPixelFormatGuessTest : public QObject
{
  Q_OBJECT

public:
  YUVPixelFormatGuessTest();
  ~YUVPixelFormatGuessTest();

private slots:
  void testFormatGuessFromFilename_data();
  void testFormatGuessFromFilename();
};

YUVPixelFormatGuessTest::YUVPixelFormatGuessTest() {}

YUVPixelFormatGuessTest::~YUVPixelFormatGuessTest() {}

void YUVPixelFormatGuessTest::testFormatGuessFromFilename_data()
{
  QTest::addColumn<QString>("filename");
  QTest::addColumn<unsigned>("fileSize");
  QTest::addColumn<unsigned>("width");
  QTest::addColumn<unsigned>("height");
  QTest::addColumn<unsigned>("bitDepth");
  QTest::addColumn<bool>("packed");
  QTest::addColumn<QString>("expectedFormatName");

  const unsigned bytes1080 = 1920 * 1080 * 3 * 6; // 12 frames 420
  const unsigned bytes1280 = 1280 * 720 * 3 * 6;  // 6 frames 444

  QTest::newRow("testResolutionOnly1") << "something_1920x1080.yuv" << bytes1080 << 1920u << 1080u
                                       << 0u << false << "YUV 4:2:0 8-bit";

  QTest::newRow("testResolutionAndFPSAndBitDepth1")
      << "something_1920x1080_25_8.yuv" << bytes1080 << 1920u << 1080u << 8u << false
      << "YUV 4:2:0 8-bit";
  QTest::newRow("testResolutionAndFPSAndBitDepth2")
      << "something_1920x1080_25_12.yuv" << bytes1080 << 1920u << 1080u << 12u << false
      << "YUV 4:2:0 12-bit LE";
  QTest::newRow("testResolutionAndFPSAndBitDepth3")
      << "something_1920x1080_25_16b.yuv" << bytes1080 << 1920u << 1080u << 16u << false
      << "YUV 4:2:0 16-bit LE";
  QTest::newRow("testResolutionAndFPSAndBitDepth4")
      << "something_1920x1080_25_10b_something.yuv" << bytes1080 << 1920u << 1080u << 10u << false
      << "YUV 4:2:0 10-bit LE";

  // Issue 211
  QTest::newRow("testIssue211_1") << "sample_1280x720_16bit_444_packed_20200109_114812.yuv"
                                  << bytes1280 << 1280u << 720u << 16u << true
                                  << "YUV 4:4:4 16-bit LE packed";
  QTest::newRow("testIssue211_2") << "sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv"
                                  << bytes1280 << 1280u << 720u << 16u << true
                                  << "YUV 4:4:4 16-bit LE packed";
  QTest::newRow("testIssue211_3") << "sample_1280x720_16b_yuv16le_packed_444_20200109_114812"
                                  << bytes1280 << 1280u << 720u << 16u << true
                                  << "YUV 4:4:4 16-bit LE packed";

  // Issue 221
  QTest::newRow("testIssue221_1") << "sample_1280x720_yuv420pUVI_114812.yuv" << bytes1280 << 1280u
                                  << 720u << 16u << false << "YUV(IL) 4:2:0 8-bit";
  QTest::newRow("testIssue221_2") << "sample_1280x720_yuv420pinterlaced_114812.yuv" << bytes1280
                                  << 1280u << 720u << 16u << false << "YUV(IL) 4:2:0 8-bit";
  QTest::newRow("testIssue221_3") << "sample_1280x720_yuv444p16leUVI_114812.yuv" << bytes1280
                                  << 1280u << 720u << 16u << false << "YUV(IL) 4:4:4 16-bit LE";
  QTest::newRow("testIssue221_4") << "sample_1280x720_yuv444p16leinterlaced_114812.yuv" << bytes1280
                                  << 1280u << 720u << 16u << false << "YUV(IL) 4:4:4 16-bit LE";
  // Things that should not work
  QTest::newRow("testIssue221_5") << "sample_1280x720_yuv420pUVVI_114812.yuv" << bytes1280 << 1280u
                                  << 720u << 8u << false << "YUV 4:2:0 8-bit";
  QTest::newRow("testIssue221_6") << "sample_1280x720_yuv420pinnterlaced_114812.yuv" << bytes1280
                                  << 1280u << 720u << 8u << false << "YUV 4:2:0 8-bit";
  QTest::newRow("testIssue221_7") << "sample_1280x720_yuv444p16leUVVI_114812.yuv" << bytes1280
                                  << 1280u << 720u << 16u << false << "YUV 4:4:4 16-bit LE";
  QTest::newRow("testIssue221_8") << "sample_1280x720_yuv444p16leinnterlaced_114812.yuv"
                                  << bytes1280 << 1280u << 720u << 16u << false
                                  << "YUV 4:4:4 16-bit LE";

  // V210 format (w must be multiple of 48)
  auto bytes1280V210 = 1296u * 720 / 6 * 16 * 3; // 3 frames
  QTest::newRow("testIssueV210_1")
      << "sample_1280x720_v210.yuv" << bytes1280V210 << 1280u << 720u << 10u << true << "V210";
  QTest::newRow("testIssueV210_2") << "sample_1280x720_v210.something.yuv" << bytes1280V210 << 1280u
                                   << 720u << 10u << true << "V210";

  QTest::newRow("testIssue310_1")
      << "sample_1920x1080_YUV400p16LE.yuv" << (1920u * 1080u * 2u) << 1920u << 1080u << 16u << false << "YUV 4:0:0 16-bit LE";
  QTest::newRow("testIssue310_2")
      << "sample_1920x1080_gray8le.yuv" << (1920u * 1080u) << 1920u << 1080u << 0u << false << "YUV 4:0:0 8-bit";
  QTest::newRow("testIssue310_3")
      << "sample_1920x1080_gray10le.yuv" << (1920u * 1080u * 2u) << 1920u << 1080u << 0u << false << "YUV 4:0:0 10-bit LE";
  QTest::newRow("testIssue310_4")
      << "sample_1920x1080_gray16le.yuv" << (1920u * 1080u * 2u) << 1920u << 1080u << 0u << false << "YUV 4:0:0 16-bit LE";

  // We need more tests here ... but I don't want to do this now :)
}

void YUVPixelFormatGuessTest::testFormatGuessFromFilename()
{
  QFETCH(QString, filename);
  QFETCH(unsigned, fileSize);
  QFETCH(unsigned, width);
  QFETCH(unsigned, height);
  QFETCH(unsigned, bitDepth);
  QFETCH(bool, packed);
  QFETCH(QString, expectedFormatName);

  QFileInfo fileInfo(filename);
  auto      fmt = YUV_Internals::guessFormatFromSizeAndName(
      Size(width, height), bitDepth, packed, fileSize, fileInfo);
  auto fmtName = fmt.getName();

  QVERIFY(fmt.isValid());
  QCOMPARE(fmtName, expectedFormatName.toStdString());
}

QTEST_MAIN(YUVPixelFormatGuessTest)

#include "YUVPixelFormatGuessTest.moc"
