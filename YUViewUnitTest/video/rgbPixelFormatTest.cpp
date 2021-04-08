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
  void testInvalidFormats();
};

QList<RGB_Internals::rgbPixelFormat> getAllFormats()
{
  QList<RGB_Internals::rgbPixelFormat> allFormats;

  for (int bitsPerPixel = 8; bitsPerPixel <= 16; bitsPerPixel++)
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
    if (name.empty())
    {
      QFAIL("Name empty");
    }
    auto fmtNew = RGB_Internals::rgbPixelFormat(name);
    if (fmt != fmtNew)
    {
      auto errorStr = "Comparison failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.posR != fmtNew.posR ||
        fmt.posG != fmtNew.posG ||
        fmt.posB != fmtNew.posB ||
        fmt.posA != fmtNew.posA ||
        fmt.bitsPerValue != fmtNew.bitsPerValue ||
        fmt.planar != fmtNew.planar)
    {
      auto errorStr = "Comparison of parameters failed. Names: " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.posA != -1 && (fmt.nrChannels() != 4 || !fmt.hasAlphaChannel()))
    {
      auto errorStr = "Alpha channel indicated wrong - " + name;
      QFAIL(errorStr.c_str());
    }
    if (fmt.posA == -1 && (fmt.nrChannels() != 3 || fmt.hasAlphaChannel()))
    {
      auto errorStr = "Alpha channel indicated wrong - %1" + name;
      QFAIL(errorStr.c_str());
    }
  }
}

void rgbPixelFormatTest::testInvalidFormats()
{
  QList<RGB_Internals::rgbPixelFormat> invalidFormats;
  invalidFormats.append(RGB_Internals::rgbPixelFormat(0, false));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(1, false));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(7, false));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(17, false));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(200, false));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 0, 0));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 1, 1));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 1, 2, 2));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 1, 2, 3));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 1, 4));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 8, 0, 1));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 1, 2, 1));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 1, 2, 4));
  invalidFormats.append(RGB_Internals::rgbPixelFormat(8, false, 0, 22, 2, 3));

  for (auto fmt : invalidFormats)
    QVERIFY(!fmt.isValid());
}

QTEST_MAIN(rgbPixelFormatTest)

#include "rgbPixelFormatTest.moc"
