#include <QtTest>

#include <filesource/fileSource.h>

class fileSourceTest : public QObject
{
    Q_OBJECT

public:
    fileSourceTest();
    ~fileSourceTest();

private slots:
    void testFormatFromFilename_data();
    void testFormatFromFilename();

};

fileSourceTest::fileSourceTest()
{
}

fileSourceTest::~fileSourceTest()
{
}

void fileSourceTest::testFormatFromFilename_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");
    QTest::addColumn<int>("framerate");
    QTest::addColumn<int>("bitDepth");

    // Things that should work
    QTest::newRow("testResolutionOnly1") << "something_1920x1080.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionOnly2") << "something_295x289.yuv" << 295 << 289 << -1 << -1;
    QTest::newRow("testResolutionOnly3") << "something_295234x289234.yuv" << 295234 << 289234 << -1 << -1;
    QTest::newRow("testResolutionOnly4") << "something_1920X1080.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionOnly5") << "something_1920*1080.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionOnly6") << "something_1920x1080_something.yuv" << 1920 << 1080 << -1 << -1;
    // Things that should not work
    QTest::newRow("testResolutionOnly7") << "something_1920_1080.yuv" << -1 << -1 << -1 << -1;
    QTest::newRow("testResolutionOnly8") << "something_19201080.yuv" << -1 << -1 << -1 << -1;
    QTest::newRow("testResolutionOnly9") << "something_1920-1080.yuv" << -1 << -1 << -1 << -1;
    QTest::newRow("testResolutionOnly10") << "something_1920-1080_something.yuv" << -1 << -1 << -1 << -1;

    // Things that should work
    QTest::newRow("testResolutionAndFPS1") << "something_1920x1080_25.yuv" << 1920 << 1080 << 25 << -1;
    QTest::newRow("testResolutionAndFPS2") << "something_1920x1080_999.yuv" << 1920 << 1080 << 999 << -1;
    QTest::newRow("testResolutionAndFPS3") << "something_1920x1080_60Hz.yuv" << 1920 << 1080 << 60 << -1;
    QTest::newRow("testResolutionAndFPS4") << "something_1920x1080_999_something.yuv" << 1920 << 1080 << 999 << -1;
    QTest::newRow("testResolutionAndFPS5") << "something_1920x1080_60hz.yuv" << 1920 << 1080 << 60 << -1;
    QTest::newRow("testResolutionAndFPS6") << "something_1920x1080_60HZ.yuv" << 1920 << 1080 << 60 << -1;
    QTest::newRow("testResolutionAndFPS7") << "something_1920x1080_60fps.yuv" << 1920 << 1080 << 60 << -1;
    QTest::newRow("testResolutionAndFPS8") << "something_1920x1080_60FPS.yuv" << 1920 << 1080 << 60 << -1;

    QTest::newRow("testResolutionAndFPSAndBitDepth1") << "something_1920x1080_25_8.yuv" << 1920 << 1080 << 25 << 8;
    QTest::newRow("testResolutionAndFPSAndBitDepth2") << "something_1920x1080_25_12.yuv" << 1920 << 1080 << 25 << 12;
    QTest::newRow("testResolutionAndFPSAndBitDepth3") << "something_1920x1080_25_8b.yuv" << 1920 << 1080 << 25 << 8;
    QTest::newRow("testResolutionAndFPSAndBitDepth4") << "something_1920x1080_25_8b_something.yuv" << 1920 << 1080 << 25 << 8;

    QTest::newRow("testResolutionIndicator1") << "something1080p.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionIndicator2") << "something1080pSomething.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionIndicator3") << "something1080p33.yuv" << 1920 << 1080 << 33 << -1;
    QTest::newRow("testResolutionIndicator4") << "something1080p33Something.yuv" << 1920 << 1080 << 33 << -1;
    QTest::newRow("testResolutionIndicator5") << "something720p.yuv" << 1280 << 720 << -1 << -1;
    QTest::newRow("testResolutionIndicator6") << "something720pSomething.yuv" << 1280 << 720 << -1 << -1;
    QTest::newRow("testResolutionIndicator7") << "something720p44.yuv" << 1280 << 720 << 44 << -1;
    QTest::newRow("testResolutionIndicator8") << "something720p44Something.yuv" << 1280 << 720 << 44 << -1;

    QTest::newRow("testResolutionKeyword1") << "something_cif.yuv" << 352 << 288 << -1 << -1;
    QTest::newRow("testResolutionKeyword2") << "something_cifSomething.yuv" << 352 << 288 << -1 << -1;
    QTest::newRow("testResolutionKeyword3") << "something_qcif.yuv" << 176 << 144 << -1 << -1;
    QTest::newRow("testResolutionKeyword4") << "something_qcifSomething.yuv" << 176 << 144 << -1 << -1;
    QTest::newRow("testResolutionKeyword5") << "something_4cif.yuv" << 704 << 576 << -1 << -1;
    QTest::newRow("testResolutionKeyword6") << "something_4cifSomething.yuv" << 704 << 576 << -1 << -1;
    QTest::newRow("testResolutionKeyword7") << "somethingUHDSomething.yuv" << 3840 << 2160 << -1 << -1;
    QTest::newRow("testResolutionKeyword8") << "somethingHDSomething.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionKeyword9") << "something1080pSomething.yuv" << 1920 << 1080 << -1 << -1;
    QTest::newRow("testResolutionKeyword10") << "something720pSomething.yuv" << 1280 << 720 << -1 << -1;

    QTest::newRow("testBitDepthIndicator1") << "something_1920x1080_8Bit.yuv" << 1920 << 1080 << -1 << 8;
    QTest::newRow("testBitDepthIndicator2") << "something_1920x1080_10Bit.yuv" << 1920 << 1080 << -1 << 10;
    QTest::newRow("testBitDepthIndicator3") << "something_1920x1080_12Bit.yuv" << 1920 << 1080 << -1 << 12;
    QTest::newRow("testBitDepthIndicator4") << "something_1920x1080_16Bit.yuv" << 1920 << 1080 << -1 << 16;
    QTest::newRow("testBitDepthIndicator5") << "something_1920x1080_8bit.yuv" << 1920 << 1080 << -1 << 8;
    QTest::newRow("testBitDepthIndicator6") << "something_1920x1080_8BIT.yuv" << 1920 << 1080 << -1 << 8;
    QTest::newRow("testBitDepthIndicator7") << "something_1920x1080_8-Bit.yuv" << 1920 << 1080 << -1 << 8;
    QTest::newRow("testBitDepthIndicator8") << "something_1920x1080_8-BIT.yuv" << 1920 << 1080 << -1 << 8;
}

void fileSourceTest::testFormatFromFilename()
{
    QFETCH(QString, filename);
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(int, framerate);
    QFETCH(int, bitDepth);

    QFileInfo fileInfo(filename);
    auto fileFormat = fileSource::formatFromFilename(fileInfo);

    QCOMPARE(fileFormat.frameSize.width(), width);
    QCOMPARE(fileFormat.frameSize.height(), height);
    QCOMPARE(fileFormat.frameRate, framerate);
    QCOMPARE(fileFormat.bitDepth, bitDepth);
}

QTEST_MAIN(fileSourceTest)

#include "tst_filesource.moc"
