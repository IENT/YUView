@ECHO OFF
ECHO Deploying YUView for Windows
ECHO Please specify the path of your QT and MinGW installation below.
SET "QT_PATH=C:\Qt\.."
SET "MINGW_PATH=C:\Qt\..\mingw64"
IF EXIST %QT_PATH%\bin\qmake.exe (
    ECHO Running qmake...
    @ECHO OFF
    %QT_PATH%\bin\qmake.exe
) ELSE (
    ECHO qmake.exe was not found. Did you specify the correct path?
)
IF EXIST %MINGW_PATH%\bin\mingw32-make.exe (
    ECHO Compiling...
    @ECHO OFF
    %MINGW_PATH%\bin\mingw32-make.exe release
) ELSE (
    ECHO mingw32-make.exe was not found. Did you specify the correct path?
)
IF EXIST %QT_PATH%\bin\windeployqt.exe (
    ECHO Deploying...
    @ECHO OFF
    %QT_PATH%\bin\windeployqt.exe release\YUView.exe
) ELSE (
    ECHO windeployqt.exe was not found. Did you specify the correct path?
)
IF EXIST %MINGW_PATH%\bin\libgomp-1.dll (
    ECHO Copying additional DLL...
    @ECHO OFF
    copy %MINGW_PATH%\bin\libgomp-1.dll release\ /Y
) ELSE (
    ECHO libgomp-1.dll was not found in the MinGW directory.
)
IF EXIST libde265\libde265.dll (
    ECHO Copying additional DLL...
    @ECHO OFF
    copy libde265\libde265.dll release\ /Y
) ELSE (
    ECHO libde265.dll was not found in the libde265 directory.
)
IF EXIST %MINGW_PATH%\bin\mingw32-make.exe (
    ECHO Cleaning...
    @ECHO OFF
    %MINGW_PATH%\bin\mingw32-make.exe release clean
) ELSE (
    ECHO mingw32-make.exe was not found. Did you specify the correct path?
)
PAUSE