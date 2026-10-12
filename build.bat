@echo off
setlocal

set "ROOT=%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="

if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
)

if not defined VSROOT if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" set "VSROOT=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community"

if not defined VSROOT (
  echo Visual Studio with the Desktop development with C++ workload was not found.
  exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

set "OUT=%ROOT%build\Release"
set "OBJ=%ROOT%build\obj\Release"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OBJ%" mkdir "%OBJ%"

set "CFLAGS=/nologo /c /std:c++17 /O2 /GL /W4 /permissive- /sdl /MT /EHsc /Zi /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WINDOWS /D_USRDLL /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /I%ROOT%. /I%ROOT%src /I%ROOT%sdk"

cl.exe %CFLAGS% /Fo"%OBJ%\MapStatistics.obj" /Fd"%OBJ%\MapStatistics.pdb" "%ROOT%src\MapStatistics.cpp"
if errorlevel 1 exit /b %ERRORLEVEL%
cl.exe %CFLAGS% /Fo"%OBJ%\Overlay.obj" /Fd"%OBJ%\MapStatistics.pdb" "%ROOT%src\Overlay.cpp"
if errorlevel 1 exit /b %ERRORLEVEL%
cl.exe %CFLAGS% /Fo"%OBJ%\Settings.obj" /Fd"%OBJ%\MapStatistics.pdb" "%ROOT%src\Settings.cpp"
if errorlevel 1 exit /b %ERRORLEVEL%
cl.exe %CFLAGS% /Fo"%OBJ%\Statistics.obj" /Fd"%OBJ%\MapStatistics.pdb" "%ROOT%src\Statistics.cpp"
if errorlevel 1 exit /b %ERRORLEVEL%

link.exe /nologo /DLL /LTCG /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DEBUG /OUT:"%OUT%\MapStatistics.dll" /PDB:"%OUT%\MapStatistics.pdb" "%OBJ%\MapStatistics.obj" "%OBJ%\Overlay.obj" "%OBJ%\Settings.obj" "%OBJ%\Statistics.obj" user32.lib
exit /b %ERRORLEVEL%
