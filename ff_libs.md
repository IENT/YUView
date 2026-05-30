# FFmpeg 版本支持矩阵

## 加载器支持的版本组合

来自 `FFmpegVersionHandler.cpp:104-110`。按优先级从新到旧尝试加载。

| avutil | avcodec | avformat | swresample | FFmpeg 分支 |
|--------|---------|----------|------------|-------------|
| 59 | 61 | 61 | 5 | FFmpeg 7.x |
| 58 | 60 | 60 | 4 | FFmpeg 6.x |
| 57 | 59 | 59 | 4 | FFmpeg 5.x |
| 56 | 58 | 58 | 3 | FFmpeg 4.x |
| 55 | 57 | 57 | 2 | FFmpeg 3.x |
| 54 | 56 | 56 | 1 | FFmpeg 2.x |

## 各 wrapper 实际覆盖范围

| 文件 | 检查库 | 覆盖版本 | 对应 FFmpeg 分支 | 未知版本行为 |
|------|--------|---------|-----------------|-------------|
| AVMotionVectorWrapper | avutil | 54, 55-59 | 2.x ~ 7.x | 构造 throw / count return 0 |
| AVFrameSideDataWrapper | avutil | 54-59 | 2.x ~ 7.x | throw |
| AVFrameWrapper | avutil | 54-59 | 2.x ~ 7.x | throw（getMetadata 断言 >=57） |
| AVPixFmtDescriptorWrapper | avutil | 54-59 | 2.x ~ 7.x | 静默跳过（无 throw） |
| AVCodecContextWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVCodecWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVPacketWrapper | avcodec | 56-61 | 2.x ~ 7.x | throw |
| AVFormatContextWrapper | avformat | 56-61 | 2.x ~ 7.x | throw |
| AVStreamWrapper | avformat | 56-61 | 2.x ~ 7.x | throw |
| AVCodecParametersWrapper | avformat | 56-61 | 2.x ~ 7.x | throw（56 设 nullptr） |
| AVInputFormatWrapper | avformat | 56-61 | 2.x ~ 7.x | 静默跳过（无 throw） |

## 关键风险点

1. **AVPixFmtDescriptorWrapper.cpp** 和 **AVInputFormatWrapper.cpp** 是仅有的两个对未知版本静默失败的文件，可能导致后续空指针访问。
