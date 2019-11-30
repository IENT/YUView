@echo on
echo Activate automatic update feature
C:\msys64\usr\bin\sed.exe -i -- "s/#define UPDATE_FEATURE_ENABLE 0/#define UPDATE_FEATURE_ENABLE 1/g" source/typedef.h
echo Setup environment
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" amd64
echo Qmake
C:\Users\travis\build\Qt\bin\qmake.exe ..
echo Start Build
nmake
