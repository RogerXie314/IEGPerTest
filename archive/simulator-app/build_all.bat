@echo off
echo ========================================
echo Building Process-Separated Architecture
echo ========================================
echo.

REM 检查VS环境
where msbuild >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MSBuild not found. Please run from VS Developer Command Prompt.
    pause
    exit /b 1
)

echo [1/3] Building C++ WLServerTest...
cd external\WLServerTest
msbuild WLServerTest.vcproj /p:Configuration=Release /p:Platform=Win32 /v:minimal
if %errorlevel% neq 0 (
    echo ERROR: C++ build failed
    cd ..\..
    pause
    exit /b 1
)
cd ..\..
echo C++ build completed: external\WLServerTest\Release\WLServerTest.exe
echo.

echo [2/3] Building C# SimulatorApp...
cd src\SimulatorApp
dotnet build -c Release /v:minimal
if %errorlevel% neq 0 (
    echo ERROR: C# build failed
    cd ..\..
    pause
    exit /b 1
)
cd ..\..
echo C# build completed: src\SimulatorApp\bin\Release\net8.0-windows\SimulatorApp.exe
echo.

echo [3/3] Copying files to deployment directory...
set DEPLOY_DIR=deploy
if not exist %DEPLOY_DIR% mkdir %DEPLOY_DIR%

copy src\SimulatorApp\bin\Release\net8.0-windows\*.exe %DEPLOY_DIR%\ >nul
copy src\SimulatorApp\bin\Release\net8.0-windows\*.dll %DEPLOY_DIR%\ >nul
copy external\WLServerTest\Release\WLServerTest.exe %DEPLOY_DIR%\ >nul
copy config.ini.example %DEPLOY_DIR%\config.ini.example >nul

echo Files copied to: %DEPLOY_DIR%\
echo.

echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Deployment directory: %DEPLOY_DIR%\
echo.
echo To run:
echo   cd %DEPLOY_DIR%
echo   SimulatorApp.exe
echo.
echo To test C++ console mode:
echo   cd %DEPLOY_DIR%
echo   WLServerTest.exe --console --config config.ini
echo.
pause
