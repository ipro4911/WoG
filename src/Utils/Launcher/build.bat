@echo off
REM Build KITFLauncher + KITFUpdater (and a small manifest builder).
REM Run this from an already-open console so you can read the output:
REM   cd C:\Users\Admin\Desktop\logs\Wyvern of Guardians\KITF-main\src\Utils\Launcher
REM   build.bat
REM Or capture it to a file:  build.bat ^> build.log 2^>^&1
setlocal EnableDelayedExpansion
REM Find vcvars64.bat: try the usual install locations (Professional,
REM Community, Enterprise, BuildTools, both drives).
set VCVARS=
for %%V in (
  "D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  "D:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) do if exist %%V set "VCVARS=%%~V"
if "%VCVARS%"=="" (echo vcvars64.bat not found. Install VS2022 C++ tools or edit VCVARS in build.bat. & pause & exit /b 1)
call "%VCVARS%" >nul
if errorlevel 1 (echo vcvars64.bat failed to initialize. & pause & exit /b 1)
if not exist bin mkdir bin
cl /nologo /W3 /O2 /EHsc /DUNICODE=0 KITFUpdater.cpp /Fe:bin\KITFUpdater.exe /link user32.lib gdi32.lib comctl32.lib wininet.lib
if errorlevel 1 (echo updater build FAILED & pause & exit /b 1)
cl /nologo /W3 /O2 /EHsc /DUNICODE=0 KITFLauncher.cpp /Fe:bin\KITFLauncher.exe /link user32.lib gdi32.lib comctl32.lib wininet.lib winmm.lib /MANIFESTUAC:"level='requireAdministrator' uiAccess='false'"
if errorlevel 1 (echo launcher build FAILED & pause & exit /b 1)
REM Embed the requireAdministrator manifest explicitly: this toolchain does
REM not embed linker manifests on its own (verified: no resource section).
REM Without this, Windows never shows the UAC prompt.
set MTEXE=mt.exe
where mt.exe >nul 2>&1
if errorlevel 1 set MTEXE=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\mt.exe
"%MTEXE%" -manifest KITFLauncher.manifest -outputresource:bin\KITFLauncher.exe;#1 -nologo
if errorlevel 1 (echo launcher manifest embed FAILED - UAC prompt will NOT appear & pause & exit /b 1)
echo [build] launcher UAC manifest embedded (requireAdministrator).
cl /nologo /W3 /O2 /EHsc /DUNICODE=0 manifest_build.cpp /Fe:bin\manifest_build.exe
if errorlevel 1 (echo manifest_build build FAILED & pause & exit /b 1)
echo.
echo Built launcher/updater in bin\.
REM Auto-stage the fresh launcher into the client folder that loginserver
REM serves (x64\Release\client), so a launcher rebuild is published without
REM a manual copy. Then run publish.bat to refresh update.txt/version.txt.
set SERVEDCLIENT=%~dp0..\..\..\x64\Release\client
if not exist "%SERVEDCLIENT%\" mkdir "%SERVEDCLIENT%" >nul 2>&1
if exist bin\KITFLauncher.exe (
  copy /y bin\KITFLauncher.exe "%SERVEDCLIENT%\" >nul
  if errorlevel 1 (echo [build] WARNING: KITFLauncher.exe is locked (close any launcher/game running from x64\Release\client^) - served copy NOT updated.) else (echo [build] staged KITFLauncher.exe into x64\Release\client)
) else (
  echo [build] WARNING: bin\KITFLauncher.exe missing, nothing staged.
)
if exist bin\KITFUpdater.exe (
  copy /y bin\KITFUpdater.exe "%SERVEDCLIENT%\" >nul
  if errorlevel 1 (echo [build] WARNING: KITFUpdater.exe is locked - served copy NOT updated.) else (echo [build] staged KITFUpdater.exe into x64\Release\client)
)
REM Staging fresh exes invalidates update.txt (size/md5 change), which shows
REM up on every client as permanent "(checksum)" failures. Rebuild the
REM manifest right here, keeping the currently published version, so the
REM served folder is ALWAYS self-consistent after a build. publish.bat is
REM still needed when the game exe / data files change.
if exist bin\manifest_build.exe (
  if exist "%SERVEDCLIENT%\version.txt" (
    set /p MBVER=<"%SERVEDCLIENT%\version.txt"
    bin\manifest_build.exe "%SERVEDCLIENT%" !MBVER!
    if errorlevel 1 (echo [build] manifest refresh FAILED & pause & exit /b 1)
    echo [build] manifest refreshed, still v!MBVER!.
  ) else (
    echo [build] WARNING: no version.txt in served client, manifest NOT refreshed.
    echo [build] Run publish.bat from the repo root once to initialize it.
  )
)
echo [build] done. Served manifest is consistent - restart loginserver.
echo [build] NOTE: run publish.bat from the repo root when ogremagix.exe or
echo [build] data files change (it stages those + rebuilds the manifest).
endlocal
