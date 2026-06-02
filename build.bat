@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat"
if not exist build mkdir build
cd build
if not exist Makefile qmake ..
jom
pause
