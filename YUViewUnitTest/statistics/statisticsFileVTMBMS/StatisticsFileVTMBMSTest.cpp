#include <QtTest>

#include "common/TemporaryFile.h"
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

struct CheckAffineTFItem
{
  unsigned x{}, y{}, w{}, h{};
  int      v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
};

struct CheckLineItem
{
  unsigned x{}, y{}, w{}, h{};
  int      v0{}, v1{}, v2{}, v3{};
};

struct CheckPolygonVectorItem
{
  int      v0{}, v1{};
  unsigned x[5];
  unsigned y[5];
};

void checkVectorList(const std::vector<stats::StatsItemVector> &vectors,
                     const std::vector<CheckStatsItem> &        checkItems)
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
    QCOMPARE(vec.point[0].x, chk.v0);
    QCOMPARE(vec.point[0].y, chk.v1);
  }
}

void checkAffineTFVectorList(const std::vector<stats::StatsItemAffineTF> &affineTFvectors,
                             const std::vector<CheckAffineTFItem> &       checkItems)
{
  QCOMPARE(affineTFvectors.size(), checkItems.size());
  for (unsigned i = 0; i < affineTFvectors.size(); i++)
  {
    auto vec = affineTFvectors[i];
    auto chk = checkItems[i];
    QCOMPARE(unsigned(vec.pos[0]), chk.x);
    QCOMPARE(unsigned(vec.pos[1]), chk.y);
    QCOMPARE(unsigned(vec.size[0]), chk.w);
    QCOMPARE(unsigned(vec.size[1]), chk.h);
    QCOMPARE(vec.point[0].x, chk.v0);
    QCOMPARE(vec.point[0].y, chk.v1);
    QCOMPARE(vec.point[1].x, chk.v2);
    QCOMPARE(vec.point[1].y, chk.v3);
    QCOMPARE(vec.point[2].x, chk.v4);
    QCOMPARE(vec.point[2].y, chk.v5);
  }
}

void checkLineList(const std::vector<stats::StatsItemVector> &lines,
                   const std::vector<CheckLineItem> &         checkItems)
{
  QCOMPARE(lines.size(), checkItems.size());
  for (unsigned i = 0; i < lines.size(); i++)
  {
    auto vec = lines[i];
    auto chk = checkItems[i];
    QCOMPARE(unsigned(vec.pos[0]), chk.x);
    QCOMPARE(unsigned(vec.pos[1]), chk.y);
    QCOMPARE(unsigned(vec.size[0]), chk.w);
    QCOMPARE(unsigned(vec.size[1]), chk.h);
    QCOMPARE(vec.point[0].x, chk.v0);
    QCOMPARE(vec.point[0].y, chk.v1);
    QCOMPARE(vec.point[1].x, chk.v2);
    QCOMPARE(vec.point[1].y, chk.v3);
  }
}

void checkPolygonvectorList(const std::vector<stats::StatsItemPolygonVector> &polygonList,
                            const std::vector<CheckPolygonVectorItem> &       checkItems)
{
  QCOMPARE(polygonList.size(), checkItems.size());
  for (unsigned i = 0; i < polygonList.size(); i++)
  {
    auto polygon = polygonList[i];
    auto chk     = checkItems[i];
    for (unsigned i = 0; i < polygon.corners.size(); i++)
    {
      auto corner = polygon.corners[i];
      QCOMPARE(unsigned(corner.x), chk.x[i]);
      QCOMPARE(unsigned(corner.y), chk.y[i]);
    }
    QCOMPARE(polygon.point.x, chk.v0);
    QCOMPARE(polygon.point.y, chk.v1);
  }
}

void checkValueList(const std::vector<stats::StatsItemValue> &values,
                    const std::vector<CheckStatsItem> &       checkItems)
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
    const std::string stats_str =
        R"(# VTMBMS Block Statistics
