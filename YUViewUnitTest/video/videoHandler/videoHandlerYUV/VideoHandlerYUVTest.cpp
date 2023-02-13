#include <QtTest>

#include <video/videoHandlerYUV.h>

using namespace video::yuv;

class VideoHandlerYUVTest : public QObject
{
  Q_OBJECT

public:
  VideoHandlerYUVTest(){};
  ~VideoHandlerYUVTest(){};

private slots:
  void testBasicDrawing();
};

void VideoHandlerYUVTest::testBasicDrawing()
{
  auto frameSize = Size(128, 128);
  auto pixelFormat =
      video::yuv::PixelFormatYUV(video::yuv::Subsampling::YUV_420, 8, video::yuv::PlaneOrder::YUV);
  // auto rawYUVData = helper::generateYUVPlane(frameSize, pixelFormat);

  // video::yuv::videoHandlerYUV video;
  // video.setFrameSize(frameSize);
  // video.setPixelFormatYUV(pixelFormat);

  // helper::YUVFramesProvider yuvFramesProvider(&video);

  // video.loadFrame(0);
}

QTEST_MAIN(VideoHandlerYUVTest)

#include "VideoHandlerYUVTest.moc"
