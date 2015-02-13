#!/bin/bash

if (( $# == 1 ))
then	
	QT_DIR=/usr
else
	if (( $# == 2 ))
	then
		QT_DIR=$2 
	else
		echo "Usage: `basename $0` {build directory} {Qt directory}"
		exit 4
  fi
fi

# find version
VERSION=$(svnversion -n)
DIRNAME=YUView_$VERSION
BUILD_DIR=$1
SRC_DIR=$(pwd)
PRO_FILE=$SRC_DIR/YUView.pro 

# run qmake
cd $BUILD_DIR
$QT_DIR/bin/qmake $PRO_FILE -r -spec linux-g++-64

# run make
make clean -w
make -w

# copy files
cd $SRC_DIR
mkdir $DIRNAME
cp $BUILD_DIR/YUView $DIRNAME

# compress (tar) the directory
tar czf ../$DIRNAME.tgz $DIRNAME/

# clean up
rm -rf $DIRNAME/
svn revert version.h
cd $BUILD_DIR
make clean -w
