#include <QtTest>

#include "common/TemporaryFile.h"
#include "statistics/StatisticsData.h"
#include "statistics/StatisticsFileCSV.h"

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

class StatisticsFileCSVTest : public QObject
{
  Q_OBJECT

public:
  StatisticsFileCSVTest(){};
  ~StatisticsFileCSVTest(){};

private slots:
  void testCSVFileParsing1();
};

void StatisticsFileCSVTest::testCSVFileParsing1()
{
  TemporaryFile csfFile("csv");

  {
    std::ofstream o(csfFile.getFilename());
    o << "%;syntax-version;v1.2\n";
    o << "%;seq-specs;BasketballDrive_L1_1920x1080_50_encoder+randomaccess+main+B+2x_FTBE9_IBD08_"
         "IBD18_IBD08_IBD18_IP48_QPL1022_SEIDPH0_stats;0;1920;1080;0;\n";
    o << "%;type;9;MVDL0;vector;\n";
    o << "%;vectorColor;100;0;0;255\n";
    o << "%;scaleFactor;4\n";
    o << "%;type;10;MVDL1;vector;\n";
    o << "%;vectorColor;0;100;0;255\n";
    o << "%;scaleFactor;4\n";
    o << "%;type;11;MVL0;vector;\n";
    o << "%;vectorColor;200;0;0;255\n";
    o << "%;scaleFactor;4\n";
    o << "%;type;12;MVL1;vector;\n";
    o << "%;vectorColor;0;200;0;255\n";
    o << "%;scaleFactor;4\n";
    o << "%;type;7;MVPIdxL0;range;\n";
    o << "%;defaultRange;0;1;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;8;MVPIdxL1;range;\n";
    o << "%;defaultRange;0;1;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;5;MergeIdxL0;range;\n";
    o << "%;defaultRange;0;5;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;6;MergeIdxL1;range;\n";
    o << "%;defaultRange;0;5;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;0;PredMode;range;\n";
    o << "%;defaultRange;0;1;jet\n";
    o << "%;type;3;RefFrmIdxL0;range;\n";
    o << "%;defaultRange;0;3;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;4;RefFrmIdxL1;range;\n";
    o << "%;defaultRange;0;3;jet\n";
    o << "%;gridColor;255;255;255;\n";
    o << "%;type;1;Skipflag;range;\n";
    o << "%;defaultRange;0;1;jet\n";
    o << "1;0;32;8;16;9;1;0\n";
    o << "1;8;32;8;16;9;0;0\n";
    o << "1;112;56;4;8;9;0;0\n";
    o << "1;116;56;4;8;9;0;0\n";
    o << "1;128;32;32;16;9;0;0\n";
    o << "1;128;48;32;16;9;0;0\n";
    o << "7;0;32;8;16;3;1\n";
    o << "7;128;48;32;16;3;0\n";
    o << "7;384;0;64;64;3;0\n";
    o << "7;520;32;24;32;3;0\n";
    o << "7;576;40;32;24;3;0\n";
    o << "1;0;32;8;16;11;31;0\n";
    o << "1;8;32;8;16;11;-33;0\n";
    o << "1;112;56;4;8;11;-30;0\n";
    o << "1;116;56;4;8;11;-30;0\n";
    o << "1;128;32;32;16;11;-31;0\n";
    o << "1;128;48;32;16;11;-31;0\n";
    o << "1;160;32;32;16;11;-31;0\n";
  }

  stats::StatisticsData    statData;
  stats::StatisticsFileCSV statFile(QString::fromStdString(csfFile.getFilename()), statData);

  QCOMPARE(statData.getFrameSize(), QSize(1920, 1080));

  auto types = statData.getStatisticsTypes();
  QCOMPARE(types.size(), size_t(12));

  // Code on how to generate the lists:

  // QString typeIDs;
  // QString names;
  // QString description;
  // QString vectorColors;
  // QString vectorScaleFactors;
  // QString valueMins;
  // QString valueMaxs;
  // QString valueComplexTypes;
  // QString valueGridColors;

  // for (unsigned i = 0; i < 12; i++)
  // {
  //   const auto &t = types[i];
  //   typeIDs += QString(", %1").arg(t.typeID);
  //   names += QString(", %1").arg(t.typeName);
  //   description += QString(", %1").arg(t.description);

  //   vectorColors += ", ";
  //   vectorScaleFactors += ", ";
  //   if (t.hasVectorData)
  //   {
  //     vectorColors += QString::fromStdString(t.vectorStyle.color.toHex());
  //     vectorScaleFactors += QString("%1").arg(t.vectorScale);
  //   }

  //   valueMins += ", ";
  //   valueMaxs += ", ";
  //   valueComplexTypes += ", ";
  //   valueGridColors += ", ";
  //   if (t.hasValueData)
  //   {
  //     valueMins += QString("%1").arg(t.colorMapper.rangeMin);
  //     valueMaxs += QString("%1").arg(t.colorMapper.rangeMax);
  //     valueComplexTypes += t.colorMapper.complexType;
  //     valueGridColors += QString::fromStdString(t.gridStyle.color.toHex());
  //   }
  // }

  // std::cout << "typeIDs: " << typeIDs.toStdString() << "\n";
  // std::cout << "names: " << names.toStdString() << "\n";
  // std::cout << "description: " << description.toStdString() << "\n";
  // std::cout << "vectorColors: " << vectorColors.toStdString() << "\n";
  // std::cout << "vectorScaleFactors: " << vectorScaleFactors.toStdString() << "\n";
  // std::cout << "valueMins: " << valueMins.toStdString() << "\n";
  // std::cout << "valueMaxs: " << valueMaxs.toStdString() << "\n";
  // std::cout << "valueComplexTypes: " << valueComplexTypes.toStdString() << "\n";
  // std::cout << "valueGridColors: " << valueGridColors.toStdString() << "\n";

  auto typeIDs       = std::vector<int>({9, 10, 11, 12, 7, 8, 5, 6, 0, 3, 4, 1});
  auto typeNameNames = std::vector<QString>({"MVDL0",
                                             "MVDL1",
                                             "MVL0",
                                             "MVL1",
                                             "MVPIdxL0",
                                             "MVPIdxL1",
                                             "MergeIdxL0",
                                             "MergeIdxL1",
                                             "PredMode",
                                             "RefFrmIdxL0",
                                             "RefFrmIdxL1",
                                             "Skipflag"});
  auto vectorColors  = std::vector<std::string>(
      {"#640000", "#006400", "#c80000", "#00c800", "", "", "", "", "", "", "", ""});
  auto vectorScaleFactors = std::vector<int>({4, 4, 4, 4, -1, -1, -1, -1, -1, -1, -1, -1});
  auto valueColorRangeMin = std::vector<int>({-1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0});
  auto valueColorRangeMax = std::vector<int>({-1, -1, -1, -1, 1, 1, 5, 5, 1, 3, 3, 1});
  auto valueComplexTypes  = std::vector<QString>(
      {"", "", "", "", "jet", "jet", "jet", "jet", "jet", "jet", "jet", "jet"});
  auto valueGridColors = std::vector<std::string>({"",
                                                   "",
                                                   "",
                                                   "",
                                                   "#ffffff",
                                                   "#ffffff",
                                                   "#ffffff",
                                                   "#ffffff",
                                                   "#000000",
                                                   "#ffffff",
                                                   "#ffffff",
                                                   "#000000"});

  for (unsigned i = 0; i < 12; i++)
  {
    const auto &t = types[i];

    QCOMPARE(t.typeID, typeIDs[i]);
    QCOMPARE(t.typeName, typeNameNames[i]);
    if (t.hasVectorData)
    {
      QCOMPARE(t.vectorStyle.color.toHex(), vectorColors[i]);
      QCOMPARE(t.vectorScale, vectorScaleFactors[i]);
    }
    if (t.hasValueData)
    {
      QCOMPARE(t.colorMapper.rangeMin, valueColorRangeMin[i]);
      QCOMPARE(t.colorMapper.rangeMax, valueColorRangeMax[i]);
      QCOMPARE(t.colorMapper.complexType, valueComplexTypes[i]);
      QCOMPARE(t.gridStyle.color.toHex(), valueGridColors[i]);
    }
  }

  // We did not let the file parse the positions of the start of each poc/type yet so loading should
  // not yield any data yet.
  statFile.loadStatisticData(statData, 1, 9);
  QCOMPARE(statData.getFrameIndex(), 1);
  {
    auto &frameData = statData[9];
    QCOMPARE(frameData.vectorData.size(), size_t(0));
    QCOMPARE(frameData.valueData.size(), size_t(0));
  }

  std::atomic_bool breakAtomic;
  breakAtomic.store(false);
  statFile.readFrameAndTypePositionsFromFile(std::ref(breakAtomic));

  // Now we should get the data
  statFile.loadStatisticData(statData, 1, 9);
  QCOMPARE(statData.getFrameIndex(), 1);
  checkVectorList(statData[9].vectorData,
                  {{0, 32, 8, 16, 1, 0},
                   {8, 32, 8, 16, 0, 0},
                   {112, 56, 4, 8, 0, 0},
                   {116, 56, 4, 8, 0, 0},
                   {128, 32, 32, 16, 0, 0},
                   {128, 48, 32, 16, 0, 0}});
  QCOMPARE(statData[9].valueData.size(), size_t(0));

  statFile.loadStatisticData(statData, 1, 11);
  QCOMPARE(statData.getFrameIndex(), 1);
  checkVectorList(statData[11].vectorData,
                  {{0, 32, 8, 16, 31, 0},
                   {8, 32, 8, 16, -33, 0},
                   {112, 56, 4, 8, -30, 0},
                   {116, 56, 4, 8, -30, 0},
                   {128, 32, 32, 16, -31, 0},
                   {128, 48, 32, 16, -31, 0},
                   {160, 32, 32, 16, -31, 0}});
  QCOMPARE(statData[11].valueData.size(), size_t(0));

  statFile.loadStatisticData(statData, 7, 3);
  QCOMPARE(statData.getFrameIndex(), 7);
  QCOMPARE(statData[3].vectorData.size(), size_t(0));
  checkValueList(statData[3].valueData,
                 {{0, 32, 8, 16, 1},
                  {128, 48, 32, 16, 0},
                  {384, 0, 64, 64, 0},
                  {520, 32, 24, 32, 0},
                  {576, 40, 32, 24, 0}});
}

QTEST_MAIN(StatisticsFileCSVTest)

#include "StatisticsFileCSVTest.moc"
