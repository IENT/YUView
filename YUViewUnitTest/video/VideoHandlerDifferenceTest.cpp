#include <QtTest>

#include <helper/YUVFramesProvider.h>

using namespace video::yuv;

class VideoHandlerDifferenceTest : public QObject
{
  Q_OBJECT

public:
  VideoHandlerDifferenceTest(){};
  ~VideoHandlerDifferenceTest(){};

private slots:
  void testDifferenceDrawing();
};

void VideoHandlerDifferenceTest::testDifferenceDrawing()
{
  auto frameSize = Size(16, 16);
  auto pixelFormat =
      video::yuv::PixelFormatYUV(video::yuv::Subsampling::YUV_420, 8, video::yuv::PlaneOrder::YUV);
  auto rawVideoBytes = helper::generateYUVVideo(frameSize, pixelFormat, 3);
}

QTEST_MAIN(VideoHandlerDifferenceTest)

#include "VideoHandlerDifferenceTest.moc"
