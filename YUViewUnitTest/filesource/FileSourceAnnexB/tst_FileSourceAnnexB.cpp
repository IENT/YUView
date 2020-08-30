#include <QtTest>

#include <filesource/FileSourceAnnexBFile.h>
#include <QTemporaryFile>

#include <optional>

class FileSourceAnnexBTest : public QObject
{
  Q_OBJECT

public:
  FileSourceAnnexBTest();
  ~FileSourceAnnexBTest();

private slots:
  void testFormatFromFilename_data();
  void testFormatFromFilename();
};

FileSourceAnnexBTest::FileSourceAnnexBTest()
{
}

FileSourceAnnexBTest::~FileSourceAnnexBTest()
{
}

void FileSourceAnnexBTest::testFormatFromFilename_data()
{
  QTest::addColumn<size_t>("startCodeLength");
  QTest::addColumn<size_t>("totalDataLength");
  QTest::addColumn<QList<size_t>>("startCodePositions");

  QTest::newRow("testNormalPosition1") << size_t(3) << size_t(10000) << QList<size_t>({80, 208, 500});
  QTest::newRow("testNormalPosition2") << size_t(3) << size_t(10000) << QList<size_t>({0, 80, 208, 500});
  QTest::newRow("testNormalPosition3") << size_t(3) << size_t(10000) << QList<size_t>({1, 80, 208, 500});
  QTest::newRow("testNormalPosition4") << size_t(3) << size_t(10000) << QList<size_t>({2, 80, 208, 500});
  QTest::newRow("testNormalPosition5") << size_t(3) << size_t(10000) << QList<size_t>({3, 80, 208, 500});
  QTest::newRow("testNormalPosition6") << size_t(3) << size_t(10000) << QList<size_t>({4, 80, 208, 500});
  QTest::newRow("testNormalPosition7") << size_t(3) << size_t(10000) << QList<size_t>({4, 80, 208, 9990});
  QTest::newRow("testNormalPosition8") << size_t(3) << size_t(10000) << QList<size_t>({4, 80, 208, 9997});

  // Test cases where a buffer reload is needed (the buffer is 500k)
  QTest::newRow("testBufferReload") << size_t(3) << size_t(1000000) << QList<size_t>({80, 208, 500, 50000, 800000});

  // The buffer is 500k in size. Test all variations with a start code around this position
  QTest::newRow("testBufferEdge1") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 499997});
  QTest::newRow("testBufferEdge2") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 499998});
  QTest::newRow("testBufferEdge3") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 499999});
  QTest::newRow("testBufferEdge4") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 500000});
  QTest::newRow("testBufferEdge5") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 500001});
  QTest::newRow("testBufferEdge6") << size_t(3) << size_t(800000) << QList<size_t>({80, 208, 500, 50000, 500002});

  QTest::newRow("testBufferEnd1") << size_t(3) << size_t(10000) << QList<size_t>({80, 208, 500, 9995});
  QTest::newRow("testBufferEnd2") << size_t(3) << size_t(10000) << QList<size_t>({80, 208, 500, 9996});
  QTest::newRow("testBufferEnd3") << size_t(3) << size_t(10000) << QList<size_t>({80, 208, 500, 9997});
}

void FileSourceAnnexBTest::testFormatFromFilename()
{
  QFETCH(size_t, startCodeLength);
  QFETCH(size_t, totalDataLength);
  QFETCH(QList<size_t>, startCodePositions);

  QVERIFY(startCodeLength == 3 || startCodeLength == 4);

  QByteArray data;
  std::optional<size_t> lastStartPos;
  QList<size_t> nalSizes;
  for (const auto pos : startCodePositions)
  {
    size_t nonStartCodeBytesToAdd;
    if (lastStartPos)
    {
      QVERIFY(pos > *lastStartPos + startCodeLength);  // Start codes can not be closer together
      nonStartCodeBytesToAdd = pos - *lastStartPos - startCodeLength;
      nalSizes.append(nonStartCodeBytesToAdd + startCodeLength);
    }
    else
      nonStartCodeBytesToAdd = pos;
    lastStartPos = pos;

    data.append(int(nonStartCodeBytesToAdd), char(128));
    
    // Append start code
    if (startCodeLength == 4)
      data.append(char(0));
    data.append(char(0));
    data.append(char(0));
    data.append(char(1));
  }
  const auto remainder = totalDataLength - *lastStartPos - startCodeLength;
  nalSizes.append(remainder + startCodeLength);
  data.append(int(remainder), char(128));

  // Write the data to file
  QTemporaryFile f;
  f.open();
  f.write(data);
  f.close();

  FileSourceAnnexBFile annexBFile(f.fileName());
  QCOMPARE(annexBFile.getNrBytesBeforeFirstNAL(), startCodePositions[0]);

  auto nalData = annexBFile.getNextNALUnit();
  unsigned counter = 0;
  while (nalData.size() > 0)
  {
    QCOMPARE(nalSizes[counter++], nalData.size());
    nalData = annexBFile.getNextNALUnit();
  }
}

QTEST_MAIN(FileSourceAnnexBTest)

#include "tst_FilesourceAnnexB.moc"
