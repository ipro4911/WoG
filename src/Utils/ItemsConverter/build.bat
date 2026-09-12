@echo off
REM Build the private items_converter tool (no Ogre dependency).
setlocal
set VCVARS=D:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat
if not exist "%VCVARS%" (
	echo vcvars64.bat not found. Edit VCVARS in build.bat.
	exit /b 1
)
call "%VCVARS%" >nul
if not exist bin mkdir bin
cl /nologo /W3 /O2 /EHsc items_converter.cpp /Fe:bin\items_converter.exe
if errorlevel 1 (
	echo Build failed.
	exit /b 1
)
echo.
echo Built: bin\items_converter.exe
echo Usage:
echo   bin\items_converter.exe encrypt <in.cfg> <out.dat>
echo   bin\items_converter.exe decrypt <in.dat> <out.cfg>
endlocal
