#include <QTemporaryFile>
#include <QtTest>


#include <filesource/FileSourceAnnexBFile.h>

#include <optional>

class FileSourceAnnexBTest : public QObject
{
  Q_OBJECT

public:
  FileSourceAnnexBTest();
  ~FileSourceAnnexBTest();

private slots:
  void testNalUnitParsing_data();
  void testNalUnitParsing();
};

FileSourceAnnexBTest::FileSourceAnnexBTest()
{
}

FileSourceAnnexBTest::~FileSourceAnnexBTest()
{
}

void FileSourceAnnexBTest::testNalUnitParsing_data()
{
  QTest::addColumn<unsigned>("startCodeLength");
  QTest::addColumn<unsigned>("totalDataLength");
  QTest::addColumn<QList<unsigned>>("startCodePositions");

  QTest::newRow("testNormalPosition1") << 3u << 10000u << QList<unsigned>({80, 208, 500});
  QTest::newRow("testNormalPosition2") << 3u << 10000u << QList<unsigned>({0, 80, 208, 500});
  QTest::newRow("testNormalPosition3") << 3u << 10000u << QList<unsigned>({1, 80, 208, 500});
  QTest::newRow("testNormalPosition4") << 3u << 10000u << QList<unsigned>({2, 80, 208, 500});
  QTest::newRow("testNormalPosition5") << 3u << 10000u << QList<unsigned>({3, 80, 208, 500});
  QTest::newRow("testNormalPosition6") << 3u << 10000u << QList<unsigned>({4, 80, 208, 500});
  QTest::newRow("testNormalPosition7") << 3u << 10000u << QList<unsigned>({4, 80, 208, 9990});
  QTest::newRow("testNormalPosition8") << 3u << 10000u << QList<unsigned>({4, 80, 208, 9997});

  // Test cases where a buffer reload is needed (the buffer is 500k)
  QTest::newRow("testBufferReload")
      << unsigned(3) << unsigned(1000000) << QList<unsigned>({80, 208, 500, 50000, 800000});

  // The buffer is 500k in size. Test all variations with a start code around this position
  QTest::newRow("testBufferEdge1")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 499997});
  QTest::newRow("testBufferEdge2")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 499998});
  QTest::newRow("testBufferEdge3")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 499999});
  QTest::newRow("testBufferEdge4")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 500000});
  QTest::newRow("testBufferEdge5")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 500001});
  QTest::newRow("testBufferEdge6")
      << 3u << 800000u << QList<unsigned>({80, 208, 500, 50000, 500002});

  QTest::newRow("testBufferEnd1") << 3u << 10000u << QList<unsigned>({80, 208, 500, 9995});
  QTest::newRow("testBufferEnd2") << 3u << 10000u << QList<unsigned>({80, 208, 500, 9996});
  QTest::newRow("testBufferEnd3") << 3u << 10000u << QList<unsigned>({80, 208, 500, 9997});
}

void FileSourceAnnexBTest::testNalUnitParsing()
{
  QFETCH(unsigned, startCodeLength);
  QFETCH(unsigned, totalDataLength);
  QFETCH(QList<unsigned>, startCodePositions);

  QVERIFY(startCodeLength == 3 || startCodeLength == 4);

  QByteArray              data;
  std::optional<unsigned> lastStartPos;
  QList<unsigned>         nalSizes;
  for (const auto pos : startCodePositions)
  {
    unsigned nonStartCodeBytesToAdd;
    if (lastStartPos)
    {
      QVERIFY(pos > *lastStartPos + startCodeLength); // Start codes can not be closer together
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
  QCOMPARE(unsigned(annexBFile.getNrBytesBeforeFirstNAL()), startCodePositions[0]);

  auto     nalData = annexBFile.getNextNALUnit();
  unsigned counter = 0;
  while (nalData.data.size() > 0)
  {
    QCOMPARE(nalSizes[counter++], unsigned(nalData.data.size()));
    nalData = annexBFile.getNextNALUnit();
  }
}

QTEST_MAIN(FileSourceAnnexBTest)

#include "FilesourceAnnexBTest.moc"
