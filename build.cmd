@echo off
setlocal enabledelayedexpansion
echo Building UWUAutocorrect...

:: 1. Attempt to auto-detect MSYS2 path from the Windows Registry
set "MSYS_PATH="
for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\MSYS2 64bit" /v "InstallDir" 2^>nul') do set "MSYS_PATH=%%B"
for /f "tokens=2*" %%A in ('reg query "HKCU\SOFTWARE\MSYS2 64bit" /v "InstallDir" 2^>nul') do set "MSYS_PATH=%%B"

:: 2. Fallback to common default installation locations if registry keys are missing
if "%MSYS_PATH%"=="" (
    if exist "C:\msys64" set "MSYS_PATH=C:\msys64"
    if exist "D:\Toolchains\msys64" set "MSYS_PATH=D:\Toolchains\msys64"
)

:: 3. Verify MSYS2 was actually found
if "%MSYS_PATH%"=="" (
    echo [ERROR] MSYS2 installation not found on this system.
    echo Please install MSYS2 or add its path to the script.
    exit /b 1
)

:: Clean up trailing backslashes from path variable
if "%MSYS_PATH:~-1%"=="\" set "MSYS_PATH=%MSYS_PATH:~0,-1%"

:: 4. Create the build folder if missing
if not exist "build" mkdir "build"

:: 5. Execute the build using the dynamically discovered path
echo Found MSYS2 at: %MSYS_PATH%
"%MSYS_PATH%\msys2_shell.cmd" -ucrt64 -defterm -no-start -here -c "g++ -std=c++20 -static-libgcc -static-libstdc++ src/main.cpp src/uwuifier.cpp src/ollama_client.cpp -lwinhttp -o build/uwuifier.exe"

if %ERRORLEVEL% EQU 0 (
    echo Build successful! File saved to build/uwuifier.exe
) else (
    echo Build failed. Please check your C++ syntax.
)
