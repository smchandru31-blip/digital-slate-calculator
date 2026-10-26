@echo off
setlocal
set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..
set GXX=g++

where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\g++.exe" (
        set "GXX=%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\g++.exe"
    ) else (
        echo [ERROR] g++ not found in PATH or WinGet packages.
        exit /b 1
    )
)

echo Building host unit tests...
"%GXX%" -static -std=c++17 -Wall -Wextra -O2 -I "%REPO_ROOT%\firmware\src" "%SCRIPT_DIR%test_calc_engine.cpp" "%REPO_ROOT%\firmware\src\CalcEngine.cpp" -o "%SCRIPT_DIR%test_calc_engine.exe"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Compilation failed.
    exit /b %ERRORLEVEL%
)

echo Running unit tests...
"%SCRIPT_DIR%test_calc_engine.exe"
exit /b %ERRORLEVEL%