# Sequence size: [2048x 872]
# Block Statistic Type: PredMode; Integer; [0, 4]
# Block Statistic Type: MVDL0; Vector; Scale: 4
# Block Statistic Type: AffineMVL0; AffineTFVectors; Scale: 4
# Block Statistic Type: GeoPartitioning; Line;
# Block Statistic Type: GeoMVL0; VectorPolygon; Scale: 4
BlockStat: POC 0 @(   0,   0) [64x64] PredMode=1
BlockStat: POC 0 @(  64,   0) [32x16] PredMode=1
BlockStat: POC 0 @( 384,   0) [64x64] PredMode=2
BlockStat: POC 0 @( 520,  32) [16x32] PredMode=4
BlockStat: POC 0 @( 320,   0) [32x24] PredMode=1
BlockStat: POC 0 @( 320,  64) [32x8] PredMode=1
BlockStat: POC 2 @( 384, 128) [64x64] MVDL0={ 12,   3}
BlockStat: POC 2 @( 384, 128) [64x64] PredMode=1
BlockStat: POC 2 @( 448, 128) [64x64] PredMode=2
BlockStat: POC 2 @( 384, 192) [64x64] MVDL0={ 52,   -44}
BlockStat: POC 2 @( 480, 192) [ 8x16] MVDL0={ 520,   888}
BlockStat: POC 2 @( 488, 192) [ 8x16] MVDL0={ -234, -256}
BlockStat: POC 2 @( 496, 192) [16x16] PredMode=2
BlockStat: POC 8 @( 640, 128) [128x128] AffineMVL0={ -61,-128, -53,-101, -99,-139}
BlockStat: POC 8 @(1296, 128) [32x64] AffineMVL0={ -68,  32, -65,  39, -79,  25}
BlockStat: POC 8 @(1328, 128) [16x64] AffineMVL0={ -64,  40, -63,  43, -75,  33}
BlockStat: POC 8 @( 288, 544) [16x32] GeoPartitioning={   5,   0,   6,  32}
BlockStat: POC 8 @(1156, 592) [ 8x 8] GeoPartitioning={   1,   0,   8,   5}
BlockStat: POC 8 @( 276, 672) [ 8x16] GeoPartitioning={   5,   0,   3,  16}
BlockStat: POC 8 @[(240, 384)--(256, 384)--(256, 430)--(240, 416)--] GeoMVL0={ 291, 233}
BlockStat: POC 8 @[(1156, 592)--(1157, 592)--(1164, 597)--(1164, 600)--(1156, 600)--] GeoMVL0={ 152,  24}
BlockStat: POC 8 @[(544, 760)--(545, 768)--(544, 768)--] GeoMVL0={ 180,  38}
)";
    std::ofstream o(vtmbmsFile.getFilename());
    o << stats_str;
  }

  stats::StatisticsData       statData;
  stats::StatisticsFileVTMBMS statFile(QString::fromStdString(vtmbmsFile.getFilename()), statData);

  QCOMPARE(statData.getFrameSize(), Size(2048, 872));

  auto types = statData.getStatisticsTypes();
  QCOMPARE(types.size(), size_t(5));

  QCOMPARE(types[0].typeID, 1);
  QCOMPARE(types[1].typeID, 2);
  QCOMPARE(types[2].typeID, 3);
  QCOMPARE(types[3].typeID, 4);
  QCOMPARE(types[4].typeID, 5);
  QCOMPARE(types[0].typeName, QString("PredMode"));
  QCOMPARE(types[1].typeName, QString("MVDL0"));
  QCOMPARE(types[2].typeName, QString("AffineMVL0"));
  QCOMPARE(types[3].typeName, QString("GeoPartitioning"));
  QCOMPARE(types[4].typeName, QString("GeoMVL0"));

  QCOMPARE(types[0].hasVectorData, false);
  QCOMPARE(types[0].hasValueData, true);
  QCOMPARE(types[0].colorMapper.valueRange.min, 0);
  QCOMPARE(types[0].colorMapper.valueRange.max, 4);
  QCOMPARE(types[0].colorMapper.predefinedType, stats::color::PredefinedType::Jet);
  QCOMPARE(types[0].gridStyle.color.toHex(), std::string("#000000"));

  QCOMPARE(types[1].hasVectorData, true);
  QCOMPARE(types[1].hasValueData, false);
  auto debugName = types[1].vectorStyle.color.toHex();
  QCOMPARE(types[1].vectorStyle.color.toHex(), std::string("#ff0000"));
  QCOMPARE(types[1].vectorScale, 4);

  QCOMPARE(types[2].hasVectorData, false);
  QCOMPARE(types[2].hasValueData, false);
  QCOMPARE(types[2].hasAffineTFData, true);
  debugName = types[2].vectorStyle.color.toHex();
  QCOMPARE(types[2].vectorStyle.color.toHex(), std::string("#ff0000"));
  QCOMPARE(types[2].vectorScale, 4);

  QCOMPARE(types[3].hasVectorData, true);
  QCOMPARE(types[3].hasValueData, false);
  debugName = types[3].vectorStyle.color.toHex();
  QCOMPARE(types[3].vectorStyle.color.toHex(), std::string("#ffffff"));
  debugName = types[3].gridStyle.color.toHex();
  QCOMPARE(types[3].gridStyle.color.toHex(), std::string("#ffffff"));
  QCOMPARE(types[3].vectorScale, 1);

  QCOMPARE(types[4].hasVectorData, true);
  QCOMPARE(types[4].hasValueData, false);
  debugName = types[4].vectorStyle.color.toHex();
  QCOMPARE(types[4].vectorStyle.color.toHex(), std::string("#ff0000"));
  QCOMPARE(types[4].vectorScale, 4);
  QCOMPARE(types[4].isPolygon, true);

  // We did not let the file parse the positions of the start of each poc/type yet so loading should
  // not yield any data yet.
  statFile.loadStatisticData(statData, 0, 1);
  QCOMPARE(statData.getFrameIndex(), 0);
  {
    auto &frameData = statData[9];
    QCOMPARE(frameData.vectorData.size(), size_t(0));
    QCOMPARE(frameData.valueData.size(), size_t(0));
    QCOMPARE(frameData.affineTFData.size(), size_t(0));
    QCOMPARE(frameData.polygonValueData.size(), size_t(0));
    QCOMPARE(frameData.polygonVectorData.size(), size_t(0));
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

  statFile.loadStatisticData(statData, 8, 3);
  QCOMPARE(statData.getFrameIndex(), 8);
  checkAffineTFVectorList(statData[3].affineTFData,
                          {
                              {640, 128, 128, 128, -61, -128, -53, -101, -99, -139},
                              {1296, 128, 32, 64, -68, 32, -65, 39, -79, 25},
                              {1328, 128, 16, 64, -64, 40, -63, 43, -75, 33},
                          });

  statFile.loadStatisticData(statData, 8, 4);
  QCOMPARE(statData.getFrameIndex(), 8);
  checkLineList(statData[4].vectorData,
                {
                    {288, 544, 16, 32, 5, 0, 6, 32},
                    {1156, 592, 8, 8, 1, 0, 8, 5},
                    {276, 672, 8, 16, 5, 0, 3, 16},
                });

  statFile.loadStatisticData(statData, 8, 5);
  QCOMPARE(statData.getFrameIndex(), 8);
  checkPolygonvectorList(
      statData[5].polygonVectorData,
      {
          {291,
           233,
           {240, 256, 256, 240, 0},
           {384, 384, 430, 416, 0}}, // 4 pt polygon, zeros for padding test struct
          {152, 24, {1156, 1157, 1164, 1164, 1156}, {592, 592, 597, 600, 600}},
          {180,
           38,
           {544, 545, 544, 0, 0},
           {760, 768, 768, 0, 0}} // 3 pt polygon, zeros for padding test struct
      });
}

QTEST_MAIN(StatisticsFileVTMBMSTest)

#include "StatisticsFileVTMBMSTest.moc"
