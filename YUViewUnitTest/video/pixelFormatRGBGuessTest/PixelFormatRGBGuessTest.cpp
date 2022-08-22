#include <QtTest>

#include <video/PixelFormatRGB.h>
#include <video/PixelFormatRGBGuess.h>

#include <iostream>

using namespace video;

class PixelFormatRGBGuessTest : public QObject
{
  Q_OBJECT

public:
  PixelFormatRGBGuessTest();
  ~PixelFormatRGBGuessTest();

private slots:
  void testFormatGuessFromFilename_data();
  void testFormatGuessFromFilename();
};

PixelFormatRGBGuessTest::PixelFormatRGBGuessTest()
{
}

PixelFormatRGBGuessTest::~PixelFormatRGBGuessTest()
{
}

void PixelFormatRGBGuessTest::testFormatGuessFromFilename_data()
{
  QTest::addColumn<QString>("filename");
  QTest::addColumn<unsigned>("fileSize");
  QTest::addColumn<unsigned>("width");
  QTest::addColumn<unsigned>("height");
  QTest::addColumn<QString>("expectedFormatName");

  const auto BytesNoAlpha   = 1920u * 1080 * 12 * 3; // 12 frames RGB
  const auto NotEnoughBytes = 22u;
  const auto UnfittingBytes = 1920u * 1080 * 5;

  QTest::newRow("testNoIndicatorInName") << "noIndicatorHere.yuv" << 0u << 0u << 0u << "RGB 8bit";
  QTest::newRow("testResolutionOnly")
      << "something_1920x1080.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";

  // No Alpha
  QTest::newRow("testFormatsChannelOrderRGB")
      << "something_1920x1080_rgb.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsChannelOrderRBG")
      << "something_1920x1080_rbg.yuv" << BytesNoAlpha << 1920u << 1080u << "RBG 8bit";
  QTest::newRow("testFormatsChannelOrderGRB")
      << "something_1920x1080_grb.yuv" << BytesNoAlpha << 1920u << 1080u << "GRB 8bit";
  QTest::newRow("testFormatsChannelOrderGBR")
      << "something_1920x1080_gbr.yuv" << BytesNoAlpha << 1920u << 1080u << "GBR 8bit";
  QTest::newRow("testFormatsChannelOrderBRG")
      << "something_1920x1080_brg.yuv" << BytesNoAlpha << 1920u << 1080u << "BRG 8bit";
  QTest::newRow("testFormatsChannelOrderBGR")
      << "something_1920x1080_bgr.yuv" << BytesNoAlpha << 1920u << 1080u << "BGR 8bit";

  // Alpha First
  QTest::newRow("testFormatsChannelOrderARGB")
      << "something_1920x1080_argb.yuv" << BytesNoAlpha << 1920u << 1080u << "ARGB 8bit";
  QTest::newRow("testFormatsChannelOrderARBG")
      << "something_1920x1080_arbg.yuv" << BytesNoAlpha << 1920u << 1080u << "ARBG 8bit";
  QTest::newRow("testFormatsChannelOrderAGRB")
      << "something_1920x1080_agrb.yuv" << BytesNoAlpha << 1920u << 1080u << "AGRB 8bit";
  QTest::newRow("testFormatsChannelOrderAGBR")
      << "something_1920x1080_agbr.yuv" << BytesNoAlpha << 1920u << 1080u << "AGBR 8bit";
  QTest::newRow("testFormatsChannelOrderABRG")
      << "something_1920x1080_abrg.yuv" << BytesNoAlpha << 1920u << 1080u << "ABRG 8bit";
  QTest::newRow("testFormatsChannelOrderABGR")
      << "something_1920x1080_abgr.yuv" << BytesNoAlpha << 1920u << 1080u << "ABGR 8bit";

  // Alpha Last
  QTest::newRow("testFormatsChannelOrderRGBA")
      << "something_1920x1080_rgba.yuv" << BytesNoAlpha << 1920u << 1080u << "RGBA 8bit";
  QTest::newRow("testFormatsChannelOrderRBGA")
      << "something_1920x1080_rbga.yuv" << BytesNoAlpha << 1920u << 1080u << "RBGA 8bit";
  QTest::newRow("testFormatsChannelOrderGRBA")
      << "something_1920x1080_grba.yuv" << BytesNoAlpha << 1920u << 1080u << "GRBA 8bit";
  QTest::newRow("testFormatsChannelOrderGBRA")
      << "something_1920x1080_gbra.yuv" << BytesNoAlpha << 1920u << 1080u << "GBRA 8bit";
  QTest::newRow("testFormatsChannelOrderBRGA")
      << "something_1920x1080_brga.yuv" << BytesNoAlpha << 1920u << 1080u << "BRGA 8bit";
  QTest::newRow("testFormatsChannelOrderBGRA")
      << "something_1920x1080_bgra.yuv" << BytesNoAlpha << 1920u << 1080u << "BGRA 8bit";

  // Bit dephts
  QTest::newRow("testFormatsBitDepth10")
      << "something_1920x1080_rgb10.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 10bit";
  QTest::newRow("testFormatsBitDepth12")
      << "something_1920x1080_rgb12.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 12bit";
  QTest::newRow("testFormatsBitDepth16")
      << "something_1920x1080_rgb16.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 16bit";
  QTest::newRow("testFormatsBitDepth48")
      << "something_1920x1080_rgb48.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 16bit";
  QTest::newRow("testFormatsBitDepth64")
      << "something_1920x1080_rgb64.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 16bit";
  QTest::newRow("testFormatsBitDepth11ShouldFail")
      << "something_1920x1080_rgb11.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";

  // Endianness
  QTest::newRow("testFormatsEndianness1")
      << "something_1920x1080_rgb8le.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsEndianness2")
      << "something_1920x1080_rgb8be.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsEndianness3")
      << "something_1920x1080_rgb10le.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 10bit";
  QTest::newRow("testFormatsEndianness4")
      << "something_1920x1080_rgb10be.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 10bit BE";
  QTest::newRow("testFormatsEndianness5")
      << "something_1920x1080_rgb16be.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 16bit BE";

  // DataLayout
  QTest::newRow("testFormatsDataLayout1")
      << "something_1920x1080_rgb_packed.yuv" << BytesNoAlpha << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsDataLayout2") << "something_1920x1080_rgb_planar.yuv" << BytesNoAlpha
                                          << 1920u << 1080u << "RGB 8bit planar";
  QTest::newRow("testFormatsDataLayout3") << "something_1920x1080_rgb10le_planar.yuv"
                                          << BytesNoAlpha << 1920u << 1080u << "RGB 10bit planar";
  QTest::newRow("testFormatsDataLayout4")
      << "something_1920x1080_rgb10be_planar.yuv" << BytesNoAlpha << 1920u << 1080u
      << "RGB 10bit planar BE";
  QTest::newRow("testFormatsDataLayout5") << "something_1920x1080_rgb16_planar.yuv" << BytesNoAlpha
                                          << 1920u << 1080u << "RGB 16bit planar";
  QTest::newRow("testFormatsDataLayout6")
      << "something_1920x1080_rgb16be_planar.yuv" << BytesNoAlpha << 1920u << 1080u
      << "RGB 16bit planar BE";

  // File size check
  QTest::newRow("testFormatsFileSizeCheck1")
      << "something_1920x1080_rgb10.yuv" << NotEnoughBytes << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsFileSizeCheck2")
      << "something_1920x1080_rgb16be.yuv" << NotEnoughBytes << 1920u << 1080u << "RGB 8bit";
  QTest::newRow("testFormatsFileSizeCheck3")
      << "something_1920x1080_rgb16be.yuv" << UnfittingBytes << 1920u << 1080u << "RGB 8bit";
}

void PixelFormatRGBGuessTest::testFormatGuessFromFilename()
{
  QFETCH(QString, filename);
  QFETCH(unsigned, fileSize);
  QFETCH(unsigned, width);
  QFETCH(unsigned, height);
  QFETCH(QString, expectedFormatName);

  QFileInfo fileInfo(filename);
  auto      fmt = video::rgb::guessFormatFromSizeAndName(fileInfo, Size(width, height), fileSize);
  auto      fmtName = fmt.getName();

  QVERIFY(fmt.isValid());
  QCOMPARE(fmtName, expectedFormatName.toStdString());
}

QTEST_MAIN(PixelFormatRGBGuessTest)

#include "PixelFormatRGBGuessTest.moc"
