@echo off
REM ═══════════════════════════════════════════════════════
REM  Winamp for Windows — Release Build Script
REM  Requires: Visual Studio 2019/2022, Git
REM ═══════════════════════════════════════════════════════

echo ╔═══════════════════════════════════════╗
echo ║  Winamp Windows Release Build         ║
echo ╚═══════════════════════════════════════╝
echo.

REM ── Check for MSBuild ──
where msbuild >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: MSBuild not found. Run from a Visual Studio Developer Command Prompt.
    echo   Or run: "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    exit /b 1
)

REM ── Install vcpkg packages ──
if not exist vcpkg (
    echo Setting up vcpkg...
    git clone https://github.com/microsoft/vcpkg.git
    vcpkg\bootstrap-vcpkg.bat -disableMetrics
    vcpkg\vcpkg.exe integrate install
)

echo Patching ports...
xcopy /K /Y /H /C /I /E vcpkg-ports\* vcpkg\ports\*

echo Installing vcpkg packages...
cd vcpkg
vcpkg install alac:x86-windows-static-md
vcpkg install expat:x86-windows-static-md
vcpkg install freetype:x86-windows-static-md
vcpkg install ijg-libjpeg:x86-windows-static-md
vcpkg install libflac:x86-windows-static-md
vcpkg install libogg:x86-windows-static-md
vcpkg install libpng:x86-windows-static-md
vcpkg install libsndfile:x86-windows-static-md
vcpkg install libtheora:x86-windows-static-md
vcpkg install libvorbis:x86-windows-static-md
vcpkg install libvpx:x86-windows-static-md
vcpkg install minizip:x86-windows-static-md
vcpkg install mp3lame:x86-windows-static-md
vcpkg install mpg123:x86-windows-static-md
vcpkg install openssl:x86-windows-static-md
vcpkg install pthread:x86-windows-static-md
vcpkg install restclient-cpp:x86-windows-static-md
vcpkg install spdlog:x86-windows-static-md
vcpkg install zlib:x86-windows-static-md
cd ..

REM ── Extract Qt DLLs ──
echo Extracting Qt DLLs...
BuildTools\7-ZipPortable_22.01\App\7-Zip\7z.exe x Qt\DLL_5.12_x86\Release.7z.001 -y -oQt\DLL_5.12_x86

REM ── Build ──
echo Building Winamp (Release x86)...
msbuild winampAll_2019.sln /p:Configuration=Release /p:Platform=Win32 /m /v:minimal

if %errorlevel% neq 0 (
    echo BUILD FAILED
    exit /b 1
)

REM ── Package ──
echo Packaging...
set OUTDIR=winamp-0.5.0-beta1-windows-x86
if exist %OUTDIR% rmdir /S /Q %OUTDIR%
mkdir %OUTDIR%

REM Copy main exe and DLLs
copy /Y Src\Winamp\Release\*.exe %OUTDIR%\ >nul 2>nul
copy /Y Src\Winamp\Release\*.dll %OUTDIR%\ >nul 2>nul

REM Copy Qt DLLs
if exist Qt\DLL_5.12_x86\Release (
    copy /Y Qt\DLL_5.12_x86\Release\*.dll %OUTDIR%\ >nul 2>nul
)

REM Copy skins and resources
xcopy /E /I /Y skins %OUTDIR%\skins >nul
xcopy /E /I /Y Src\Winamp\resource %OUTDIR%\resource >nul

copy /Y LICENSE.md %OUTDIR%\ >nul
copy /Y README.md %OUTDIR%\ >nul

REM Create ZIP using 7zip
BuildTools\7-ZipPortable_22.01\App\7-Zip\7z.exe a -tzip "%OUTDIR%.zip" "%OUTDIR%"

echo.
echo ╔═══════════════════════════════════════╗
echo ║       Build Complete! 🎉             ║
echo ╚═══════════════════════════════════════╝
echo.
echo Output: %OUTDIR%.zip
