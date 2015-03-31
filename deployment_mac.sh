#!/bin/bash

if [ $# -ne 1 ]
then
  echo "Usage: `basename $0` {build directory}"
  exit 4
fi

# these paths need to be adapted to the local system
QMAKE="/Users/jaeger/Qt/5.4/clang_64/bin/qmake"
MACDEPLOY="/Users/jaeger/Qt/5.4/clang_64/bin/macdeployqt"

# make sure that we have the latest version
svn update

# find version
VERSION=$(git describe)
DIRNAME="YUView_$VERSION-Mac"
BUILD_DIR=$1
SRC_DIR=$(pwd)
PRO_FILE=$SRC_DIR/YUView.pro

# step 0: run qmake+make
$QMAKE $PRO_FILE -r -spec macx-g++ CONFIG+=release CONFIG+=x86_64

make clean -w
rm -rf $BUILD_DIR/YUView.app
make -w

# step 1: make application deployable
$MACDEPLOY $BUILD_DIR/YUView.app

# step 1.1: update Info.plist with version number
/usr/libexec/PlistBuddy -c "Set CFBundleShortVersionString '${VERSION}'" $BUILD_DIR/YUView.app/Contents/Info.plist

# step 2: copy files to temporary directory
mkdir $DIRNAME
cp -r $BUILD_DIR/YUView.app $DIRNAME/

# step 3: compress files
ditto -c -k --keepParent $DIRNAME ../$DIRNAME.zip

# step 4: clean up
rm -rf $DIRNAME/
make clean -w
rm -rf $BUILD_DIR/YUView.app