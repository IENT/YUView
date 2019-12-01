#include <QtTest>

// add necessary includes here

class YUViewTest : public QObject
{
    Q_OBJECT

public:
    YUViewTest();
    ~YUViewTest();

private slots:
    void test_case1();

};

YUViewTest::YUViewTest()
{

}

YUViewTest::~YUViewTest()
{

}

void YUViewTest::test_case1()
{

}

QTEST_APPLESS_MAIN(YUViewTest)

#include "tst_yuviewtest.moc"
