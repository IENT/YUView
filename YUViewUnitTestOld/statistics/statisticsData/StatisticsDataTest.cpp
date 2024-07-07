#include <QtTest>

#include "statistics/StatisticsData.h"

class StatisticsDataTest : public QObject
{
  Q_OBJECT

public:
  StatisticsDataTest()  = default;
  ~StatisticsDataTest() = default;

private slots:
  void testPixelValueRetrieval1();
  void testPixelValueRetrieval2();
};

void StatisticsDataTest::testPixelValueRetrieval1()
{
  stats::StatisticsData data;

  constexpr auto typeID     = 0;
  constexpr auto frameIndex = 0;

  stats::StatisticsType valueType(
      typeID, "Something", stats::color::ColorMapper({0, 10}, stats::color::PredefinedType::Jet));
  valueType.render = true;
  data.addStatType(valueType);

  QCOMPARE(data.needsLoading(frameIndex), ItemLoadingState::LoadingNeeded);
  QCOMPARE(data.getTypesThatNeedLoading(frameIndex).size(), std::size_t(1));
  QCOMPARE(data.getTypesThatNeedLoading(frameIndex).at(0), typeID);

  // Load data
  data.setFrameIndex(frameIndex);
  data[typeID].addBlockValue(8, 8, 16, 16, 7);

  const auto dataInsideRect = data.getValuesAt(QPoint(10, 12));
  QCOMPARE(dataInsideRect.size(), 1);
  QCOMPARE(dataInsideRect.at(0), QStringPair({"Something", "7"}));

  const auto dataOutside = data.getValuesAt(QPoint(0, 0));
  QCOMPARE(dataOutside.size(), 1);
  QCOMPARE(dataOutside.at(0), QStringPair({"Something", "-"}));
}

void StatisticsDataTest::testPixelValueRetrieval2()
{
  stats::StatisticsData data;

  constexpr auto typeID     = 0;
  constexpr auto frameIndex = 0;

  using VectorScaling = int;
  stats::StatisticsType valueType(typeID, "Something", VectorScaling(4));
  valueType.render = true;
  data.addStatType(valueType);

  QCOMPARE(data.needsLoading(frameIndex), ItemLoadingState::LoadingNeeded);
  QCOMPARE(data.getTypesThatNeedLoading(frameIndex).size(), std::size_t(1));
  QCOMPARE(data.getTypesThatNeedLoading(frameIndex).at(0), typeID);

  // Load data
  data.setFrameIndex(frameIndex);
  data[typeID].addBlockVector(8, 8, 16, 16, 5, 13);

  const auto dataInsideRect = data.getValuesAt(QPoint(10, 12));
  QCOMPARE(dataInsideRect.size(), 1);
  // This is wrong
  QCOMPARE(dataInsideRect.at(0), QStringPair({"Something", "(1.25,3.25)"}));

  const auto dataOutside = data.getValuesAt(QPoint(0, 0));
  QCOMPARE(dataOutside.size(), 1);
  QCOMPARE(dataOutside.at(0), QStringPair({"Something", "-"}));
}

QTEST_MAIN(StatisticsDataTest)

#include "StatisticsDataTest.moc"
