#!/bin/bash
echo -n "#define YUVIEW_VERSION \"" > version.h
svnversion -n >> version.h
echo \" >> version.h
