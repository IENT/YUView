# YUView <img align="right" src="https://raw.githubusercontent.com/IENT/YUView/master/images/IENT-YUView-256.png">

YUView is a QT based, cross-platform YUV player with an advanced analytic toolset. 

## Build Status

Master branch | Development branch
------------ | -------------
[![Build Status](https://travis-ci.org/IENT/YUView.svg?branch=master)](https://travis-ci.org/IENT/YUView) [![Appveyor build status](https://ci.appveyor.com/api/projects/status/github/IENT/yuview?branch=master&svg=true)](https://ci.appveyor.com/project/ChristianFeldmann/yuview/branch/master) | [![Build Status](https://travis-ci.org/IENT/YUView.svg?branch=development)](https://travis-ci.org/IENT/YUView) [![Build Status](https://ci.appveyor.com/api/projects/status/github/IENT/yuview?branch=development&svg=true)](https://ci.appveyor.com/project/ChristianFeldmann/yuview/branch/development)

## Description

At its core, YUView is a YUV player and analysis tool. However, it can do so much more:
* simple navigation/zooming in the video
* support for a wide variety of YUV formats using various subsamplings and bit depts
* support for raw RGB files, image files and image sequences
* direct decoding of raw h.265/HEVC bitstreams with visualization of internals like prediction modes and motion vectors and many more
* interface with visualization for the reference software decoders HM and JEM
* support for opening almost any file using FFmpeg
* image comparison using side-by-side and comparison view
* calculation and display of differences (in YUV or RGB colorspace)
* save and load playlists
* overlay the video with statistics data
* ... and many more

Further details of the features can be found either [here](http://ient.github.io/YUView) or 
in the [wiki](https://github.com/IENT/YUView/wiki).

Screenshot of YUView:

![YUView Main Window](https://raw.githubusercontent.com/IENT/YUView/gh-pages/images/Overview.png)

## Download

You can download precompiled binaries for Windows and MAC from our release site: [Download](https://github.com/IENT/YUView/releases). For Ubuntu and Arch Linux we provide [YUView packages](https://github.com/IENT/YUView/wiki/YUView-on-Linux). If none of these apply to you, you can easily build YUView yourself.

## Building

Compiling YUView from source is easy! We use qmake for the project so on all supported platforms you just have to install qt and run `qmake` and `make` to build YUView. Alternatively, you can use the QTCreator if you prefer a GUI. More help on building YUView can be found in the [wiki](https://github.com/IENT/YUView/wiki/Compile-YUView).
