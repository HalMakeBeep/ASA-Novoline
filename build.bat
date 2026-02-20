@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
)
"C:\Program Files\JetBrains\CLion 2025.3.2\bin\cmake\win\x64\bin\cmake.exe" --build C:\Users\Yeis\Desktop\PrismeASA\cmake-build-debug --target PrismeASA 2>&1
