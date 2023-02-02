#include <QTemporaryFile>
#include <QtTest>


#include <parser/AVC/AnnexBAVC.h>

#include <optional>

class AnnexBAVCTest : public QObject
{
  Q_OBJECT

public:
  AnnexBAVCTest()  = default;
  ~AnnexBAVCTest() = default;

private slots:
  void testFile1();
};

void AnnexBAVCTest::testFile1()
{
}

QTEST_MAIN(AnnexBAVCTest)

#include "AnnexBAVCTest.moc"
