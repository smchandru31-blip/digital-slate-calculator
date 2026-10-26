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

echo =========================================================================
echo [1/2] Building and running Calculation Engine unit tests...
echo =========================================================================
"%GXX%" -static -std=c++17 -Wall -Wextra -O2 -I "%REPO_ROOT%\firmware\src" "%SCRIPT_DIR%test_calc_engine.cpp" "%REPO_ROOT%\firmware\src\CalcEngine.cpp" -o "%SCRIPT_DIR%test_calc_engine.exe"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CalcEngine compilation failed.
    exit /b %ERRORLEVEL%
)
"%SCRIPT_DIR%test_calc_engine.exe"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CalcEngine tests failed.
    exit /b %ERRORLEVEL%
)

echo.
echo =========================================================================
echo [2/2] Building and running Geometry and Trigger Line evaluation suite...
echo =========================================================================
"%GXX%" -static -std=c++17 -Wall -Wextra -O2 -I "%REPO_ROOT%\firmware\src" "%SCRIPT_DIR%test_geometry.cpp" "%REPO_ROOT%\firmware\src\GeometryEngine.cpp" -o "%SCRIPT_DIR%test_geometry.exe"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GeometryEngine compilation failed.
    exit /b %ERRORLEVEL%
)
"%SCRIPT_DIR%test_geometry.exe" "%REPO_ROOT%\data\p4_testset"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GeometryEngine evaluation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo =========================================================================
echo [3/3] Building and running Classifier Engine and Preprocessing tests...
echo =========================================================================
"%GXX%" -static -std=c++17 -Wall -Wextra -O2 -I "%REPO_ROOT%\firmware\src" "%SCRIPT_DIR%test_classifier.cpp" "%REPO_ROOT%\firmware\src\ClassifierEngine.cpp" "%REPO_ROOT%\firmware\src\StrokeStore.cpp" -o "%SCRIPT_DIR%test_classifier.exe"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] ClassifierEngine compilation failed.
    exit /b %ERRORLEVEL%
)
"%SCRIPT_DIR%test_classifier.exe" "%REPO_ROOT%\data\dataset"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] ClassifierEngine evaluation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [ALL HOST TEST SUITES PASSED SUCCESSFULLY]
exit /b 0
