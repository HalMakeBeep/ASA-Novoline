@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
echo === Starting build ===
"C:\Program Files\JetBrains\CLion 2025.3.2\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-debug --target ASA-Internal -j 16
echo === Build exit code: %ERRORLEVEL% ===
