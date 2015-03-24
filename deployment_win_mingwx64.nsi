!define PRODUCT_GIT
!system 'git --git-dir=C:\Users\Max\workspace\YUView\.git describe ${PRODUCT_GIT}'

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "YUView"
!define PRODUCT_VERSION "1.0"
!define PRODUCT_PUBLISHER "IENT"
!define PRODUCT_WEB_SITE "http://www.ient.rwth-aachen.de"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\YUView.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define PRODUCT_STARTMENU_REGVAL "NSIS:StartMenuDir"

; MUI 1.67 compatible ------
!include "MUI.nsh"
; x64 header
!include "x64.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
!define MUI_LICENSEPAGE_RADIOBUTTONS
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Start menu page
var ICONS_GROUP
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "YUView"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_UNINST_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${PRODUCT_STARTMENU_REGVAL}"
!insertmacro MUI_PAGE_STARTMENU Application $ICONS_GROUP
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\YUView.exe"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
!insertmacro MUI_LANGUAGE "English"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "YUViewMinGWx64Setup${PRODUCT_GIT}.exe"
InstallDir "$PROGRAMFILES64\YUView"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "Hauptgruppe" SEC01
${If} ${RunningX64}
   DetailPrint "Installer running on 64-bit host"
   ; disable registry redirection (enable access to 64-bit portion of registry)
   SetRegView 64
   ; change install dir
   StrCpy $INSTDIR "$PROGRAMFILES64\YUView"
${EndIf}
  SetOutPath "$INSTDIR"
  SetOverwrite try
  File "D3Dcompiler_47.dll"
  SetOutPath "$INSTDIR\iconengines"
  File "iconengines\qsvgicon.dll"
  SetOutPath "$INSTDIR\imageformats"
  File "imageformats\qdds.dll"
  File "imageformats\qgif.dll"
  File "imageformats\qicns.dll"
  File "imageformats\qico.dll"
  File "imageformats\qjp2.dll"
  File "imageformats\qjpeg.dll"
  File "imageformats\qmng.dll"
  File "imageformats\qsvg.dll"
  File "imageformats\qtga.dll"
  File "imageformats\qtiff.dll"
  File "imageformats\qwbmp.dll"
  File "imageformats\qwebp.dll"
  SetOutPath "$INSTDIR"
  File "libEGL.dll"
  File "libgcc_s_sjlj-1.dll"
  File "libGLESV2.dll"
  File "libgomp-1.dll"
  File "libstdc++-6.dll"
  File "libwinpthread-1.dll"
  File "LICENSE.txt"
  File "opengl32sw.dll"
  SetOutPath "$INSTDIR\platforms"
  File "platforms\qwindows.dll"
  SetOutPath "$INSTDIR"
  File "Qt5Core.dll"
  File "Qt5Gui.dll"
  File "Qt5Svg.dll"
  File "Qt5Widgets.dll"
  File "Qt5Xml.dll"
  File "qt_ca.qm"
  File "qt_cs.qm"
  File "qt_de.qm"
  File "qt_fi.qm"
  File "qt_hu.qm"
  File "qt_it.qm"
  File "qt_ja.qm"
  File "qt_ru.qm"
  File "qt_sk.qm"
  File "qt_uk.qm"
  File "YUView.exe"

; Shortcuts
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$ICONS_GROUP"
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\YUView.lnk" "$INSTDIR\YUView.exe"
  CreateShortCut "$DESKTOP\YUView.lnk" "$INSTDIR\YUView.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section -AdditionalIcons
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateShortCut "$SMPROGRAMS\$ICONS_GROUP\Uninstall.lnk" "$INSTDIR\uninst.exe"
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\YUView.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\YUView.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) wurde erfolgreich deinstalliert."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Möchten Sie $(^Name) und alle seinen Komponenten deinstallieren?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  !insertmacro MUI_STARTMENU_GETFOLDER "Application" $ICONS_GROUP
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\YUView.exe"
  Delete "$INSTDIR\qt_uk.qm"
  Delete "$INSTDIR\qt_sk.qm"
  Delete "$INSTDIR\qt_ru.qm"
  Delete "$INSTDIR\qt_ja.qm"
  Delete "$INSTDIR\qt_it.qm"
  Delete "$INSTDIR\qt_hu.qm"
  Delete "$INSTDIR\qt_fi.qm"
  Delete "$INSTDIR\qt_de.qm"
  Delete "$INSTDIR\qt_cs.qm"
  Delete "$INSTDIR\qt_ca.qm"
  Delete "$INSTDIR\Qt5Xml.dll"
  Delete "$INSTDIR\Qt5Widgets.dll"
  Delete "$INSTDIR\Qt5Svg.dll"
  Delete "$INSTDIR\Qt5Gui.dll"
  Delete "$INSTDIR\Qt5Core.dll"
  Delete "$INSTDIR\platforms\qwindows.dll"
  Delete "$INSTDIR\opengl32sw.dll"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libgomp-1.dll"
  Delete "$INSTDIR\libGLESV2.dll"
  Delete "$INSTDIR\libgcc_s_sjlj-1.dll"
  Delete "$INSTDIR\libEGL.dll"
  Delete "$INSTDIR\imageformats\qwebp.dll"
  Delete "$INSTDIR\imageformats\qwbmp.dll"
  Delete "$INSTDIR\imageformats\qtiff.dll"
  Delete "$INSTDIR\imageformats\qtga.dll"
  Delete "$INSTDIR\imageformats\qsvg.dll"
  Delete "$INSTDIR\imageformats\qmng.dll"
  Delete "$INSTDIR\imageformats\qjpeg.dll"
  Delete "$INSTDIR\imageformats\qjp2.dll"
  Delete "$INSTDIR\imageformats\qico.dll"
  Delete "$INSTDIR\imageformats\qicns.dll"
  Delete "$INSTDIR\imageformats\qgif.dll"
  Delete "$INSTDIR\imageformats\qdds.dll"
  Delete "$INSTDIR\iconengines\qsvgicon.dll"
  Delete "$INSTDIR\D3Dcompiler_47.dll"

  Delete "$SMPROGRAMS\$ICONS_GROUP\Uninstall.lnk"
  Delete "$DESKTOP\YUView.lnk"
  Delete "$SMPROGRAMS\$ICONS_GROUP\YUView.lnk"

  RMDir "$SMPROGRAMS\$ICONS_GROUP"
  RMDir "$INSTDIR\platforms"
  RMDir "$INSTDIR\imageformats"
  RMDir "$INSTDIR\iconengines"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd