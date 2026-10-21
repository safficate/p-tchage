Unicode true
Target amd64-unicode
!include "MUI2.nsh"
!include "x64.nsh"
!ifndef PRODUCT_BUILD
  !define PRODUCT_BUILD "../dist"
!endif
Name "Microtonal Pitch Map Pro 3.0.1 Engineering Preview"
OutFile "${PRODUCT_BUILD}/MicrotonalPitchMapPro-Setup-x64.exe"
InstallDir "$COMMONFILES64\VST3"
RequestExecutionLevel admin
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "../LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Turkish"
!insertmacro MUI_LANGUAGE "English"
Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Windows x64 is required."
    Abort
  ${EndIf}
FunctionEnd
Section "Windows x64 VST3" SEC01
  SetRegView 64
  SetOutPath "$INSTDIR\Microtonal Pitch Map Pro.vst3"
  File /r "${PRODUCT_BUILD}/Microtonal Pitch Map Pro.vst3/*"
  SetOutPath "$PROGRAMFILES64\Microtonal Pitch Map Pro"
  File /nonfatal "${PRODUCT_BUILD}/Microtonal Pitch Map Pro.exe"
  File "../docs/WINDOWS-KURULUM.txt"
  File "../docs/SONAR-TEST.md"
  File "../docs/TEST-PLAN-3.0.md"
  File "../docs/BUILD-3.0.md"
  File "../LICENSE"
  File /r "../licenses"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\Microtonal Pitch Map Pro"
  CreateShortcut "$SMPROGRAMS\Microtonal Pitch Map Pro\Microtonal Pitch Map Pro.lnk" "$PROGRAMFILES64\Microtonal Pitch Map Pro\Microtonal Pitch Map Pro.exe"
  WriteRegStr HKLM "Software\MicrotonalAudio\PitchMapPro" "VST3Path" "$INSTDIR\Microtonal Pitch Map Pro.vst3"
  WriteUninstaller "$PROGRAMFILES64\Microtonal Pitch Map Pro\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrotonalPitchMapPro" "DisplayName" "Microtonal Pitch Map Pro"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrotonalPitchMapPro" "UninstallString" '"$PROGRAMFILES64\Microtonal Pitch Map Pro\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrotonalPitchMapPro" "DisplayVersion" "3.0.1-preview"
SectionEnd
Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  ReadRegStr $0 HKLM "Software\MicrotonalAudio\PitchMapPro" "VST3Path"
  StrCmp $0 "" skipPlugin
  RMDir /r "$0"
  skipPlugin:
  DeleteRegKey HKLM "Software\MicrotonalAudio\PitchMapPro"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MicrotonalPitchMapPro"
  RMDir /r "$SMPROGRAMS\Microtonal Pitch Map Pro"
  RMDir /r "$PROGRAMFILES64\Microtonal Pitch Map Pro"
SectionEnd
