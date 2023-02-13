#include <QtTest>

#include <filesource/FileSource.h>

class FileSourceTest : public QObject
{
  Q_OBJECT

public:
  FileSourceTest();
  ~FileSourceTest();

private slots:
  void testFormatFromFilename_data();
  void testFormatFromFilename();
};

FileSourceTest::FileSourceTest()
{
}

FileSourceTest::~FileSourceTest()
{
}

void FileSourceTest::testFormatFromFilename_data()
{
  QTest::addColumn<QString>("filename");
  QTest::addColumn<unsigned>("width");
  QTest::addColumn<unsigned>("height");
  QTest::addColumn<int>("framerate");
  QTest::addColumn<unsigned>("bitDepth");
  QTest::addColumn<bool>("packed");

  // Things that should work
  QTest::newRow("testResolutionOnly1")
      << "something_1920x1080.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly2")
      << "something_295x289.yuv" << 295u << 289u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly3")
      << "something_295234x289234.yuv" << 295234u << 289234u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly4")
      << "something_1920X1080.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly5")
      << "something_1920*1080.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly6")
      << "something_1920x1080_something.yuv" << 1920u << 1080u << -1 << 0u << false;
  // Things that should not work
  QTest::newRow("testResolutionOnly7")
      << "something_1920_1080.yuv" << 0u << 0u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly8") << "something_19201080.yuv" << 0u << 0u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly9")
      << "something_1920-1080.yuv" << 0u << 0u << -1 << 0u << false;
  QTest::newRow("testResolutionOnly10")
      << "something_1920-1080_something.yuv" << 0u << 0u << -1 << 0u << false;

  // Things that should work
  QTest::newRow("testResolutionAndFPS1")
      << "something_1920x1080_25.yuv" << 1920u << 1080u << 25 << 0u << false;
  QTest::newRow("testResolutionAndFPS2")
      << "something_1920x1080_999.yuv" << 1920u << 1080u << 999 << 0u << false;
  QTest::newRow("testResolutionAndFPS3")
      << "something_1920x1080_60Hz.yuv" << 1920u << 1080u << 60 << 0u << false;
  QTest::newRow("testResolutionAndFPS4")
      << "something_1920x1080_999_something.yuv" << 1920u << 1080u << 999 << 0u << false;
  QTest::newRow("testResolutionAndFPS5")
      << "something_1920x1080_60hz.yuv" << 1920u << 1080u << 60 << 0u << false;
  QTest::newRow("testResolutionAndFPS6")
      << "something_1920x1080_60HZ.yuv" << 1920u << 1080u << 60 << 0u << false;
  QTest::newRow("testResolutionAndFPS7")
      << "something_1920x1080_60fps.yuv" << 1920u << 1080u << 60 << 0u << false;
  QTest::newRow("testResolutionAndFPS8")
      << "something_1920x1080_60FPS.yuv" << 1920u << 1080u << 60 << 0u << false;

  QTest::newRow("testResolutionAndFPSAndBitDepth1")
      << "something_1920x1080_25_8.yuv" << 1920u << 1080u << 25 << 8u << false;
  QTest::newRow("testResolutionAndFPSAndBitDepth2")
      << "something_1920x1080_25_12.yuv" << 1920u << 1080u << 25 << 12u << false;
  QTest::newRow("testResolutionAndFPSAndBitDepth3")
      << "something_1920x1080_25_8b.yuv" << 1920u << 1080u << 25 << 8u << false;
  QTest::newRow("testResolutionAndFPSAndBitDepth4")
      << "something_1920x1080_25_8b_something.yuv" << 1920u << 1080u << 25 << 8u << false;

  QTest::newRow("testResolutionIndicator1")
      << "something1080p.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionIndicator2")
      << "something1080pSomething.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionIndicator3")
      << "something1080p33.yuv" << 1920u << 1080u << 33 << 0u << false;
  QTest::newRow("testResolutionIndicator4")
      << "something1080p33Something.yuv" << 1920u << 1080u << 33 << 0u << false;
  QTest::newRow("testResolutionIndicator5")
      << "something720p.yuv" << 1280u << 720u << -1 << 0u << false;
  QTest::newRow("testResolutionIndicator6")
      << "something720pSomething.yuv" << 1280u << 720u << -1 << 0u << false;
  QTest::newRow("testResolutionIndicator7")
      << "something720p44.yuv" << 1280u << 720u << 44 << 0u << false;
  QTest::newRow("testResolutionIndicator8")
      << "something720p44Something.yuv" << 1280u << 720u << 44 << 0u << false;

  QTest::newRow("testResolutionKeyword1")
      << "something_cif.yuv" << 352u << 288u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword2")
      << "something_cifSomething.yuv" << 352u << 288u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword3")
      << "something_qcif.yuv" << 176u << 144u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword4")
      << "something_qcifSomething.yuv" << 176u << 144u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword5")
      << "something_4cif.yuv" << 704u << 576u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword6")
      << "something_4cifSomething.yuv" << 704u << 576u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword7")
      << "somethingUHDSomething.yuv" << 3840u << 2160u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword8")
      << "somethingHDSomething.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword9")
      << "something1080pSomething.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testResolutionKeyword10")
      << "something720pSomething.yuv" << 1280u << 720u << -1 << 0u << false;

  QTest::newRow("testBitDepthIndicator1")
      << "something_1920x1080_8Bit.yuv" << 1920u << 1080u << -1 << 8u << false;
  QTest::newRow("testBitDepthIndicator2")
      << "something_1920x1080_10Bit.yuv" << 1920u << 1080u << -1 << 10u << false;
  QTest::newRow("testBitDepthIndicator3")
      << "something_1920x1080_12Bit.yuv" << 1920u << 1080u << -1 << 12u << false;
  QTest::newRow("testBitDepthIndicator4")
      << "something_1920x1080_16Bit.yuv" << 1920u << 1080u << -1 << 16u << false;
  QTest::newRow("testBitDepthIndicator5")
      << "something_1920x1080_8bit.yuv" << 1920u << 1080u << -1 << 8u << false;
  QTest::newRow("testBitDepthIndicator6")
      << "something_1920x1080_8BIT.yuv" << 1920u << 1080u << -1 << 8u << false;
  QTest::newRow("testBitDepthIndicator7")
      << "something_1920x1080_8-Bit.yuv" << 1920u << 1080u << -1 << 8u << false;
  QTest::newRow("testBitDepthIndicator8")
      << "something_1920x1080_8-BIT.yuv" << 1920u << 1080u << -1 << 8u << false;

  QTest::newRow("testPackedIndicator1")
      << "something_1920x1080_packed.yuv" << 1920u << 1080u << -1 << 0u << true;
  QTest::newRow("testPackedIndicator2")
      << "something_1920x1080_packed-something.yuv" << 1920u << 1080u << -1 << 0u << true;
  QTest::newRow("testPackedIndicator3")
      << "something_1920x1080packed.yuv" << 1920u << 1080u << -1 << 0u << false;
  QTest::newRow("testPackedIndicator4")
      << "packed_something_1920x1080.yuv" << 1920u << 1080u << -1 << 0u << false;

  // Issue 211
  QTest::newRow("testIssue211") << "sample_1280x720_16bit_444_packed_20200109_114812.yuv" << 1280u
                                << 720u << -1 << 16u << true;
  QTest::newRow("testIssue211") << "sample_1280x720_16b_yuv44416le_packed_20200109_114812.yuv"
                                << 1280u << 720u << -1 << 16u << true;
  QTest::newRow("testIssue211") << "sample_1280x720_16b_yuv16le_packed_444_20200109_114812" << 1280u
                                << 720u << -1 << 16u << true;
}

void FileSourceTest::testFormatFromFilename()
{
  QFETCH(QString, filename);
  QFETCH(unsigned, width);
  QFETCH(unsigned, height);
  QFETCH(int, framerate);
  QFETCH(unsigned, bitDepth);
  QFETCH(bool, packed);

  FileSource file;
  file.openFile(filename);
  auto fileFormat = file.guessFormatFromFilename();

  QCOMPARE(fileFormat.frameSize.width, width);
  QCOMPARE(fileFormat.frameSize.height, height);
  QCOMPARE(fileFormat.frameRate, framerate);
  QCOMPARE(fileFormat.bitDepth, bitDepth);
  QCOMPARE(fileFormat.packed, packed);
}

QTEST_MAIN(FileSourceTest)

#include "FilesourceTest.moc"
