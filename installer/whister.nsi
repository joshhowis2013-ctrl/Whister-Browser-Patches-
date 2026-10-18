; Whister Browser installer (NSIS). Build with package_installer.ps1.
Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"

!ifndef STAGE
  !define STAGE "..\dist\stage"
!endif
!ifndef ICON
  !define ICON "whister.ico"
!endif
!ifndef OUTFILE
  !define OUTFILE "..\dist\Whister-Beta1-Setup.exe"
!endif
!ifndef APPVERSION
  !define APPVERSION "Beta 1"
!endif

Name "Whister Browser ${APPVERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Whister"
InstallDirRegKey HKCU "Software\Whister" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "Whister Browser"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\chrome.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Whister"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Whister Browser" SecMain
  SectionIn RO
    Delete "$INSTDIR\extensions\cjpalhdlnbpafiamejdnhcphjbkeiagm.json"
    Delete "$INSTDIR\extensions\ddkjiahejlhfcafbddmgiahcphecmpfh.json"
    Delete "$INSTDIR\default_apps\cjpalhdlnbpafiamejdnhcphjbkeiagm.json"
    Delete "$INSTDIR\default_apps\ddkjiahejlhfcafbddmgiahcphecmpfh.json"
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"

  ClearErrors
  ExecWait '"$SYSDIR\icacls.exe" "$INSTDIR" /grant "*S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-316187997-3786583170-1043257646:(OI)(CI)(RX)" /T /C' $0
  IfErrors appcontainer_acl_failed
  ${If} $0 != 0
    Goto appcontainer_acl_failed
  ${EndIf}

  ClearErrors
  ExecWait '"$SYSDIR\icacls.exe" "$INSTDIR" /grant "*S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-4240214049-4028510897-3317428798:(OI)(CI)(RX)" /T /C' $0
  IfErrors appcontainer_acl_failed
  ${If} $0 != 0
    Goto appcontainer_acl_failed
  ${EndIf}

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Whister" "InstallDir" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\Whister"
  CreateShortcut "$SMPROGRAMS\Whister\Whister.lnk" "$INSTDIR\chrome.exe"
  CreateShortcut "$SMPROGRAMS\Whister\Uninstall Whister.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\Whister.lnk" "$INSTDIR\chrome.exe"

  Goto appcontainer_acl_configured
appcontainer_acl_failed:
  MessageBox MB_ICONSTOP|MB_OK "Windows could not grant the browser sandbox access to its install files. Whister was not installed."
  Abort
appcontainer_acl_configured:

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "DisplayName" "Whister Browser ${APPVERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "DisplayVersion" "${APPVERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "Publisher" "Whister"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "DisplayIcon" "$INSTDIR\chrome.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister" "NoRepair" 1
SectionEnd

Section "Uninstall"
  Delete "$DESKTOP\Whister.lnk"
  RMDir /r "$SMPROGRAMS\Whister"
  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Whister"
  DeleteRegKey HKCU "Software\Whister"
SectionEnd
