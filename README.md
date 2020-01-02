# YUView <img align="right" src="https://raw.githubusercontent.com/IENT/YUView/master/images/IENT-YUView-256.png">

YUView is a QT based, cross-platform YUV player with an advanced analytic toolset. 

## Build Status

Master branch | Development branch
------------ | -------------
[![Build Status](https://travis-ci.org/IENT/YUView.svg?branch=master)](https://travis-ci.org/IENT/YUView) [![Codeship Status](https://app.codeship.com/projects/0527b270-5bb7-0137-0dd0-2e547607d91e/status?branch=master)](https://app.codeship.com/projects/342701) | [![Build Status](https://travis-ci.org/IENT/YUView.svg?branch=development)](https://travis-ci.org/IENT/YUView) [![Codeship Status](https://app.codeship.com/projects/0527b270-5bb7-0137-0dd0-2e547607d91e/status?branch=development)](https://app.codeship.com/projects/342701)

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

You can download precompiled binaries for Windows and MAC from the [release site](https://github.com/IENT/YUView/releases) which are all compiled on travis CI. On the release page you can find:

 - Windows installer
 - Windows zip
 - Mac OS Application
 - Linux Appimage

For the Linux based platforms we are also on [flathub](https://flathub.org/apps/details/de.rwth_aachen.ient.YUView). More information on YUView on Linux can be found in out wiki page ["YUView on Linux"](https://github.com/IENT/YUView/wiki/YUView-on-Linux). 

If none of these apply to you, you can easily [build YUView yourself](https://github.com/IENT/YUView/wiki/Compile-YUView).

## Building

Compiling YUView from source is easy! We use qmake for the project so on all supported platforms you just have to install qt and run `qmake` and `make` to build YUView. There are no further dependent libraries. Alternatively, you can use the QTCreator if you prefer a GUI. More help on building YUView can be found in the [wiki](https://github.com/IENT/YUView/wiki/Compile-YUView).
