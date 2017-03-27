# YUView
![YUView logo](https://raw.githubusercontent.com/IENT/YUView/master/images/IENT-YUView-256.png)

YUView is a QT based, cross-platform YUV player with an advanced analytic toolset. 

YUView can play YUV sequences in several YUV formats and supports various features like:
* simple zooming and navigation in the video
* support for a wide variety of YUV formats as well as raw RGB files, images and image sequences
* direct decoding of raw h.265/HEVC bitstreams with visualization of internals like prediction modes and motion vectors and many more
* support for opening almos any file using FFmpeg
* side-by-side and comparison view
* calculation and display of differences in the YUV space
* create and load playlists
* overlayer statistics
* ... and more

Further details of the features can be found either [here](http://ient.github.io/YUView) or 
in the [wiki](https://github.com/IENT/YUView/wiki).

Screenshot of YUView:

![YUView Main Window](https://raw.githubusercontent.com/IENT/YUView/gh-pages/images/Overview.png)

#Download

You can download precompiled binaries for Windows and MAC from our release site: [Download](https://github.com/IENT/YUView/releases). For Ubuntu and Arch Linux we provide [YUView packages](https://github.com/IENT/YUView/wiki/YUView-on-Linux). If none of these apply to you, you can easily build YUView yourself.

#Building

Compiling YUView from source is easy! We use qmake for the project so on all supported platforms you just have to install qt and run qmake to build YUView. Alternatively you can use the QTCreator if you prefer a GUI. More help on building YUView can be found in the [wiki](https://github.com/IENT/YUView/wiki/Compile-YUView).
