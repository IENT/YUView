#include <QtTest>

#include <video/rgbPixelFormat.h>

class rgbPixelFormatTest : public QObject
{
  Q_OBJECT

public:
  rgbPixelFormatTest() {};
  ~rgbPixelFormatTest() {};

private slots:
  void testFormatFromToString();
};

QList<RGB_Internals::rgbPixelFormat> getAllFormats()
{
  QList<RGB_Internals::rgbPixelFormat> allFormats;

  for (int bitsPerPixel = 1; bitsPerPixel <= 16; bitsPerPixel++)
  {
    for (auto planar : {false, true})
    {
      // No alpha
      int positions[] = {0, 1, 2};
      do {
        allFormats.append(RGB_Internals::rgbPixelFormat(bitsPerPixel, planar, positions[0], positions[1], positions[2]));
      } while (std::next_permutation(positions,positions+3));

      // With alpha
      int positionsA[] = {0, 1, 2, 3};
      do {
        allFormats.append(RGB_Internals::rgbPixelFormat(bitsPerPixel, planar, positionsA[0], positionsA[1], positionsA[2], positionsA[3]));
      } while (std::next_permutation(positionsA,positionsA+4));
    }
  }

  return allFormats;
}

void rgbPixelFormatTest::testFormatFromToString()
{
  for (auto fmt : getAllFormats())
  {
    QVERIFY(fmt.isValid());
    auto name = fmt.getName();
    auto fmtNew = RGB_Internals::rgbPixelFormat(name);
    if (name.isEmpty())
    {
      QFAIL("Name empty");
    }
    if (fmt != fmtNew)
    {
      QFAIL(QString("Comparison failed. Names: %1").arg(name).toLocal8Bit().data());
    }
    if (fmt.posR != fmtNew.posR ||
        fmt.posG != fmtNew.posG ||
        fmt.posB != fmtNew.posB ||
        fmt.posA != fmtNew.posA ||
        fmt.bitsPerValue != fmtNew.bitsPerValue ||
        fmt.planar != fmtNew.planar)
    {
      QFAIL(QString("Comparison of parameters failed. Names: %1").arg(name).toLocal8Bit().data());
    }
    if (fmt.posA != -1 && (fmt.nrChannels() != 4 || !fmt.hasAlphaChannel()))
    {
      QFAIL(QString("Alpha channel indicated wrong - %1").arg(name).toLocal8Bit().data());
    }
    if (fmt.posA == -1 && (fmt.nrChannels() != 3 || fmt.hasAlphaChannel()))
    {
      QFAIL(QString("Alpha channel indicated wrong - %1").arg(name).toLocal8Bit().data());
    }
  }
}

QTEST_MAIN(rgbPixelFormatTest)

#include "rgbPixelFormatTest.moc"
