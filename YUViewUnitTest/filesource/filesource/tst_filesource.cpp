#include <QtTest>

#include <filesource/fileSource.h>

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

QTEST_MAIN(YUViewTest)

#include "tst_filesource.moc"
