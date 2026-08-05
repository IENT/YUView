@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Compile GLSL shaders to QSB format for QRhi.
REM Usage:
REM   compile_shaders.bat
REM   set QSB=C:\Qt\6.x\msvc2022_64\bin\qsb.exe && compile_shaders.bat

cd /d "%~dp0"

set "QSB_BIN=%QSB%"
if not defined QSB_BIN (
  where qsb.exe >nul 2>&1
  if not errorlevel 1 (
    for /f "delims=" %%I in ('where qsb.exe') do (
      set "QSB_BIN=%%I"
      goto :have_qsb
    )
  )
)

if not defined QSB_BIN if exist "%QTDIR%\bin\qsb.exe" set "QSB_BIN=%QTDIR%\bin\qsb.exe"
if not defined QSB_BIN if exist "C:\Qt\6.11.0\msvc2022_64\bin\qsb.exe" set "QSB_BIN=C:\Qt\6.11.0\msvc2022_64\bin\qsb.exe"
if not defined QSB_BIN if exist "C:\Qt\6.11.1\msvc2022_64\bin\qsb.exe" set "QSB_BIN=C:\Qt\6.11.1\msvc2022_64\bin\qsb.exe"
if not defined QSB_BIN if exist "C:\Qt\Tools\QtCreator\bin\qsb.exe" set "QSB_BIN=C:\Qt\Tools\QtCreator\bin\qsb.exe"

:have_qsb
if not defined QSB_BIN (
  echo Error: qsb.exe not found.
  echo Install Qt Shader Tools, set QSB=... to qsb.exe, or add Qt bin to PATH.
  exit /b 1
)

echo Using qsb: %QSB_BIN%
echo Compiling HDR RHI shaders...

"%QSB_BIN%" --glsl "440,310 es" --hlsl 50 --msl 12 -o hdr_rhi_vertex.vert.qsb hdr_rhi_vertex.vert
if errorlevel 1 exit /b 1
echo   hdr_rhi_vertex.vert -^> hdr_rhi_vertex.vert.qsb

"%QSB_BIN%" --glsl "440,310 es" --hlsl 50 --msl 12 -o hdr_rhi_fragment.frag.qsb hdr_rhi_fragment.frag
if errorlevel 1 exit /b 1
echo   hdr_rhi_fragment.frag -^> hdr_rhi_fragment.frag.qsb

"%QSB_BIN%" --glsl "440,310 es" --hlsl 50 --msl 12 -o hdr_rhi_yuv_vertex.vert.qsb hdr_rhi_yuv_vertex.vert
if errorlevel 1 exit /b 1
echo   hdr_rhi_yuv_vertex.vert -^> hdr_rhi_yuv_vertex.vert.qsb

"%QSB_BIN%" --glsl "440,310 es" --hlsl 50 --msl 12 -o hdr_rhi_yuv_fragment.frag.qsb hdr_rhi_yuv_fragment.frag
if errorlevel 1 exit /b 1
echo   hdr_rhi_yuv_fragment.frag -^> hdr_rhi_yuv_fragment.frag.qsb

echo Done! All shaders compiled successfully.
exit /b 0
