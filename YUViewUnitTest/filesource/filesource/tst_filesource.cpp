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

    QTest::newRow("test1") << "something_1920x1080.yuv" << 1920 << 1080 << -1 << -1;
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
