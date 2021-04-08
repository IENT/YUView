#include <QtTest>

#include "common/TemporaryFile.h"
#include "statistics/StatisticsData.h"
#include "statistics/StatisticsFileVTMBMS.h"

#include <fstream>
#include <iostream>

namespace
{

struct CheckStatsItem
{
  unsigned x{}, y{}, w{}, h{};
  int      v0{}, v1{};
};

void checkVectorList(const std::vector<stats::statisticsItem_Vector> &vectors,
                     const std::vector<CheckStatsItem> &              checkItems)
{
  QCOMPARE(vectors.size(), checkItems.size());
  for (unsigned i = 0; i < vectors.size(); i++)
  {
    auto vec = vectors[i];
    auto chk = checkItems[i];
    QCOMPARE(unsigned(vec.pos[0]), chk.x);
    QCOMPARE(unsigned(vec.pos[1]), chk.y);
    QCOMPARE(unsigned(vec.size[0]), chk.w);
    QCOMPARE(unsigned(vec.size[1]), chk.h);
    QCOMPARE(vec.point[0].first, chk.v0);
    QCOMPARE(vec.point[0].second, chk.v1);
  }
}

void checkValueList(const std::vector<stats::statisticsItem_Value> &values,
                    const std::vector<CheckStatsItem> &             checkItems)
{
  QCOMPARE(values.size(), checkItems.size());
  for (unsigned i = 0; i < values.size(); i++)
  {
    auto val = values[i];
    auto chk = checkItems[i];
    QCOMPARE(unsigned(val.pos[0]), chk.x);
    QCOMPARE(unsigned(val.pos[1]), chk.y);
    QCOMPARE(unsigned(val.size[0]), chk.w);
    QCOMPARE(unsigned(val.size[1]), chk.h);
    QCOMPARE(val.value, chk.v0);
  }
}

} // namespace

class StatisticsFileVTMBMSTest : public QObject
{
  Q_OBJECT

public:
  StatisticsFileVTMBMSTest(){};
  ~StatisticsFileVTMBMSTest(){};

private slots:
  void testVTMBMSParsing();
};

void StatisticsFileVTMBMSTest::testVTMBMSParsing()
{
  TemporaryFile vtmbmsFile("vtmbmsstats");

  {
    std::ofstream o(vtmbmsFile.getFilename());
    o << "# VTMBMS Block Statistics\n";
    o << "# Sequence size: [2048x 872]\n";
    o << "# Block Statistic Type: PredMode; Integer; [0, 4]\n";
    o << "# Block Statistic Type: MVDL0; Vector; Scale: 4\n";
    o << "BlockStat: POC 0 @(   0,   0) [64x64] PredMode=1\n";
    o << "BlockStat: POC 0 @(  64,   0) [32x16] PredMode=1\n";
    o << "BlockStat: POC 0 @( 384,   0) [64x64] PredMode=2\n";
    o << "BlockStat: POC 0 @( 520,  32) [16x32] PredMode=4\n";
    o << "BlockStat: POC 0 @( 320,   0) [32x24] PredMode=1\n";
    o << "BlockStat: POC 0 @( 320,  64) [32x8] PredMode=1\n";
    o << "BlockStat: POC 2 @( 384, 128) [64x64] MVDL0={ 12,   3}\n";
    o << "BlockStat: POC 2 @( 384, 128) [64x64] PredMode=1\n";
    o << "BlockStat: POC 2 @( 448, 128) [64x64] PredMode=2\n";
    o << "BlockStat: POC 2 @( 384, 192) [64x64] MVDL0={ 52,   -44}\n";
    o << "BlockStat: POC 2 @( 480, 192) [ 8x16] MVDL0={ 520,   888}\n";
    o << "BlockStat: POC 2 @( 488, 192) [ 8x16] MVDL0={ -234, -256}\n";
    o << "BlockStat: POC 2 @( 496, 192) [16x16] PredMode=2\n";
  }

  stats::StatisticsData       statData;
  stats::StatisticsFileVTMBMS statFile(QString::fromStdString(vtmbmsFile.getFilename()), statData);

  QCOMPARE(statData.getFrameSize(), Size(2048, 872));

  auto types = statData.getStatisticsTypes();
  QCOMPARE(types.size(), size_t(2));

  QCOMPARE(types[0].typeID, 1);
  QCOMPARE(types[1].typeID, 2);
  QCOMPARE(types[0].typeName, QString("PredMode"));
  QCOMPARE(types[1].typeName, QString("MVDL0"));

  QCOMPARE(types[0].hasVectorData, false);
  QCOMPARE(types[0].hasValueData, true);
  QCOMPARE(types[0].colorMapper.rangeMin, 0);
  QCOMPARE(types[0].colorMapper.rangeMax, 4);
  QCOMPARE(types[0].colorMapper.complexType, QString("jet"));
  QCOMPARE(types[0].gridStyle.color.toHex(), std::string("#000000"));

  QCOMPARE(types[1].hasVectorData, true);
  QCOMPARE(types[1].hasValueData, false);
  auto debugName = types[1].vectorStyle.color.toHex();
  QCOMPARE(types[1].vectorStyle.color.toHex(), std::string("#ff0000"));
  QCOMPARE(types[1].vectorScale, 4);

  // We did not let the file parse the positions of the start of each poc/type yet so loading should
  // not yield any data yet.
  statFile.loadStatisticData(statData, 0, 1);
  QCOMPARE(statData.getFrameIndex(), 0);
  {
    auto &frameData = statData[9];
    QCOMPARE(frameData.vectorData.size(), size_t(0));
    QCOMPARE(frameData.valueData.size(), size_t(0));
  }

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));

  // Now we should get the data
  statFile.loadStatisticData(statData, 0, 1);
  QCOMPARE(statData.getFrameIndex(), 0);
  QCOMPARE(statData[2].valueData.size(), size_t(0));
  checkValueList(statData[1].valueData,
                 {{0, 0, 64, 64, 1},
                  {64, 0, 32, 16, 1},
                  {384, 0, 64, 64, 2},
                  {520, 32, 16, 32, 4},
                  {320, 0, 32, 24, 1},
                  {320, 64, 32, 8, 1}});
  QCOMPARE(statData[1].vectorData.size(), size_t(0));

  statFile.loadStatisticData(statData, 2, 1);
  statFile.loadStatisticData(statData, 2, 2);
  QCOMPARE(statData.getFrameIndex(), 2);
  checkValueList(statData[1].valueData,
                 {{384, 128, 64, 64, 1}, {448, 128, 64, 64, 2}, {496, 192, 16, 16, 2}});
  QCOMPARE(statData[1].vectorData.size(), size_t(0));

  checkVectorList(statData[2].vectorData,
                  {{384, 128, 64, 64, 12, 3},
                   {384, 192, 64, 64, 52, -44},
                   {480, 192, 8, 16, 520, 888},
                   {488, 192, 8, 16, -234, -256}});
  QCOMPARE(statData[2].valueData.size(), size_t(0));
}

QTEST_MAIN(StatisticsFileVTMBMSTest)

#include "StatisticsFileVTMBMSTest.moc"
